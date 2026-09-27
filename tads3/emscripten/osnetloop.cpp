/*
Name
  osnetloop.cpp - Emscripten in-memory loopback network transport
Function
  See osnetloop.h for the full design doc comment.

  This file buffers bytes in both directions and hands out small integer
  ids in place of real socket file descriptors; unix/osnetunix.h's
  OS_Listener/OS_Socket treat any negative-and-below-(-1) value in their
  inherited 'int s' field as one of these loopback ids rather than a real
  fd (real fds from socket()/accept() are always >= 0, and -1 already means
  "not open" throughout that file, so this can't collide with anything).
  That's a deliberate choice to avoid adding any new member fields to
  OS_CoreSocket/OS_Listener/OS_Socket at all - see this repo's
  webui-emscripten-plan.md (in the sibling htmltads repo) for why keeping
  the diff against that file small mattered here.

  Scope note on flow control: osu_loop_send() (the VM -> browser direction)
  buffers without limit rather than ever reporting OS_EWOULDBLOCK back to
  the VM. A real socket would apply backpressure once the browser stopped
  reading, but replicating that faithfully would mean the JS side telling
  this module how many bytes it has actually consumed so far, which needs
  the real Service-Worker/fetch-streaming bridge (plan step 4) to have
  something meaningful to report in the first place. A WebUI reply is
  normally at most a few hundred KB (page resources, JSON event payloads,
  small file up/downloads), so unbounded buffering is a reasonable
  simplification for now; revisit if a game ever needs to stream something
  much larger back to the browser.

  Locking: a single global mutex protects every listener and connection
  table. Traffic through here is low-volume (a handful of concurrent WebUI
  connections at most - see the plan's "small pool of keep-alive
  connections" design note), so one coarse lock is simpler and easier to
  reason about correctly than per-object locking, and isn't a measurable
  bottleneck for this workload.
Notes

Modified
  2026 - Creation (step 3 of the WebUI-over-Emscripten plan)
*/

#ifdef __EMSCRIPTEN__

#include "emscripten/osnetloop.h"

#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <emscripten/emscripten.h>

/* pulls in the real OS_Event definition - see osnetloop.h's doc comment on
 * why only this .cpp file (not osnetloop.h) needs it */
#include "unix/osnetunix.h"

/* ------------------------------------------------------------------------ */
/*
 *   Tunables
 */

/* max simultaneous loopback listeners and connections - see this file's
 * doc comment on why a small fixed bound is fine for this workload */
#define OSU_LOOP_MAX_LISTENERS  16
#define OSU_LOOP_MAX_CONNS      64

/* max not-yet-accepted connections queued per listener */
#define OSU_LOOP_MAX_PENDING    16

/* dynamic/private port range (RFC 6335) to auto-assign from when a
 * listener asks for port 0, mirroring what a real OS would hand out */
#define OSU_LOOP_DYN_PORT_LO    49152
#define OSU_LOOP_DYN_PORT_HI    65535

/* ------------------------------------------------------------------------ */
/*
 *   A simple growable byte queue.  Bytes are appended at the tail and
 *   consumed from the head; consumed space is reclaimed by shifting the
 *   remaining bytes down whenever the queue goes empty (cheap in practice,
 *   since HTTP requests/replies are small and this happens on nearly every
 *   full read/write cycle).
 */
struct osu_bytequeue
{
    unsigned char *buf;
    size_t cap;
    size_t head;  /* offset of first unconsumed byte */
    size_t len;   /* offset one past the last valid byte */

    void init() { buf = 0; cap = 0; head = 0; len = 0; }

    void clear()
    {
        if (buf != 0)
            free(buf);
        init();
    }

    /* append 'n' bytes; returns false only on allocation failure */
    bool append(const unsigned char *data, size_t n)
    {
        if (n == 0)
            return true;

        /* reclaim consumed space if we're out of room at the tail */
        if (len + n > cap && head > 0)
        {
            memmove(buf, buf + head, len - head);
            len -= head;
            head = 0;
        }

        /* grow if we still don't have room */
        if (len + n > cap)
        {
            size_t newcap = cap == 0 ? 256 : cap;
            while (newcap < len + n)
                newcap *= 2;
            unsigned char *nb = (unsigned char *)realloc(buf, newcap);
            if (nb == 0)
                return false;
            buf = nb;
            cap = newcap;
        }

        memcpy(buf + len, data, n);
        len += n;
        return true;
    }

