/*
Name
  osnetloop.h - Emscripten in-memory loopback network transport
Function
  Stands in for a real TCP loopback connection when guit3 is running under
  Emscripten, where wasm code has no way to bind()/listen()/accept() at all
  (unlike t3run's Emscripten build, which proxies real BSD socket calls out
  to an external process via a WebSocket bridge - see osnetemscipten.cpp -
  this module needs no companion process, since the "client" (the WebUI
  page's browser requests) and the "server" (this VM) are both already in
  the same browser tab).

  unix/osnetunix.h's OS_Listener and OS_Socket call into this module only
  when osu_loop_available() is true - otherwise they fall straight through
  to their normal real-socket code path, unchanged. That decision is made
  at compile time (see osnetloop.cpp's g_loop_available doc comment for
  why it can't safely be a JS-driven runtime registration, which is what
  an earlier version of this file tried): true whenever compiled into
  t3htm (i.e. for guit3, via T3_COMPILING_FOR_HTML), false otherwise. That
  keeps this entirely inert for t3run/t3core, which are never compiled
  with T3_COMPILING_FOR_HTML and so never see osu_loop_available() return
  true, so introducing this file has zero effect on that existing build.

  See htmltads/htmltads/imgui/webui-emscripten-plan.md (in the sibling
  htmltads repo) for the full design, in particular step 3's section and the
  "Progress" notes recorded there once this was verified against a real
  build.
Notes
  Scope note: the VM -> browser (send) direction buffers unboundedly rather
  than implementing real flow-control/backpressure from the JS side. A
  WebUI reply is normally at most a few hundred KB, so this is a reasonable
  simplification for now - see osnetloop.cpp's file comment for the exact
  reasoning, revisit if a game ever needs to stream something much larger
  back to the browser.
Modified
  2026 - Creation (step 3 of the WebUI-over-Emscripten plan)
*/

#ifndef OSNETLOOP_H
#define OSNETLOOP_H

#ifdef __EMSCRIPTEN__

#include <stddef.h>

/* ------------------------------------------------------------------------ */
/*
 *   C++ API - used by unix/osnetunix.h's OS_Listener/OS_Socket.  These take
 *   a real OS_Event* (forward-declared here; osnetloop.cpp includes
 *   unix/osnetunix.h to get the full definition) so that a JS-side push of
 *   new data can wake up a thread that's blocked in OS_Event::wait() on the
 *   *other* end of a connection - see osnetloop.cpp's file comment for why
 *   that's the one place this module needs to touch an event directly;
 *   everywhere else, the caller (already holding its own ready_evt/
 *   blocked_evt) signals/resets them itself, exactly like the real-socket
 *   code path already does on EWOULDBLOCK.
 */

/* is a JS-side loopback bridge registered? gates whether OS_Listener/
 * OS_Socket use this module at all - see this file's own doc comment */
bool osu_loop_available();

/* ---- listener side (OS_Listener) ---- */

/* Start listening on the given port (0 = auto-assign from the dynamic/
 * private port range, 49152-65535). Returns a listener id (>= 0), or -1 on
 * failure (e.g. the requested port is already in use by another loopback
 * listener). */
int osu_loop_listen(int port_num);

/* the port actually bound - meaningful only after a successful
 * osu_loop_listen() call, used to answer HTTPServer.getPort() */
int osu_loop_listener_port(int listener_id);

/* Register the event that should be signaled when a new connection becomes
 * available to accept. Called once, right after osu_loop_listen()
 * succeeds, from OS_CoreSocket::set_non_blocking()'s loopback branch (the
 * same point where the real-socket path launches its monitor thread -
 * this module needs no monitor thread, since it can signal readiness
 * directly at the one place state actually changes: osu_loop_new_conn()). */
void osu_loop_listener_bind_event(int listener_id, class OS_Event *ready_evt);

/* Accept the next pending connection, if any. Returns a new connection id
 * (>= 0) with *out_wouldblock left untouched, or -1 with *out_wouldblock
 * set to true if the listener has no pending connection right now (the
 * OS_EWOULDBLOCK case) or false if listener_id itself is invalid/closed
 * (a harder failure). */
int osu_loop_accept(int listener_id, bool *out_wouldblock);

/* Stop listening, and drop (without accepting) any connections that
 * arrived but were never accepted. */
void osu_loop_listener_close(int listener_id);