    size_t available() const { return len - head; }

    /* copy out up to 'n' bytes, consuming them; returns the count copied */
    size_t take(unsigned char *dest, size_t n)
    {
        size_t avail = available();
        if (n > avail)
            n = avail;
        if (n > 0)
        {
            memcpy(dest, buf + head, n);
            head += n;
        }

        /* fully drained - reset so future appends don't grow unboundedly */
        if (head == len)
            head = len = 0;

        return n;
    }
};

/* ------------------------------------------------------------------------ */
/*
 *   Listener table
 */
struct osu_loop_listener
{
    bool in_use;
    int port;
    int pending[OSU_LOOP_MAX_PENDING];
    int pending_count;
    OS_Event *ready_evt;

    void reset()
    {
        in_use = false;
        port = 0;
        pending_count = 0;
        ready_evt = 0;
    }
};

/* ------------------------------------------------------------------------ */
/*
 *   Connection table
 */
struct osu_loop_conn
{
    bool in_use;

    /* browser -> VM (what OS_Socket::recv() reads) */
    osu_bytequeue recv_q;

    /* VM -> browser (what OS_Socket::send() writes, JS pulls) */
    osu_bytequeue send_q;

    OS_Event *ready_evt;

    void reset()
    {
        in_use = false;
        recv_q.init();
        send_q.init();
        ready_evt = 0;
    }
};

/* ------------------------------------------------------------------------ */
/*
 *   Global state
 */
static pthread_mutex_t g_loop_mutex = PTHREAD_MUTEX_INITIALIZER;
/*
 *   Default availability. This has to be decided at compile time, not by a
 *   JS call to osu_loop_set_available() below - a real WebUI game can open
 *   its HTTPServer (and thus call OS_Listener::open(), which consults
 *   osu_loop_available()) extremely early in its own startup, before any
 *   JS code can safely call into the wasm module at all: Emscripten
 *   asserts ("native function ... called before runtime initialization")
 *   against a native call attempted before the runtime finishes
 *   initializing, and that checkpoint is not reached until after main()
 *   has already had a chance to run some of the game's own startup code -
 *   confirmed the hard way while building the step 3 end-to-end test (see
 *   webui-emscripten-plan.md's Progress notes), where even a preRun hook
 *   (which normally runs before main(), and seemed like the earliest safe
 *   place to call osu_loop_set_available(1)) still hit that same assertion.
 *
 *   T3_COMPILING_FOR_HTML (tads3/CMakeLists.txt) is this codebase's
 *   existing, already-established way to distinguish "compiled into t3htm,
 *   i.e. for guit3" from "compiled into t3core, i.e. for t3run" within
 *   shared source files, so it's the natural switch to use here too:
 *   guit3 always wants the loopback transport under Emscripten (there's no
 *   working alternative - see osnetloop.h's doc comment), so it defaults
 *   to available; t3run/t3core default to unavailable, leaving their real
 *   socket()/PROXY_POSIX_SOCKETS/websocket-bridge path untouched exactly
 *   as before this file existed. osu_loop_set_available() below is kept
 *   as a still-functional override on top of this default, for tests or
 *   any future scenario that wants to force it either way at runtime.
 */
#ifdef T3_COMPILING_FOR_HTML
static bool g_loop_available = true;
#else
static bool g_loop_available = false;
#endif
static osu_loop_listener g_listeners[OSU_LOOP_MAX_LISTENERS];
static osu_loop_conn g_conns[OSU_LOOP_MAX_CONNS];
static bool g_tables_init = false;
static int g_next_dyn_port = OSU_LOOP_DYN_PORT_LO;

static void osu_loop_init_tables_if_needed()
{
    if (g_tables_init)
        return;
    for (int i = 0 ; i < OSU_LOOP_MAX_LISTENERS ; ++i)
        g_listeners[i].reset();
    for (int i = 0 ; i < OSU_LOOP_MAX_CONNS ; ++i)
        g_conns[i].reset();
    g_tables_init = true;
}

/* ------------------------------------------------------------------------ */
/*
 *   Availability
 */
bool osu_loop_available()
{
    return g_loop_available;
}

EMSCRIPTEN_KEEPALIVE
void osu_loop_set_available(int flag)
{
    pthread_mutex_lock(&g_loop_mutex);
    osu_loop_init_tables_if_needed();
    g_loop_available = (flag != 0);
    pthread_mutex_unlock(&g_loop_mutex);
}

/* ------------------------------------------------------------------------ */
/*
 *   Listener side
 */
int osu_loop_listen(int port_num)
{
    pthread_mutex_lock(&g_loop_mutex);
    osu_loop_init_tables_if_needed();

    /* find a free listener slot */
    int idx = -1;
    for (int i = 0 ; i < OSU_LOOP_MAX_LISTENERS ; ++i)
    {
        if (!g_listeners[i].in_use)
        {
            idx = i;
            break;
        }
    }
    if (idx < 0)
    {
        pthread_mutex_unlock(&g_loop_mutex);
        return -1;
    }

    /* pick a port: use the requested one if free, or auto-assign on 0 */
    int port = port_num;
    if (port == 0)
    {
        /* scan for a free dynamic port, wrapping if we run off the top of
         * the range (OSU_LOOP_MAX_LISTENERS is far smaller than the range,
         * so this always terminates well before wrapping all the way
         * around) */
        for (;;)
        {
            bool used = false;
            for (int i = 0 ; i < OSU_LOOP_MAX_LISTENERS ; ++i)
            {
                if (g_listeners[i].in_use && g_listeners[i].port == g_next_dyn_port)
                {
                    used = true;
                    break;
                }
            }
            int candidate = g_next_dyn_port;
            g_next_dyn_port = (g_next_dyn_port >= OSU_LOOP_DYN_PORT_HI
                              ? OSU_LOOP_DYN_PORT_LO : g_next_dyn_port + 1);
            if (!used)
            {
                port = candidate;
                break;
            }
        }
    }
    else
    {
        /* an explicit port was requested - fail if it's already in use,
         * matching a real bind()'s EADDRINUSE */
        for (int i = 0 ; i < OSU_LOOP_MAX_LISTENERS ; ++i)
        {
            if (g_listeners[i].in_use && g_listeners[i].port == port)
            {
                pthread_mutex_unlock(&g_loop_mutex);
                return -1;
            }
        }
    }

    g_listeners[idx].in_use = true;
    g_listeners[idx].port = port;
    g_listeners[idx].pending_count = 0;
    g_listeners[idx].ready_evt = 0;

    pthread_mutex_unlock(&g_loop_mutex);
    return idx;
}

int osu_loop_listener_port(int listener_id)
{
    if (listener_id < 0 || listener_id >= OSU_LOOP_MAX_LISTENERS)
        return -1;

    pthread_mutex_lock(&g_loop_mutex);
    int port = g_listeners[listener_id].in_use ? g_listeners[listener_id].port : -1;
    pthread_mutex_unlock(&g_loop_mutex);
    return port;
}

void osu_loop_listener_bind_event(int listener_id, OS_Event *ready_evt)
{
    if (listener_id < 0 || listener_id >= OSU_LOOP_MAX_LISTENERS)
        return;

    pthread_mutex_lock(&g_loop_mutex);
    if (g_listeners[listener_id].in_use)
        g_listeners[listener_id].ready_evt = ready_evt;
    pthread_mutex_unlock(&g_loop_mutex);
}