/* ---- connection side (OS_Socket) ---- */

/* Register the event that should be signaled when new inbound (browser ->
 * VM) data arrives for this connection - see set_non_blocking()'s loopback
 * branch, same reasoning as osu_loop_listener_bind_event() above. */
void osu_loop_conn_bind_event(int conn_id, class OS_Event *ready_evt);

/* VM -> browser direction: buffer bytes for JS to pull out later via
 * osu_loop_pending_len()/osu_loop_pull() below. Never blocks (see this
 * file's Scope note) - returns len (matching OS_Socket::send()'s
 * successful-full-write contract), or -1 if conn_id is invalid or either
 * end has already closed it. */
int osu_loop_send(int conn_id, const char *buf, size_t len);

/* browser -> VM direction: copy up to buflen buffered bytes into buf,
 * consuming them. Returns the number of bytes copied (> 0); 0 if conn_id
 * is invalid or the browser end has closed with nothing left buffered
 * (the "socket closed" case real recv() signals by returning 0); or -1
 * with *out_wouldblock = true if the connection is open but has no data
 * buffered yet - in which case the bound ready event has already been
 * reset, under the transport lock, so the caller must not reset it
 * again itself (that would race with a concurrent osu_loop_push()). */
int osu_loop_recv(int conn_id, char *buf, size_t buflen, bool *out_wouldblock);

/* Close the VM end of a connection. Like a real TCP close, this is a
 * half-close: reply bytes already sent but not yet pulled by JS stay
 * available to osu_loop_pull(), and the slot is only freed once JS has
 * closed its end too (osu_loop_end_conn()). Safe to call more than once
 * or on an id that was never opened. */
void osu_loop_close(int conn_id);

/* ------------------------------------------------------------------------ */
/*
 *   JS-facing API - plain-old-data wrappers around the above, exported so
 *   JS can drive the transport via Module.ccall()/cwrap(). Declared here,
 *   defined (and EMSCRIPTEN_KEEPALIVE'd) in osnetloop.cpp. The real
 *   Service-Worker-based bridge (plan step 4) calls these; step 3's own
 *   verification test harness (emscripten/webui-sw-spike, or a standalone
 *   page - see webui-emscripten-plan.md's Progress notes for which) calls
 *   the exact same functions directly, with no Service Worker involved, to
 *   exercise this transport in isolation.
 */
extern "C"
{
    /* Force the loopback transport on or off, overriding the compile-time
     * default (see osu_loop_available()). guit3 doesn't need to call this
     * itself - it's already available by default - this exists for tests
     * or any future scenario that wants to switch it at runtime. Calling
     * this (or any other native function below) before the Emscripten
     * runtime finishes initializing aborts with an assertion failure -
     * safe call sites are things like onRuntimeInitialized, never
     * Module.preRun. */
    void osu_loop_set_available(int flag);

    /* simulate a new inbound connection to the given port (mirrors an
     * incoming browser request arriving at that listener); returns a new
     * connection id, or -1 if nothing is listening on that port */
    int osu_loop_new_conn(int port);

    /* push bytes from JS into the VM (browser -> VM); returns 0 on
     * success, -1 if conn_id is invalid */
    int osu_loop_push(int conn_id, const unsigned char *data, int len);

    /* bytes currently buffered for JS to pull (VM -> browser); 0 if none
     * or conn_id is invalid */
    int osu_loop_pending_len(int conn_id);

    /* copy up to destLen buffered bytes out for JS, consuming them;
     * returns the number of bytes actually copied */
    int osu_loop_pull(int conn_id, unsigned char *dest, int destLen);

    /* has the VM closed its end of this connection (or is conn_id not a
     * live connection at all)? Once this returns 1 and
     * osu_loop_pending_len() returns 0, the reply is complete - this is
     * the end-of-stream signal for a reply with no Content-Length */
    int osu_loop_is_closed(int conn_id);

    /* close the browser end of a connection (the request finished, or was
     * aborted). The VM can still read anything already pushed, then sees
     * end-of-stream. JS must call this exactly once for every connection
     * osu_loop_new_conn() handed out: the slot is only freed when both
     * ends have closed (see osu_loop_close()) */
    void osu_loop_end_conn(int conn_id);
}

#endif /* __EMSCRIPTEN__ */
#endif /* OSNETLOOP_H */