int osu_loop_accept(int listener_id, bool *out_wouldblock)
{
    *out_wouldblock = false;

    if (listener_id < 0 || listener_id >= OSU_LOOP_MAX_LISTENERS)
        return -1;

    pthread_mutex_lock(&g_loop_mutex);

    osu_loop_listener &l = g_listeners[listener_id];
    if (!l.in_use)
    {
        pthread_mutex_unlock(&g_loop_mutex);
        return -1;
    }

    if (l.pending_count == 0)
    {
        *out_wouldblock = true;
        pthread_mutex_unlock(&g_loop_mutex);
        return -1;
    }

    /* pop the oldest pending connection (FIFO) */
    int conn_id = l.pending[0];
    for (int i = 1 ; i < l.pending_count ; ++i)
        l.pending[i - 1] = l.pending[i];
    l.pending_count -= 1;

    pthread_mutex_unlock(&g_loop_mutex);
    return conn_id;
}

void osu_loop_listener_close(int listener_id)
{
    if (listener_id < 0 || listener_id >= OSU_LOOP_MAX_LISTENERS)
        return;

    pthread_mutex_lock(&g_loop_mutex);
    osu_loop_listener &l = g_listeners[listener_id];
    if (l.in_use)
    {
        /* drop any connections that arrived but were never accepted -
         * there's no OS_Socket wrapping them yet, so nothing else owns
         * their resources */
        for (int i = 0 ; i < l.pending_count ; ++i)
        {
            int cid = l.pending[i];
            if (cid >= 0 && cid < OSU_LOOP_MAX_CONNS && g_conns[cid].in_use)
                g_conns[cid].reset();
        }
        l.reset();
    }
    pthread_mutex_unlock(&g_loop_mutex);
}

/* ------------------------------------------------------------------------ */
/*
 *   Connection side
 */
void osu_loop_conn_bind_event(int conn_id, OS_Event *ready_evt)
{
    if (conn_id < 0 || conn_id >= OSU_LOOP_MAX_CONNS)
        return;

    pthread_mutex_lock(&g_loop_mutex);
    if (g_conns[conn_id].in_use)
        g_conns[conn_id].ready_evt = ready_evt;
    pthread_mutex_unlock(&g_loop_mutex);
}

int osu_loop_send(int conn_id, const char *buf, size_t len)
{
    if (conn_id < 0 || conn_id >= OSU_LOOP_MAX_CONNS)
        return -1;

    pthread_mutex_lock(&g_loop_mutex);
    osu_loop_conn &c = g_conns[conn_id];
    if (!c.in_use)
    {
        pthread_mutex_unlock(&g_loop_mutex);
        return -1;
    }

    bool ok = c.send_q.append((const unsigned char *)buf, len);
    pthread_mutex_unlock(&g_loop_mutex);

    /* on allocation failure, there's nothing sensible left to do but claim
     * success anyway - a real socket has no "out of memory" error code
     * either, and the caller has no way to retry a partial VM->browser
     * write against this transport */
    (void)ok;
    return (int)len;
}

int osu_loop_recv(int conn_id, char *buf, size_t buflen, bool *out_wouldblock)
{
    *out_wouldblock = false;

    if (conn_id < 0 || conn_id >= OSU_LOOP_MAX_CONNS)
        return 0;

    pthread_mutex_lock(&g_loop_mutex);
    osu_loop_conn &c = g_conns[conn_id];
    if (!c.in_use)
    {
        pthread_mutex_unlock(&g_loop_mutex);
        return 0;
    }

    if (c.recv_q.available() == 0)
    {
        *out_wouldblock = true;
        pthread_mutex_unlock(&g_loop_mutex);
        return -1;
    }

    size_t n = c.recv_q.take((unsigned char *)buf, buflen);
    pthread_mutex_unlock(&g_loop_mutex);
    return (int)n;
}

void osu_loop_close(int conn_id)
{
    if (conn_id < 0 || conn_id >= OSU_LOOP_MAX_CONNS)
        return;

    pthread_mutex_lock(&g_loop_mutex);
    g_conns[conn_id].reset();
    pthread_mutex_unlock(&g_loop_mutex);
}

/* ------------------------------------------------------------------------ */
/*
 *   JS-facing wrappers - see osnetloop.h's doc comment
 */

EMSCRIPTEN_KEEPALIVE
int osu_loop_new_conn(int port)
{
    pthread_mutex_lock(&g_loop_mutex);
    osu_loop_init_tables_if_needed();

    /* find the listener on this port */
    int lidx = -1;
    for (int i = 0 ; i < OSU_LOOP_MAX_LISTENERS ; ++i)
    {
        if (g_listeners[i].in_use && g_listeners[i].port == port)
        {
            lidx = i;
            break;
        }
    }
    if (lidx < 0)
    {
        pthread_mutex_unlock(&g_loop_mutex);
        return -1;
    }

    osu_loop_listener &l = g_listeners[lidx];
    if (l.pending_count >= OSU_LOOP_MAX_PENDING)
    {
        /* too many not-yet-accepted connections already queued - the
         * listener thread isn't keeping up, or JS is opening far more
         * loopback connections than this transport is sized for */
        pthread_mutex_unlock(&g_loop_mutex);
        return -1;
    }

    /* find a free connection slot */
    int cidx = -1;
    for (int i = 0 ; i < OSU_LOOP_MAX_CONNS ; ++i)
    {
        if (!g_conns[i].in_use)
        {
            cidx = i;
            break;
        }
    }
    if (cidx < 0)
    {
        pthread_mutex_unlock(&g_loop_mutex);
        return -1;
    }

    g_conns[cidx].reset();
    g_conns[cidx].in_use = true;

    l.pending[l.pending_count++] = cidx;
    OS_Event *evt = l.ready_evt;

    pthread_mutex_unlock(&g_loop_mutex);

    /* signal outside the lock - OS_Event::signal() takes its own mutex,
     * and there's no reason to hold ours while it does */
    if (evt != 0)
        evt->signal();

    return cidx;
}

EMSCRIPTEN_KEEPALIVE
int osu_loop_push(int conn_id, const unsigned char *data, int len)
{
    if (conn_id < 0 || conn_id >= OSU_LOOP_MAX_CONNS || len < 0)
        return -1;

    pthread_mutex_lock(&g_loop_mutex);
    osu_loop_conn &c = g_conns[conn_id];
    if (!c.in_use)
    {
        pthread_mutex_unlock(&g_loop_mutex);
        return -1;
    }

    bool was_empty = (c.recv_q.available() == 0);
    bool ok = c.recv_q.append(data, (size_t)len);
    OS_Event *evt = c.ready_evt;
    pthread_mutex_unlock(&g_loop_mutex);

    if (!ok)
        return -1;

    /* wake up a thread that might be blocked in OS_Event::wait() on this
     * connection's ready_evt waiting for exactly this - if it was already
     * signaled (more data arriving while some was already buffered), this
     * is a harmless redundant signal on a manual-reset event */
    if (was_empty && evt != 0)
        evt->signal();

    return 0;
}

EMSCRIPTEN_KEEPALIVE
int osu_loop_pending_len(int conn_id)
{
    if (conn_id < 0 || conn_id >= OSU_LOOP_MAX_CONNS)
        return 0;

    pthread_mutex_lock(&g_loop_mutex);
    int n = g_conns[conn_id].in_use
        ? (int)g_conns[conn_id].send_q.available() : 0;
    pthread_mutex_unlock(&g_loop_mutex);
    return n;
}

EMSCRIPTEN_KEEPALIVE
int osu_loop_pull(int conn_id, unsigned char *dest, int destLen)
{
    if (conn_id < 0 || conn_id >= OSU_LOOP_MAX_CONNS || destLen < 0)
        return 0;

    pthread_mutex_lock(&g_loop_mutex);
    osu_loop_conn &c = g_conns[conn_id];
    int n = c.in_use ? (int)c.send_q.take(dest, (size_t)destLen) : 0;
    pthread_mutex_unlock(&g_loop_mutex);
    return n;
}

EMSCRIPTEN_KEEPALIVE
void osu_loop_end_conn(int conn_id)
{
    osu_loop_close(conn_id);
}

#endif /* __EMSCRIPTEN__ */
