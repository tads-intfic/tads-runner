/*  Copyright (C) 1987, 1988 by Michael J. Roberts.  All Rights Reserved. */

/*
 * OS-dependent module for Unix and Termcap/Termio library.
 *
 * 29-Jan-93
 * Dave Baggett
 */

/*
 * Fri Jan 29 16:14:39 EST 1993
 * dmb@ai.mit.edu   Termcap version works.
 *
 * Thu Apr 22 01:58:07 EDT 1993
 * dmb@ai.mit.edu   Lots of little changes.  Incorporated NeXT, Linux,
 *          and DJGCC changes.
 *
 * Sat Jul 31 02:04:31 EDT 1993
 * dmb@ai.mit.edu   Many changes to improve screen update efficiency.
 *          Added unix_tc flag, set by compiler mainline,
 *          to disable termcap stuff and just run using stdio.
 *
 * Fri Oct 15 15:50:09 EDT 1993
 * dmb@ai.mit.edu   Changed punt() code to save game state.
 *
 * Mon Oct 25 16:39:57 EDT 1993
 * dmb@ai.mit.edu   Now use our own os_defext and os_remext.
 *
 */
#include <stdio.h>
#if !defined(SUN3)
#include <stddef.h>
#endif
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>          /* SRG added for usleep() and select() */
#include <fcntl.h>
#include <sys/ioctl.h>       /* tril@igs.net added for TIOCGWINSZ */
#if defined(LINUX_386) || defined(FREEBSD_386)   /* tril@igs.net 2000/Aug/20 */
#include <sys/time.h>
#include <sys/types.h>
#endif
#if defined(__EMSCRIPTEN__)
#include <limits.h>
#include <sys/select.h>
#endif

#ifdef  DJGCC_386
#include <pc.h>
#include <dos.h>
#include "biosvid.h"
#endif

#include "os.h"
#include "osgen.h"
#if !defined(USE_STDIO) && !defined(USE_HTML)
#include "osdbg.h"
#endif
#include "voc.h"

extern void safe_strcpy(char *dst, size_t dstlen, const char *src);

/*
 * ^C pressed?
 */
static int  break_set = 0;

/*
 * Run-time voc context for save-on-punt
 */
extern voccxdef *main_voc_ctx;

/*
 * Running as compiler?  If so, we shouldn't do any term handling.
 */
int unix_tc = 0;

/*
 * Private terminal handling stuff
 */
int unix_max_line;
int unix_max_column;

/*
 *   Stuff for the timing functions. Added by SRG 
 */
static long timeZero = 0;

/*
 * "Colors"
 * Note that cNORMAL *must* be zero for other parts of TADS to work.
 */
#define cNORMAL     (1 << 0)
#define cSTANDOUT   (1 << 1)
#define cREVERSE    (1 << 2)
#define cBOLD       (1 << 3)
#define cBLINK      (1 << 4)
#define cUNDERSCORE (1 << 5)

#if !defined(USE_STDIO) && !defined(USE_HTML)

#ifndef DJGCC_386
#include <signal.h>
#ifdef  USE_SGTTY
#include <sgtty.h>
#else
#ifdef  TERMIOS_IS_NOT_IN_SYS
#include <termios.h>
#else
#include <sys/termios.h>
#endif  /* TERMIOS_IS_NOT_IN_SYS */
#endif  /* USE_SGTTY */
#endif  /* DJGCC_386 */

#ifdef  HAVE_TPARM
/*
 * Use the standard library routine if we have it.
 */
#define Tparm tparm
#else
char *Tparm();
#endif

/*
 * The following flag determines whether or not the output routines
 * will attempt to use hardware features of the controlling terminal.
 * If it is NOT on, ALL screen updates will go through t_redraw.
 * This is useful for debugging problems with a termcap entry or
 * getting TADS running on a new environment quickly, but IS INCREDIBLY
 * SLOW and hence should not be used for production releases.
 */
#define T_OPTIMIZE 

/*
 * The following flag determines whether or not ossdsp will just send
 * its string out to t_puts or whether it will redraw the entire affected
 * line.  The latter is slower (perhaps painfully so over slow modem
 * connections), but fixes a problem with the way TADS currently updates
 * the status line.
 *
 * Sat Jul 31 02:06:42 EDT 1993  dmb@ai.mit.edu
 * This problem seems to be fixed now, but I'll leave the flag anyway.
 *
 */
#define FAST_OSSDSP


#define INIT 0
#define RESTORE 1

/*
 * We store an image of the screen in a private array.
 * The maximum dimensions of this array are given here.
 */
#define MAXCOLS (OS_MAXWIDTH-1)
#define MAXLINES 256

/*
 * Running as debugger?  If so, split the screen into the debugging
 * window and the execution window.
 */
int unix_tdb = 0;
int breakpoint_color;   /* for debugger */
int watchpoint_color;   /* for debugger */
int filename_color;     /* for debugger */
int usebios = 0;  /* for dbgu.c */

/*
 * Our private notion of what the screen looks like.
 * We keep one array of characters and another array of "colors."
 * Under termcap/termio handling we don't actually do color (at the
 * moment, at least), but can do character attributes like standout,
 * reverse video, blinking, etc.
 *
 * We keep track of the current cursor location and text attributes
 * so that we don't have to send redundant codes out to the terminal.
 */
static int  cw = 0;     /* current window (for debugger) */
static int  COLS, LINES;
static char COLOR = -1;
static int  cursorX = 0, cursorY = 0;
static int  inputX = 0, inputY = 0;
static char screen[MAXLINES][MAXCOLS];
static char colors[MAXLINES][MAXCOLS];

#ifdef  DJGCC_386
static int  plaincolor, revcolor, boldcolor;
#endif

/*
 * Flags indicating whether our controlling terminal has various
 * capabilities.
 */
static int  standout_ok, rev_ok, bold_ok, blink_ok, underscore_ok;
static int  hideshow_ok;

static int  k_erase, k_kill, k_word_erase, k_reprint;

/*
 * Terminal capabilities from termcap.
 * Certainly more than you ever wanted to know about.
 */

/*
 * Booleans
 */
static int
    Txs,    /* Standout not erased by overwriting (Hewlett-Packard) */
    Tms,    /* Safe to move in standout modes */
    Tos;    /* Terminal overstrikes on hard-copy terminal */

/*
 * Numbers
 */
static int
    Tsg;    /* Number for characters left by standout */
/*
 * Strings
 */
static char
    *Tcs,   /* Change scroll region to lines #1 through #2 (VT100) */
    *TcS,   /* Change scroll region: #1 = total lines, #2 =  lines
           above region, #3 = lines below region, #4 = total lines */
    *Tcl,   /* Clear screen and home cursor */
    *Tce,   /* Clear to end of line */
    *Tcd,   /* Clear to end of display */
    *Tcm,   /* Cursor motion to row #1 col #2 */
    *Tdo,   /* Down one line */
    *Tho,   /* Home cursor */
    *Tvi,   /* Make cursor invisible */
    *Tle,   /* Move cursor left one SPACE */
    *Tve,   /* Make cursor appear normal (undo cvvis/civis) */
    *Tnd,   /* Non-destructive space (cursor right) */
    *Tup,   /* Upline (cursor up) */
    *Tvs,   /* Make cursor very visible */
    *Tdl,   /* Delete line */
    *Tmb,   /* Turn on blinking */
    *Tmd,   /* Turn on bold (extra bright) mode */
    *Tme,   /* Turn off attributes (blinking, reverse, bold, etc.) */
    *Tti,   /* String to begin programs that use cursor addresing */
    *Tmr,   /* Turn on reverse video mode */
    *Tso,   /* Begin standout mode */
    *Tus,   /* Start underscore mode */
    *Tte,   /* String to end programs that use cursor addressing */
    *Tse,   /* End standout mode */
    *Tue,   /* End underscore mode */
    *Tal,   /* Add new blank line (insert line) */
    *Tpc,   /* Pad character (rather than null) */
    *TDL,   /* Delete #1 lines */
    *TDO,   /* Move cursor down #1 lines. */
    *TSF,   /* Scroll forward #1 lines. */
    *TAL,   /* Add #1 new blank lines */
    *TLE,   /* Move cursor left #1 spaces */
    *TRI,   /* Move cursor right #1 spaces. */
    *TSR,   /* Scroll backward #1 lines. */
    *TUP,   /* Move cursor up #1 lines. */
    *Tsf,   /* Scroll forward one line. */
    *Tsr,   /* Scroll backward one line. */
    *Twi;   /* Current window is lines #1-#2 cols #3-#4 */

#ifndef DJGCC_386
/*
 * We must import the following three variables from the termcap
 * library and set the appropriately so that output padding will
 * work properly.
 */
extern char PC;
extern short ospeed;
#endif  /* DJGCC_386 */

static void punt(int sig);
static void susp(int sig);
static void cont(int sig);
static void get_screen_size(int *w, int *h);
static void resize(void);
static void t_init(void);
static void t_tty(int action);
static void winch(int sig);
static void set_win_size(void);
static void t_color_init(void);
static void t_term(void);
static void t_hide(void);
static void t_show(void);
static void t_putc(char c);
static void t_outs(char *s);
static void t_puts(char *s);
static void t_refresh(void);
static void t_update(int x, int y);
static void t_loc(int y, int x, int force);
static void t_color(char c);
static void t_redraw(int top, int bottom);
static void t_scroll(int top, int bottom, int lines);
static void t_set_scroll_region(int top, int bot);
static void break_handler(int sig);

void ossclr(int top, int left, int bottom, int right, int blank_color);
void ossdbgloc(int y, int x);

/*
 * Set up our controlling terminal and find out what it's capable
 * of doing.  Punt if we have no ability to position the cursor,
 * scroll, etc.
 */
static void
t_init(void)
{
#ifdef  DJGCC_386
    
    bios_video_init();
    rev_ok = 1;
    bold_ok = 1;
    standout_ok = 0;
    blink_ok = 0;
    underscore_ok = 0;
    hideshow_ok = 0;
    bios_video_get_screen_dimensions(&COLS, &LINES);
    LINES++; COLS++; set_win_size(); LINES--; COLS--;
    t_color_init();
    
#else   /* DJGCC_386*/
    
#define Tgetstr(key) (tgetstr(key, &tbufptr))
    extern char *tgetstr();

    static char tbuf[4096];
    register char   *term;
    register char   *tptr;
    char        *tbufptr;
    int     scroll_ok, curs_ok;

    if (unix_tc)
        return;

    /*
     * Set tty up the way we need it.
     */
    t_tty(INIT);

    /*
     * Set all string capabilities to NULL for safety's sake.
     */
    Tcs = TcS = Tcl = Tce = Tcd = Tcm = Tdo = Tho = Tvi = Tle = Tve =
    Tnd = Tup = Tvs = Tdl = Tme = Tmb = Tmd = Tti = Tmr = Tso = Tus =
    Tte = Tse = Tue = Tal = Tpc = TDL = TDO = TSF = TAL = TLE = TRI =
    TSR = TUP = Twi = NULL;

    /*
     * Find the name of the user's terminal.
     */
    term = getenv("TERM");
    if(!term) {
        printf("Can't find TERM, the name of your terminal.\n");
        exit(-1);
    }

    /*
     * Allocate a buffer for the termcap entry.  Make it big to be safe.
     */
    tptr = (char *) malloc(4096);
    if (!tptr) {
        printf("Out of memory allocating termcap entry buffer.\n");
        exit(-1);
    }

    /*
     * Get the termcap entry.
     */
    tbufptr = tbuf;
    if(tgetent(tptr, term) < 1) {
        printf("Unknown terminal type: %s", term);
        exit(-1);
    }

    /*
     * Check for fatal conditions:
     * 
     *  - Tos (terminal overstrikes)
     *  - Does not have any of:
     *      - Add line and delete line (AL/DL, al/dl)
     *      - Set scrolling region (cs, cS) *and* scroll up and
     *        down (sf and sr).
     *      - Set window (wi) *and* scroll up and scroll down
     *        (sf and sr).
     *  - Does not have cursor addressing *or* home *and* down and right
     */
    if (tgetflag("os")) {
        printf("Can't run on a hard-copy terminal (termcap os).\n");
        exit(-1);
    }
    TAL = Tgetstr("AL");
    TDL = Tgetstr("DL");
    Tal = Tgetstr("al");
    Tdl = Tgetstr("dl");
    Tcs = Tgetstr("cs");
    TcS = Tgetstr("cS");
    Twi = Tgetstr("wi");
    Tsf = Tgetstr("sf");
    Tsr = Tgetstr("sr");
    TSF = Tgetstr("SF");
    TSR = Tgetstr("SR");

    if (!Tsf)
        Tsf = "\n";

    scroll_ok = 0;
    if ((Tal || TAL) && (Tdl || TDL))
        scroll_ok = 1;
    else if ((Tcs || TcS || Twi) && (Tsf || TSF) && (Tsr || TSR))
        scroll_ok = 1;
    if (!scroll_ok) {
        printf("Can't run without window scrolling capability.\n");
        exit(-1);
    }
    Tcm = Tgetstr("cm");
    Tho = Tgetstr("ho");
    Tle = Tgetstr("le");
    if (!Tle)
        Tle = Tgetstr("bs");    /* obsolete alternative to "le" */
    TLE = Tgetstr("LE");
    Tnd = Tgetstr("nd");
    TRI = Tgetstr("RI");
    Tup = Tgetstr("up");
    TUP = Tgetstr("UP");
    Tdo = Tgetstr("do");
    if (!Tdo)
        Tdo = Tgetstr("nl");    /* obsolete alternative to "do" */
    TDO = Tgetstr("DO");
    Tti = Tgetstr("ti");
    Tte = Tgetstr("te");
    curs_ok = 0;
    if (Tcm)
        curs_ok = 1;
    else if (Tho) {
        if ((Tnd || TRI) && (Tdo || TDO))
            curs_ok = 1;
    }
    if (!curs_ok) {
        printf("Can't run without cursor positioning capability.\n");
        exit(-1);
    }

    /*
     * Get region clearing capabilities, if any.
     */
    Tcl = Tgetstr("cl");
    Tce = Tgetstr("ce");
    Tcd = Tgetstr("cd");

    /*
     * See if we have standout mode.
     * Don't use it if term has standout erase bug (xs) or it's not
     * safe to move in standout mode (ms), or standout generates spaces
     * (sg > 0).
     */
    standout_ok = 0;
    if (!tgetflag("xs") && !tgetflag("ms") && tgetnum("sg") <= 0) {
        Tso = Tgetstr("so");
        Tse = Tgetstr("se");
        if (Tso && Tse)
            standout_ok = 1;
    }

    /*
     * See if we can turn off special modes
     */
    Tme = Tgetstr("me");

    /*
     * See if we have reverse video mode.
     */
    rev_ok = 0;
    if (Tme) {
        Tmr = Tgetstr("mr");
        if (Tmr)
            rev_ok = 1;;
    }

    /*
     * See if we have bold mode.
     */
    bold_ok = 0;
    if (Tme) {
        Tmd = Tgetstr("md");
        if (Tmd)
            bold_ok = 1;
    }

    /*
     * See if we have blink mode.
     */
    blink_ok = 0;
    if (Tme) {
        Tmb = Tgetstr("mb");
        if (Tmb)
            blink_ok = 1;
    }

    /*
     * See if we have underscore mode.
     */
    underscore_ok = 0;
    Tus = Tgetstr("us");
    Tue = Tgetstr("ue");
    if (Tus && Tue)
        underscore_ok = 1;

    /*
     * See if we can hide/show the cursor.
     */
    hideshow_ok = 0;
    Tvi = Tgetstr("vi");    
    Tve = Tgetstr("ve");
    Tvs = Tgetstr("vs");    
    if (Tvi && Tve)
        hideshow_ok = 1;

    /*
     * Get padding character (if there is one for this term).
     */
    Tpc = Tgetstr("pc");
    if (Tpc)
        PC = *Tpc;
    else
        PC = 0;

    free(tptr);

    /*
     * Do the right thing when window's resized.
     */
    signal(SIGWINCH, winch);

    /*
     * Handle ^C.
     */
    signal(SIGINT, break_handler);
    /* signal(SIGINT, SIG_IGN); */

    /* 
     * Handle ^Z.
     */
    signal(SIGTSTP, susp);
    signal(SIGCONT, cont);

    /*
     * Restore terminal sanity when signals nuke us.
     */
    signal(SIGHUP, punt);
    signal(SIGQUIT, punt);
    signal(SIGILL, punt);
    signal(SIGTRAP, punt);
    signal(SIGABRT, punt);
#if !defined(LINUX_386)
    signal(SIGEMT, punt);
#endif
    signal(SIGFPE, punt);
#if !defined(LINUX_386)
    signal(SIGBUS, punt);
#endif
    signal(SIGSEGV, punt);
#if !defined(LINUX_386)
    signal(SIGSYS, punt);
#endif

    /*
     * Determine screen size.  Ensure sanity of text formatter.
     */
    resize();

    /*
     * Init "colors"
     */
    t_color_init();

    /*
     * Init screen
     */
    t_puts(Tti);
    t_puts(Tvs);
    
#endif  /* DJGCC_386 */ 
}

/*
 * Clean up when we get a request to suspend from the terminal.
 */
static void
susp(int sig)
{
#ifndef DJGCC_386   
    t_term();
    signal(SIGTSTP, SIG_DFL);
    kill(getpid(), SIGTSTP);
#endif  
}

/*
 * Restore term state upon resuming from suspension.
 */
static void
cont(int sig)
{
#ifndef DJGCC_386
    t_init();
    t_update(-1, -1);
    COLOR = -1;
    t_refresh();
#endif  
}

/*
 * Die gracefully when we get a fatal signal.
 * We try to save the game before we exit.
 */
static void
punt(int sig)
{
    char s[15];

    if (!unix_tc && main_voc_ctx != 0) {
        sprintf(s, "fatal%d.sav", getpid());
        printf("Caught a fatal signal!  Attempting to save game in %s...\n", s);
        fiosav(main_voc_ctx, s, 0);
    }
    else {
        printf("Caught a fatal signal!\n");
    }

    os_term(-1);
}

/*
 * Get terminal size.
 * Negative and zero values mean the system has no clue.
 */
static void
get_screen_size(int *w, int *h)
{
#ifndef DJGCC_386
    /*
     * Define the 4.3 names in terms of the Sun names
     * if the latter exist and the former do not.
     */
#ifdef TIOCGSIZE
#ifndef TIOCGWINSZ
#define TIOCGWINSZ TIOCGSIZE
#define winsize ttysize
#define ws_row ts_lines
#define ws_col ts_cols
#endif
#endif

    /*
     * Use the 4.3 names if possible.
     */
#ifdef TIOCGWINSZ
    struct winsize size;
    *w = 0;
    *h = 0;
    if (ioctl(0, TIOCGWINSZ, &size) < 0)
        return;

    *w = size.ws_col;
    *h = size.ws_row;

#else /* not TIOCGWNSIZ */

    /*
     * System has no clue
     */
    *w = 0;
    *h = 0;

#endif /* not TIOCGWINSZ */

    /*
     * If running as debugger, halve the height
     * since we want two windows.
     */
    if (unix_tdb)
        *h /= 2;
    
#endif  /* DJGCC_386 */ 
}

static void
resize(void)
{
#ifndef DJGCC_386   
    get_screen_size(&COLS, &LINES);

    if (COLS <= 0 || LINES <= 0) {
        COLS = tgetnum("co");
        LINES = tgetnum("li");
    }

    set_win_size();
#endif  
}

static void
winch(int sig)
{
#ifndef DJGCC_386   
    resize();

    /*
     * Clear screen
     */
    ossclr(0, 0, LINES - 1, COLS - 1, cNORMAL);
    t_refresh();
#endif  
}

/*
 * When window size changes, we have to notify the rest of the system
 * so text will be formatted properly.
 */
static void
set_win_size(void)
{
    /*
     * Ensure output formatter happiness.
     */
    if (LINES < 5)
        LINES = 5;
    if (COLS < 20)
        COLS = 20;

    /*
     * Ensure our own personal happiness.
     */
    if (LINES >= MAXLINES)
        LINES = MAXLINES - 1;
    if (COLS >= MAXCOLS)
        COLS = MAXCOLS - 1;

    G_oss_screen_height = LINES - 1;
    G_oss_screen_width = COLS;
    unix_max_line = LINES - 2;
    unix_max_column = COLS - 2;
    osssb_on_resize_screen();
}

/*
 * Tell the rest of the system what "colors" to use for various things.
 */
static void
t_color_init(void)
{
    if (rev_ok)
        sdesc_color = cREVERSE;
    else if (standout_ok)
        sdesc_color = cSTANDOUT;
    else if (bold_ok)
        sdesc_color = cBOLD;
    else if (underscore_ok)
        sdesc_color = cUNDERSCORE;
    else
        sdesc_color = cNORMAL;

    ldesc_color = sdesc_color;
    debug_color = sdesc_color;

    text_color  = cNORMAL;
    if (bold_ok)
        text_bold_color = cBOLD;
    else if (standout_ok)
        text_bold_color = cSTANDOUT;
    else if (rev_ok)
        text_bold_color = cREVERSE;
    else if (underscore_ok)
        text_bold_color = cUNDERSCORE;
    else
        text_bold_color = cNORMAL;

    text_normal_color = text_color;
    breakpoint_color = sdesc_color;
    watchpoint_color = sdesc_color;

    if (rev_ok)
        filename_color = cREVERSE;
    else if (underscore_ok)
        filename_color = cUNDERSCORE;
    else if (bold_ok)
        filename_color = cBOLD;
    else
        filename_color = cNORMAL;
}

static void
t_term(void)
{
#ifndef DJGCC_386
    
    if (unix_tc)
        return;

    /*
     * Restore scroll window if term has that capability.
     */
    if (Tcs || TcS || Twi)
        if (unix_tdb)
            t_set_scroll_region(0, LINES*2 - 1);
        else
            t_set_scroll_region(0, LINES - 1);

    /*
     * Disable all attributes (if possible), including standout.
     */
    t_puts(Tme);
    t_puts(Tse);
    t_puts(Tue);

    /*
     * Restore tty
     */
    t_tty(RESTORE);

    /*
     * End visual mode
     */
    t_puts(Tte);

    /*
     * Make cursor visible
     */
    t_puts(Tve);
    
#endif  /* DJGCC_386 */ 
}

/*
 * Hide cursor
 */
static void
t_hide(void)
{
#ifdef  DJGCC_386
    void bios_video_cursor_hide();
#else   /* DJGCC_386 */ 
    
    if (hideshow_ok)
        t_puts(Tvi);
    
#endif  /* DJGCC_386 */ 
}

/*
 * Show cursor
 */
static void
t_show(void)
{
#ifdef  DJGCC_386
    void bios_video_cursor_hide();
#else   /* DJGCC_386 */ 
    
    if (hideshow_ok)
        t_puts(Tve);
    
#endif  /* DJGCC_386 */ 
}

/*
 * Write a character to the screen (without padding)
 */
static void
t_putc(char c)
{
#ifndef DJGCC_386   
    fputc(c, stdout);
#endif  
}

/*
 * Write a string to the screen (without padding)
 */
static void
t_outs(char *s)
{
#ifdef  DJGCC_386   
    
    bios_video_write_string(s);
    
#else   /* DJGCC_386 */ 
    if (s) {
        printf("%s", s);
        fflush(stdout);
    }
#endif  
}

/*
 * Write an escape sequence to the string to the screen with padding
 */
static void
t_puts(char *s)
{
#ifndef DJGCC_386   
    
#if 0
    if (s)
        printf("%s", s);
    return;
#endif

    if (s) {
        Tputs(s, 1, (int (*)())t_putc);
        fflush(stdout);
    }
#endif  
}

/*
 * Init or restore terminal.
 */
static void
t_tty(int action)
{
#ifndef DJGCC_386
    
#ifdef  USE_SGTTY
    struct sgttyb       t;
    struct ltchars      t1;
    static struct sgttyb    orig;
#else   /* USE_SGTTY */
    struct termios      t;
    static struct termios   orig;
#endif  /* USE_SGTTY */
    
    if (action == RESTORE) {

#ifdef  USE_IOCTL_INSTEAD_OF_TERMIOS
#ifdef  USE_SGTTY
        ioctl(0, TIOCSETP, &orig);
#else
        ioctl(0, TCSETSW, &orig);
#endif /* USE_SGTTY */
#else
        tcsetattr(0, TCSANOW, &orig);
#endif
    }
    else {

#ifdef  USE_IOCTL_INSTEAD_OF_TERMIOS
#ifdef  USE_SGTTY
        ioctl(0, TIOCGETP, &orig);
        ioctl(0, TIOCGETP, &t);
#else
        ioctl(0, TCGETS, &orig);
        ioctl(0, TCGETS, &t);
#endif /* USE_SGTTY */
#else
        tcgetattr(0, &orig);
        tcgetattr(0, &t);
#endif

#ifdef  USE_SGTTY
        t.sg_flags &= ~(XTABS | ECHO | CRMOD);
        t.sg_flags |= CBREAK;
#else
        t.c_lflag &= ~(ICANON | ECHO | ICRNL);
        t.c_lflag |= INLCR;
        t.c_cc[VMIN] = 1;
        t.c_cc[VTIME] = 0;
#if !defined(SGI_IRIX)
        t.c_oflag &= (~XTABS);
#else
        t.c_oflag &= (~TAB3);
#endif
#endif  /* USE_SGTTY */
        
#ifdef  USE_IOCTL_INSTEAD_OF_TERMIOS
#ifdef  USE_SGTTY
        ioctl(0, TIOCSETP, &t);
#else
        ioctl(0, TCSETSW, &t);
#endif  /* USE_SGTTY */
#else
        tcsetattr(0, TCSANOW, &t);
#endif
    }

    /*
     * Determine baud rate for Tputs.
     * Look for it in termios struct; if not found, assume 2400 baud.
     */
#ifdef  USE_SGTTY
    ospeed = t.sg_ospeed;
#else   
    ospeed = t.c_cflag & CBAUD;
#endif
    if (ospeed == 0)
        ospeed = 11;

    /*
     * Get special keys
     */
#ifdef  USE_SGTTY
    k_erase = t.sg_erase;
    k_kill = t.sg_kill;
    ioctl(0, TIOCGLTC, &t1);
    k_word_erase = t1.t_werasc;
    k_reprint = t1.t_rprntc;
#else   /* USE_SGTTY */
    k_erase = t.c_cc[VERASE];
    k_kill = t.c_cc[VKILL];
    k_word_erase = t.c_cc[VWERASE];

#if defined(SGI_IRIX) || defined(ULTRIX_MIPS)
    k_reprint = t.c_cc[VRPRNT];
#else
    k_reprint = t.c_cc[VREPRINT];
#endif
#endif  /* USE_SGTTY */
    
#endif  /* DJGCC_386 */ 
}

int
os_init( int *argc, char *argv[], const char *prompt, char *buf, int bufsiz )
{
#ifdef  DJGCC_386   
    FILE    *fp;
    char    nbuf[128];
    
    /*
     * Monochrome monitor?
     */
    if (bios_video_monochrome()) {
        revcolor = 0x70;
        plaincolor = 7;
        boldcolor = 0x70;
    }
    else if ( os_locate("trcolor.dat", 11, argv[0], nbuf, sizeof(nbuf))
         && (fp = fopen(nbuf, "r")) ) {
        int     i;
        static int *colorptr[] = { &plaincolor, &boldcolor, &revcolor };

        for (i = 0 ; i < sizeof(colorptr)/sizeof(colorptr[0]) ; ++i)
        {
            fgets(nbuf, sizeof(nbuf), fp);
            *colorptr[i] = atoi(nbuf);
        }
        fclose(fp);
    }
    else {
        plaincolor = 7;
        revcolor = 0x70;
        boldcolor = 15;
    }
    text_color = text_normal_color;
#endif  /* DJGCC_386 */
    
    t_init();

    /*
     * Clear screen
     */
    if (unix_tdb) {
        cw = 0;
        ossclr(0, 0, LINES - 1, COLS - 1, cNORMAL);
        cw = 1;
        ossclr(0, 0, LINES - 1, COLS - 1, cNORMAL);
    }
    else
        ossclr(0, 0, LINES - 1, COLS - 1, cNORMAL);

#ifdef USE_SCROLLBACK

    osssbini(32767*8);      /* allocate scrollback buffer */

#endif /* USE_SCROLLBACK */
    
    if (!unix_tc) {
        status_mode = 1;
#ifdef  DJGCC_386       
        os_printz( "TADS (GO32 version)" );
#else
        os_printz( "TADS" );
#endif      
        status_mode = 0;
        os_score(0, 0);
    }

    return 0;
}

void
os_term(int rc)
{
    if (!unix_tc) {
        os_printz("[strike a key to exit]");
        os_waitc();
        os_printz("\n");
    }

#ifdef USE_SCROLLBACK
    osssbdel();
#endif /* USE_SCROLLBACK */

    exit(0); /* always return zero since mainline doesn't set it right */
}

void os_uninit()
{
    t_term();
}

/*
 *   Check for control-break.  Returns status of break flag, and clears
 *   the flag. 
 */
int
os_break(void)
{
    int ret;
    
    ret = break_set;
    break_set = 0;
    return ret;
}

/*
 * Handle ^C.
 */
static void
break_handler(int sig)
{
    break_set = 1;
}

/*
 *   Get an event--a key press, a timeout, etc. Added by SRG.  
 */
int os_get_event(unsigned long timeout, int use_timeout,
         os_event_info_t *info)
{
    fd_set readfds;
    struct timeval tv;
    int    retval;

    /* We're watching stdin (fd 0) for input */
    FD_ZERO(&readfds);
    FD_SET(0, &readfds);

    /* Wait for the specified time, if we're supposed to */
    tv.tv_sec = (int)(timeout / 1000);
    tv.tv_usec = timeout % 1000;

    /* Redraw the screen, if necessary */
    osssb_redraw_if_needed();

    if (use_timeout)
        retval = select(1, &readfds, NULL, NULL, &tv);
    else
        retval = select(1, &readfds, NULL, NULL, NULL);

    if (!retval)
        return OS_EVT_TIMEOUT;

    (*info).key[0] = os_getc_raw();
    return OS_EVT_KEY;
}

#ifdef  DJGCC_386

int os_getch(void)
{
    return bios_getchar();
}

void os_waitc(void)
{
    int c = os_getch();

    if ((c == 0 || c == 0340)) {
        os_getch();
    }
}

int os_getc(void)
{
    static char altkeys[] = {
        30, 48, 46, 32, 18, 33, 34, 35, 23, 36, 37, 38, 50,
        49, 24, 25, 16, 19, 31, 20, 22, 47, 17, 45, 21, 44
    };

    int     c;
    static int  cmdval;
    
    if (cmdval) {
        int tmpcmd = cmdval;            /* save command for a moment */
        cmdval = 0;       /* clear the flag - next call gets another key */
        return tmpcmd;            /* return the command that we saved */
    }

tryAgain:
    c = os_getch();

    if ((c == 0 || c == 0340))  {               /* Extended keycode? */
        int   i;
        char *p;

        c = os_getch();

        /* check for alt keys */
        cmdval = 0;
        for (i = 0, p = altkeys ; i < 26 ; ++p, ++i)
        {
            if (c == *p)
            {
                cmdval = CMD_ALT + i;
                break;
            }
        }

        if (!cmdval) switch(c) {
            case 0110:  /* Up Arrow */
                cmdval = CMD_UP;
                break;
            case 0120:  /* Down Arrow */
                cmdval = CMD_DOWN;
                break;
            case 0115:  /* Right Arrow */
                cmdval = CMD_RIGHT;
                break;
            case 0113:  /* Left Arrow */
                cmdval = CMD_LEFT;
                break;
            case 0117:  /* End */
                cmdval = CMD_END;
                break;
            case 0107:  /* Home */
                cmdval = CMD_HOME;
                break;
            case 0165:  /* Ctrl-End */
                cmdval = CMD_DEOL;
                break;
            case 0123:  /* Del */
                cmdval = CMD_DEL;
                break;
            case 073:   /* F1 - change this? */
                cmdval = CMD_SCR;
                break;
            case 0111:  /* PgUp */
                cmdval = CMD_PGUP;
                break;
            case 0121:  /* PgDn */
                cmdval = CMD_PGDN;
                break;
            case 132:   /* control-PgUp */
                cmdval = CMD_TOP;
                break;
            case 118:   /* control-PgDn */
                cmdval = CMD_BOT;
                break;
            case 119:   /* control home */
                cmdval = CMD_CHOME;
                break;
            case 115:   /* control left arrow */
                cmdval = CMD_WORD_LEFT;
                break;
            case 116:   /* control right arrow */
                cmdval = CMD_WORD_RIGHT;
                break;

            case 60: cmdval = CMD_F2; break;
            case 61: cmdval = CMD_F3; break;
            case 62: cmdval = CMD_F4; break;
            case 63: cmdval = CMD_F5; break;
            case 64: cmdval = CMD_F6; break;
            case 65: cmdval = CMD_F7; break;
            case 66: cmdval = CMD_F8; break;
            case 67: cmdval = CMD_F9; break;
            case 68: cmdval = CMD_F10; break;

            case 85: cmdval = CMD_SF2; break;             /* shifted F2 */

            default:    /* Unrecognized function key */
                cmdval = 0;
                break;
        }
    }
    else
    {
        switch( c )
        {
            case 1:     /* ^A */
                cmdval = CMD_HOME;
                break;
            case 2:     /* ^B */
                cmdval = CMD_LEFT;
                break;
            case 4:     /* ^D */
                cmdval = CMD_DEL;
                break;
            case 5:     /* ^E */
                cmdval = CMD_END;
                break;
            case 6:     /* ^F */
                cmdval = CMD_RIGHT;
                break;
            case '\t':
                cmdval = CMD_TAB;
                break;
            case 11:    /* ^K */
                cmdval = CMD_DEOL;
                break;
            case 14:    /* ^N */
                cmdval = CMD_DOWN;
                break;
            case 16:    /* ^P */
                cmdval = CMD_UP;
                break;
            case 21:    /* ^U */
            case 27:    /* Escape */
                cmdval = CMD_KILL;
                break;
        }
    }

    if (cmdval) 
        return 0;
    else if (c & 0xff)
        return c & 0xff;
    else
        goto tryAgain;
}

int os_getc_raw(void)
{
    static char altkeys[] = {
        30, 48, 46, 32, 18, 33, 34, 35, 23, 36, 37, 38, 50,
        49, 24, 25, 16, 19, 31, 20, 22, 47, 17, 45, 21, 44
    };

    int     c;
    static int  cmdval;

    if (cmdval) {
        int tmpcmd = cmdval;                   /* save command for a moment */
        cmdval = 0;          /* clear the flag - next call gets another key */
        return tmpcmd;                  /* return the command that we saved */
    }

tryAgain:
    c = os_getch();

    if ((c == 0 || c == 0340))  {                      /* Extended keycode? */
        int   i;
        char *p;

        c = os_getch();

        /* check for alt keys */
        cmdval = 0;
        for (i = 0, p = altkeys ; i < 26 ; ++p, ++i)
        {
            if (c == *p)
            {
                cmdval = CMD_ALT + i;
                break;
            }
        }

        if (!cmdval) switch(c) {
        case 0110:                                              /* Up Arrow */
            cmdval = CMD_UP;
            break;
        case 0120:                                            /* Down Arrow */
            cmdval = CMD_DOWN;
            break;
        case 0115:                                           /* Right Arrow */
            cmdval = CMD_RIGHT;
            break;
        case 0113:                                            /* Left Arrow */
            cmdval = CMD_LEFT;
            break;
        case 0117:                                                   /* End */
            cmdval = CMD_END;
            break;
        case 0107:                                                  /* Home */
            cmdval = CMD_HOME;
            break;
        case 0165:                                              /* Ctrl-End */
            cmdval = CMD_DEOL;
            break;
        case 0123:                                                   /* Del */
            cmdval = CMD_DEL;
            break;
        case 0111:                                                  /* PgUp */
            cmdval = CMD_PGUP;
            break;
        case 0121:                                                  /* PgDn */
            cmdval = CMD_PGDN;
            break;
        case 132:                                           /* control-PgUp */
            cmdval = CMD_TOP;
            break;
        case 118:                                           /* control-PgDn */
            cmdval = CMD_BOT;
            break;
        case 119:                                           /* control home */
            cmdval = CMD_CHOME;
            break;
        case 115:                                     /* control left arrow */
            cmdval = CMD_WORD_LEFT;
            break;
        case 116:                                    /* control right arrow */
            cmdval = CMD_WORD_RIGHT;
            break;

        case 59: cmdval = CMD_F1; break;
        case 60: cmdval = CMD_F2; break;
        case 61: cmdval = CMD_F3; break;
        case 62: cmdval = CMD_F4; break;
        case 63: cmdval = CMD_F5; break;
        case 64: cmdval = CMD_F6; break;
        case 65: cmdval = CMD_F7; break;
        case 66: cmdval = CMD_F8; break;
        case 67: cmdval = CMD_F9; break;
        case 68: cmdval = CMD_F10; break;

        case 85: cmdval = CMD_SF2; break;                     /* shifted F2 */

        default:                               /* Unrecognized function key */
            cmdval = 0;
            break;
        }
    }

    if (cmdval) 
        return 0;
    else if (c & 0xff)
        return c & 0xff;
    else
        goto tryAgain;
}

#else   /* DJGCC_386 */

int
os_getc(void)
{
    char        c;
    static char cbuf;
    static int  buffered = 0;
    os_event_info_t evt;

    /* if there's a special key pending, return it */
    if (buffered) {
        buffered = 0;
        return cbuf;
    }

    /* get the next character from standard input (non-blocking) */
    read(0, &c, 1);

    /* refresh the screen on ^L */
    while (c == 12 || c == k_reprint) {
        t_refresh();
        read(0, &c, 1);
    }

    /* translate the raw keystroke to a CMD_xxx sequence */
    evt.key[0] = c;
    oss_raw_key_to_cmd(&evt);

    /* read the new keystroke from the translated event */
    c = evt.key[0];
    cbuf = evt.key[1];

    /* if it's special, note the buffered command code for the next call */
    if (c == 0)
        buffered = 1;

    /* return the primary key code */
    return c;
}

void
oss_raw_key_to_cmd(os_event_info_t *evt)
{
    char c;
    char cbuf;

#define ctrl(c) (c - 'A' + 1)

    /* get the primary character of the keystroke event */
    c = evt->key[0];
    
    /* map NL to CR */
    if (c == 10)
        c = 13;

    /* handle special keys first, since we can't use them as switch cases */
    if (c == k_erase || c == 0x7F)         /* erase key or DEL -> backspace */
        c = ctrl('H');
    if (c == k_kill)
        c = ctrl('U');

    /* map special keys to command codes */
    switch (c) {
    case 0:
        c = 1;
        break;

    case 10:
        c = 13;
        break;

    case ctrl('W'):
        c = 0;
        cbuf = CMD_WORDKILL;
        break;

    case ctrl('D'):
        c = 0;
        cbuf = CMD_DEL;
        break;

    case ctrl('U'):
        c = 0;
        cbuf = CMD_KILL;
        break;

    case ctrl('K'):
        c = 0;
        cbuf = CMD_DEOL;
        break;

    case ctrl('P'):
        c = 0;
        cbuf = CMD_UP;
        break;

    case ctrl('N'):
        c = 0;
        cbuf = CMD_DOWN;
        break;

    case ctrl('F'):
        c = 0;
        cbuf = CMD_RIGHT;
        break;

    case ctrl('B'):
        c = 0;
        cbuf = CMD_LEFT;
        break;

    case ctrl('E'):
        c = 0;
        cbuf = CMD_END;
        break;

    case ctrl('A'):
        c = 0;
        cbuf = CMD_HOME;
        break;

    case 27:
        c = 0;
        cbuf = CMD_SCR;
        break;

    case '<':
        c = 0;
        cbuf = CMD_PGUP;
        break;

    case '>':
        c = 0;
        cbuf = CMD_PGDN;
        break;
    }

    /* store the two-character sequence back in the event */
    evt->key[0] = c;
    evt->key[1] = cbuf;
}


int
os_getc_raw(void)
{
    char c;

    /*
     *   Get the next character from standard input.  (Non-blocking) 
     */
    read(0, &c, 1);

    /*
     *   Handle special keys.  os_gets expects certain keys to be certain
     *   codes.  
     */
    if (c == k_erase || c == 0x7F)         /* erase key or DEL -> backspace */
        c = ctrl('H');
    if (c == k_kill)
        c = ctrl('U');

    /* handle some more special key translations */
    switch (c) {
    case 0:
        c = 1;
        break;

    case 10:
        /* map newline to return */
        c = 13;
        break;
    }

    /* return the key */
    return c;
#undef ctrl
}

void
os_waitc(void)
{
    char    c;

    /*
     * Get the next character from standard input.
     * Wait until we get one.
     */
    while (read(0, &c, 1) < 1);
}
#endif  /* DJGCC_386 */

/*
 * Update the real screen so it matches our idea of what's on the screen.
 */
static void
t_refresh(void)
{
    if (unix_tdb) {
        t_redraw(0, LINES * 2 - 1);
    }
    else
        t_redraw(0, LINES - 1);
}

/*
 * Update internal understanding of where the physical cursor is.
 */
static void
t_update(int y, int x)
{
    cursorX = x;
    cursorY = y;
}

/*
 * Move the cursor.  Note that this has nothing to do with the virtual cursor
 * that TADS tells us to move around -- that's updated by ossloc.
 *
 * The force parameter tells us to not use the current value of cursorX
 * and cursorY and to reposition the cursor absolutely.
 *
 */
static void
t_loc(int y, int x, int force)
{
#ifdef  DJGCC_386
    
    bios_video_set_cursor_position(x, y);
    
#else   /* DJGCC_386 */ 
    
    register int    i;

#if 0
#define X printf("[line %d]\n", __LINE__);
#else
#define X
#endif

    if (!force)
        if (cursorX == x && cursorY == y)
            return;

#if 0
    printf("[*%d, %d*]", x, y);
#endif

    /*
     * User direct cursor addressing if we have it; otherwise
     * home the cursor and move right and down from there.
     *
     * If we can get to the destiation by moving just one
     * space, we'll use the shorter single movement commands.
     */
    if (!force && x == cursorX && y == cursorY - 1 && (Tup || TUP)) {
X
        /*
         * Up one space
         */
        if (Tup)
            t_puts(Tup);
        else
            t_puts(Tparm(TUP, 1));
    }   
    else if (!force && x == cursorX && y == cursorY + 1 && (Tdo || TDO)) {
X
        /*
         * Down one space
         */
        if (Tdo)
            t_puts(Tdo);
        else
            t_puts(Tparm(TDO, 1));
    }
    else if (!force && y == cursorY && x == cursorX + 1 && (Tnd || TRI)) {
X
        /*
         * Right one space
         */
        if (Tnd)
            t_puts(Tnd);
        else
            t_puts(Tparm(TRI, 1));
    }
    else if (!force && y == cursorY && x == cursorX - 1 && (Tle || TLE)) {
X
        /*
         * Left one space
         */
        if (Tle)
            t_puts(Tle);
        else
            t_puts(Tparm(TLE, 1));
    }
    else if (Tcm) {
X
        t_puts(Tparm(Tcm, y, x));
    }
    else {
X
        t_puts(Tho);

        if (TRI)
            t_puts(Tparm(TRI, x - 1));
        else
            for (i = 0; i < x; i++)
                t_puts(Tnd);

        if (TDO)
            t_puts(Tparm(TDO, y - 1));
        else 
            for (i = 0; i < y; i++)
                t_puts(Tdo);
    }

#undef X
    t_update(y, x);

#endif  /* DJGCC_386 */
}

/*
 * Change subsequent text to new color.
 */
static void
t_color(char c)
{
#ifdef  DJGCC_386

    if (c & cREVERSE)
        bios_video_set_color(revcolor);
    else if (c & cBOLD)
        bios_video_set_color(boldcolor);
    else
        bios_video_set_color(plaincolor);
    
#else   /* DJGCC_386 */
    
    if (c == COLOR)
        return;

    /*
     * Disable all attributes (if possible), including standout.
     */
    t_puts(Tme);
    if (standout_ok && !(c & cSTANDOUT))
        t_puts(Tse);    
    if (underscore_ok && !(c & cUNDERSCORE))
        t_puts(Tue);    

    if (c & cSTANDOUT)
        if (standout_ok)
            t_puts(Tso);
    if (c & cREVERSE)
        if (rev_ok)
            t_puts(Tmr);
    if (c & cBOLD)
        if (bold_ok)
            t_puts(Tmd);
    if (c & cBLINK)
        if (blink_ok)
            t_puts(Tmb);
    if (c & cUNDERSCORE)
        if (underscore_ok)
            t_puts(Tus);

    /*
     * Save color.
     */
    COLOR = c;
    
#endif  /* DJGCC_386 */ 
}

/*
 * Redraw a portion of the screen.
 */
static void
t_redraw(int top, int bottom)
{
    static char ds[MAXCOLS + 1] = {0};

    register int    row, col, pos;
    int     color, newcolor;

    /*
     * Hide cursor
     */
    t_hide();

    /*
     * Position to top line, column zero.
     */
    t_loc(top, 0, 1);

    /*
     * Redraw each line between top line and bottom line.
     * We have to build a string from our text and color
     * information so we can write the output a line at a time
     * rather than a character at a time.
     */
    color = colors[top][0];
    t_color(color);
    for (row = top; row <= bottom; row++) {
        /*
         * Move cursor to beginning of this line
         */
        t_loc(row, 0, 1);

        /*
         * Combine color and text information into a single
         * string for this line.
         */
        for (col = 0, pos = 0; col < COLS; col++) {
            /*
             * Do we have to change text color? 
             * If so, print the text we've buffered for
             * this line and then print the change string.
             */
            if ((newcolor = colors[row][col]) != color) {
                ds[pos] = 0;
                t_outs(ds);
                t_color(newcolor);
                t_loc(row, col, 1);
                pos = 0;
                color = newcolor;
            }
            ds[pos++] = screen[row][col];
        }   

        ds[pos] = 0;
        t_outs(ds);     /* blast the string */
    }

    /*
     * Reposition cursor to its former location
     */
    t_loc(inputY, inputX, 1);

    /*
     * Show cursor
     */
    t_show();
}

/*
 * Scroll region
 * lines < 0 -> scroll up (delete line)
 * lines > 0 -> scroll down (insert line)
 */
static void
t_scroll(int top, int bot, int lines)
{
#ifdef  DJGCC_386
    
    bios_video_scroll_region(top, bot, 0, COLS - 1, lines);
    
#else   /* DJGCC_386 */

    char    *single = lines > 0 ? Tsr : Tsf;
    char    *multi  = lines > 0 ? TSR : TSF;
    int labs = lines < 0 ? -lines : lines;
    int i;

    /*
     * Make sure new lines have the right background color
     */
    t_color(text_normal_color);

    /*
     * If we have scroll region capability, use it.
     * Otherwise fake it with insert/delete line.
     */
    if ((single || multi) && (Tcs || TcS || Twi)) {
        t_set_scroll_region(top, bot);

        if (lines < 0)
            t_loc(bot, 0, 1);
        else
            t_loc(top, 0, 1);

        if (labs > 1 && multi || !single)
            t_puts(Tparm(multi, labs));
        else
            for (i = 0; i < labs; i++)
                t_puts(single);

        t_set_scroll_region(0, LINES - 1);
    }
    else {
        /*
         * Make sure we never eat lines below the window
         */
        if (labs> bot - top + 1)
            labs = bot - top + 1;

        if (lines < 0) {
            /*
             * Delete lines
             */
            t_loc(top, 0, 1);
            if (TDL && labs != 1) {
                t_puts(Tparm(TDL, labs));
            }
            else if (Tdl) {
                for (i = 0; i < labs; i++)
                    t_puts(Tdl);
            }
            else {
                /* shouldn't happen */
            }

            /*
             * Insert lines to keep windows below this
             * one intact.
             */
            if (bot < LINES - 1) {
                t_loc(bot + 1 - labs, 0, 1);
                if (TAL && labs != 1) {
                    t_puts(Tparm(TAL, labs));
                }
                else if (Tal) {
                    for (i = 0; i < labs; i++)
                        t_puts(Tal);
                }
                else {
                    /* shouldn't happen */
                }
            }   
        }
        else {
            /*
             * Insert lines
             */

            /*
             * Delete lines to keep windows below this
             * one intact.
             */
            if (bot < LINES - 1) {
                t_loc(bot + 1 - labs, 0, 1);
                if (TDL && labs != 1) {
                    t_puts(Tparm(TDL, labs));
                }
                else if (Tdl) {
                    for (i = 0; i < labs; i++)
                        t_puts(Tdl);
                }
                else {
                    /* shouldn't happen */
                }
            }


            /*
             * Insert lines at top of window.
             */
            t_loc(top, 0, 1);
            if (TAL && labs != 1) {
                t_puts(Tparm(TAL, labs));
            }
            else if (Tal) {
                for (i = 0; i < labs; i++)
                    t_puts(Tal);
            }
            else {
                /* shouldn't happen */
            }
        }
    }

    /*
     * After we scroll, we don't know where the cursor is.
     */
    t_update(-1, -1);
    
#endif  /* DJGCC_386 */
}

static void
t_set_scroll_region(int top, int bot)
{
#ifndef DJGCC_386
    static int  topsave = -1, botsave = -1;

    if (top == topsave && bot == botsave)
        return;

    if (Tcs) {
        t_puts(Tparm(Tcs, top, bot));
    }
    else if (TcS) {
        t_puts(Tparm(TcS, LINES, top, LINES - (bot + 1), LINES));
    }
    else {
        t_puts(Tparm(Twi, top, 0, bot, COLS - 1));
    }

    topsave = top;
    botsave = bot;
#endif  
}

/*
 * Scroll region down a line.
 * This is equivalent to inserting a line at the top of the region.
 */
void
ossscu(int top, int left, int bottom, int right, int blank_color)
{
    register int    r1, r2, col;

    if (unix_tdb) {
        top += LINES * cw;
        bottom += LINES * cw;
    }
        
#ifdef DEBUG_OUTPUT
    printf("[ossscu(%d, %d, %d, %d, %d)]", 
        top, left, bottom, right, blank_color);
#endif

    if (unix_tc)
        return;
    
    /*
     * Update our internal version
     */
    for (r1 = bottom - 1, r2 = bottom; r1 >= top; r1--, r2--)
        for (col = left; col <= right; col++) {
            screen[r2][col] = screen[r1][col];
            colors[r2][col] = colors[r1][col];
        }   
    for (col = left; col <= right; col++) {
        screen[r2][col] = ' ';
        colors[r2][col] = blank_color;
    }

    /*
     * If we can duplicate the effect of this scroll on the screen
     * with scrolling commands, do so; otherwise, refresh by redrawing
     * every affected line.
     */
#ifdef  T_OPTIMIZE
    if (left == 0 && right >= unix_max_column) {
        t_scroll(top, bottom, 1);
    }
    else {
#else
        t_redraw(top, bottom);
#endif

#ifdef  T_OPTIMIZE
    }
#endif
}

/*
 * Scroll region up a line
 * This is equivalent to deleting a line at the top of the region and pulling
 * everything below it up.
 */
void
ossscr(int top, int left, int bottom, int right, int blank_color)
{
    register int    r1, r2, col;

    if (unix_tdb) {
        top += LINES * cw;
        bottom += LINES * cw;
    }
    
#ifdef DEBUG_OUTPUT
    printf("[ossscr(%d, %d, %d, %d, %d)]", 
        top, left, bottom, right, blank_color);
#endif

    if (unix_tc) {
        putchar('\n');
        return;
    }

    /*
     * Update our internal version
     */
    for (r1 = top, r2 = top + 1; r2 <= bottom; r1++, r2++)
        for (col = left; col <= right; col++) {
            screen[r1][col] = screen[r2][col];
            colors[r1][col] = colors[r2][col];
        }   
    for (col = left; col <= right; col++) {
        screen[r1][col] = ' ';
        colors[r1][col] = blank_color;
    }

    /*
     * If we can duplicate the effect of this scroll on the screen
     * with a scrolling command, do so; otherwise, refresh by redrawing
     * every affected line.
     */
#ifdef  T_OPTIMIZE
    if (left == 0 && right >= unix_max_column) {
        t_scroll(top, bottom, -1);
    }
    else {
#else
        t_redraw(top, bottom);
#endif

#ifdef  T_OPTIMIZE
    }
#endif
}

/*
 * Clear region (fill with spaces)
 */
void
ossclr(int top, int left, int bottom, int right, int blank_color)
{
    register int    row, col;

    if (unix_tdb) {
        top += LINES * cw;
        bottom += LINES * cw;
    }

#ifdef DEBUG_OUTPUT
    printf("[ossclr(%d, %d, %d, %d, %d)]", 
        top, left, bottom, right, blank_color);
#endif

    if (unix_tc)
        return;

    /*
     * Update our internal version
     */
    for (row = top; row <= bottom; row++)
        for (col = left; col <= right; col++) {
            screen[row][col] = ' ';
            colors[row][col] = blank_color;
        }

    /*
     * If we can duplicate the effect of this clear on the screen
     * with a clear command, do so; otherwise, refresh by redrawing every
     * affected line.
     */
#ifdef  DJGCC_386
    bios_video_set_bkgnd(plaincolor);
    bios_video_clear_region(top, bottom, left, right);
#else   /* DJGCC_386 */ 
    
#ifdef  T_OPTIMIZE
    if (Tce && left == 0 && right >= unix_max_column) {
        t_color(blank_color);

        if (Tcl && top == 0 && bottom >= unix_max_line) {
            t_loc(0, 0, 1);
            t_puts(Tcl);
        }
        else if (Tcd && bottom >= unix_max_line) {
            t_loc(top, 0, 1);
            t_puts(Tcd);
        }
        else for (row = top; row <= bottom; row++) {
            t_loc(row, 0, 1);
            t_puts(Tce);
        }

        /*
         * Don't know where the cursor is after clear.
         */
        t_update(-1, -1);
    }
    else {
#endif  
        t_redraw(top, bottom);
#ifdef  T_OPTIMIZE
    }
#endif  
    
#endif  /* DJGCC_386 */ 
}

/*
 * Locate (input) cursor at given row and column.
 * Nore that this is never used to determine where things are drawn.
 * It's only used for aesthetics; i.e., showing the user where input
 * will be taken from next.
 */ 
void
ossloc(int row, int col)
{
    if (unix_tc)
        return;

    if (unix_tdb) 
        row += LINES * cw;

    t_loc(row, col, 0);
    
    /*
     * Update internal cursor position so t_redraw will
     * be correct.
     */
    inputX = col;
    inputY = row;   
}

/*
 * Display msg with color at coordinates (y, x).
 * The color must be in the range 0 <= color <= 16, and specifies both
 * foreground and background colors.
 */
void ossdsp(int y, int x, int color, const char *msg)
{
    register int    col;
    register char   *s, *m = msg, *c;

#ifdef DEBUG_OUTPUT
    printf("[ossdsp(%d, %d, %d, \"%s\")]", y, x, color, msg);
#endif

    if (unix_tc) {
        printf("%s", msg);
        fflush(stdout);
    }

    if (y >= LINES || x >= COLS)
        return;

    if (unix_tdb)
        y += LINES * cw;
    
    s = &screen[y][x];
    c = &colors[y][x];
    
    /*
     * Update our own version of the screen.
     */
    for (col = x; *m && col < COLS; col++) {
        *s++ = *m++;
        *c++ = color;
    }

#ifdef  FAST_OSSDSP
    /*
     * XXX
     *
     * We redraw the whole line if it's the status line.
     * This is pretty bogus.
     */
     t_loc(y, x, 0);
     t_color(color);
     t_outs(msg);
     t_update(cursorY, cursorX + strlen(msg));
#else
    t_redraw(y, y);
#endif
}


/*
 * Stuff for the debugger
 */
void
ossgmx(int *maxline, int *maxcol)
{
    *maxline = LINES - 1;
    *maxcol = COLS - 1;
}

/* clear a window */
void osdbgclr(oswdef *win)
{
    ossclr(win->oswy1, win->oswx1, win->oswy2, win->oswx2, win->oswcolor);
}

int osdbgini(int rows, int cols)
{
    return(0);
}

int ossvpg(char pg)
{
    static int  s_inputX = 0, s_inputY = 0;
    int     ret;

    if (cw == pg)
        return cw;

    inputX = s_inputX;
    inputY = s_inputY;

    ret = cw;
    cw = pg;
    t_update(-1, -1);
    COLOR = -1;
/*    t_refresh(); */

    return ret;
}

int ossmon(void)
{
    return 0;
}

/* scroll a window up a line */
static void osdbgsc(oswdef *win)
{
    ossscr(win->oswy1, win->oswx1, win->oswy2, win->oswx2, win->oswcolor);
}

# ifdef USE_STDARG
void osdbgpt(oswdef *win, const char *fmt, ...)
# else /* USE_STDARG */
void osdbgpt( win, fmt, a1, a2, a3, a4, a5, a6, a7, a8 )
oswdef *win;
char *fmt;
long  a1, a2, a3, a4, a5, a6, a7, a8;
# endif /* USE_STDARG */
{
    char buf[256];
    char *p;

# ifdef USE_STDARG
    va_list argptr;

    va_start( argptr, fmt );
    vsprintf( buf, fmt, argptr );
    va_end( argptr );
# else /* USE_STDARG */
    sprintf( buf, fmt, a1, a2, a3, a4, a5, a6, a7, a8 );
# endif /* USE_STDARG */

    for (p=buf ; *p ; )
    {
    char *p1;

    if ((win->oswflg & OSWFMORE) && win->oswx == win->oswx1 &&
        win->oswmore+1 >= win->oswy2 - win->oswy1)
    {
        char c;
        int eof;
        
        ossdsp(win->oswy, win->oswx, win->oswcolor, "[More]");
        ossdbgloc(win->oswy, win->oswx+6);
        eof = FALSE;
        do
        {
        switch(c = os_getc())
        {
        case '\n':
        case '\r':
            win->oswmore--;
            break;

        case ' ':
            win->oswmore = 0;
            break;

        case 0:
            if (os_getc() == CMD_EOF)
            {
            eof = TRUE;
            win->oswmore = 0;
            }
            break;
        }
        } while (c != ' ' && c != '\n' && c != '\r' && !eof);
        
        ossdsp(win->oswy, win->oswx, win->oswcolor, "      ");
    }

    for (p1 = p ; *p1 && *p1 != '\n' && *p1 != '\r' && *p1 != '\t'; p1++);
    if (*p1 == '\n' || *p1 == '\r' || *p1 == '\t')
    {
        int c = *p1;
        
        *p1 = '\0';

        if (win->oswx + strlen(p) > win->oswx2 &&
        (win->oswflg & OSWFCLIP))
        p[win->oswx2 - win->oswx + 1] = '\0';
        ossdsp(win->oswy, win->oswx, win->oswcolor, p);

        if (c == '\n')
        {
        ++(win->oswy);
        win->oswx = win->oswx1;
        if (win->oswy > win->oswy2)
        {
            win->oswy = win->oswy2;
            osdbgsc(win);
        }
        win->oswmore++;
        }
        else if (c == '\t')
        {
        win->oswx += strlen(p);
        do
        {
            ossdsp(win->oswy, win->oswx, win->oswcolor, " ");
            ++(win->oswx);
            if (win->oswx > win->oswx2 && (win->oswflg & OSWFCLIP))
            break;
        } while ((win->oswx - 2) & 7);
        }
        p = p1 + 1;
        if (win->oswx > win->oswx2) return;
    }
    else
    {
        if (win->oswx + strlen(p) > win->oswx2
        && (win->oswflg & OSWFCLIP))
        p[win->oswx2 - win->oswx + 1] = '\0';
        ossdsp(win->oswy, win->oswx, win->oswcolor, p);
        win->oswx += strlen(p);
        p = p1;
    }
    }
}

/* open a window - set up location */
void osdbgwop(oswdef *win, int x1, int y1, int x2, int y2, int color)
{
    win->oswx1 = win->oswx = x1;
    win->oswx2 = x2;
    win->oswy1 = win->oswy = y1;
    win->oswy2 = y2;
    win->oswcolor = color;
    win->oswflg = 0;
}

void ossdbgloc(int y, int x)
{
    ossloc(y, x);
}

/* get some text */
int osdbggts( oswdef *win,
          char *buf,
          int (*cmdfn)(/*_ void *ctx, char cmd _*/),
          void *cmdctx)
{
    char *p = buf;
    char *eol = buf;
    char *eob = buf + 127;
    int   x = win->oswx;
    int   y = win->oswy;
    int   origx = x;
    int   cmd = 0;

    win->oswmore = 0;
    for (buf[0] = '\0' ; ; )
    {
    char c;

    ossdbgloc(y, x);
    switch(c = os_getc())
    {
        case 8:
        if (p > buf)
        {
            char *q;
            char  tmpbuf[2];
            int   thisx, thisy;

            for ( q=(--p) ; q<eol ; q++ ) *q = *( q+1 );
            eol--;
            if ( --x < 0 )
            {
            x = win->oswx2;
            y--;
            }
            *eol = ' ';
            thisx = x;
            thisy = y;
            for ( q=p, tmpbuf[1]='\0' ; q<=eol ; q++ )
            {
            tmpbuf[0] = *q;
            ossdsp( thisy, thisx, win->oswcolor, tmpbuf );
            if ( ++thisx > win->oswx2 )
            {
                thisx = 0;
                thisy++;
            }
            }
            *eol = '\0';
        }
        break;
        case 13:
        /*
         *   Scroll the screen to account for the carriage return,
         *   position the cursor at the end of the new line, and
         *   null-terminate the line.
         */
        *eol = '\0';
        while( p != eol )
        {
            p++;
            if ( ++x > win->oswx2 )
            {
            y++;
            x = 0;
            }
        }

        if ( y == win->oswy2 ) osdbgsc(win);
        else ++y;
        x = 0;
        ossdbgloc( y, x );

        /*
         *   Finally, copy the buffer to the screen save buffer
         *   (if applicable), and return the contents of the buffer.
         */
        win->oswx = x;
        win->oswy = y;
        return(0);

        case 0:
        switch(c = os_getc())
        {
            case CMD_EOF:
            return 0; 

            case CMD_LEFT:
            if ( p>buf )
            {
                p--;
                x--;
                if ( x < 0 )
                {
                x = win->oswx2;
                y--;
                }
            }
            break;
            case CMD_RIGHT:
            if ( p<eol )
            {
                p++;
                x++;
                if ( x > win->oswx2 )
                {
                x = 0;
                y++;
                }
            }
            break;
            case CMD_DEL:
            if ( p<eol )
            {
                char *q;
                char  tmpbuf[2];
                int   thisx=x, thisy=y;

                for ( q=p ; q<eol ; q++ ) *q = *(q+1);
                eol--;
                *eol = ' ';
                for ( q=p, tmpbuf[1]='\0' ; q<=eol ; q++ )
                {
                tmpbuf[0] = *q;
                ossdsp( thisy, thisx, win->oswcolor, tmpbuf );
                if ( ++thisx > win->oswx2 )
                {
                    thisx = 0;
                    thisy++;
                }
                }
                *eol = '\0';
            }
            break;
            case CMD_KILL:
            case CMD_HOME:
            do_kill:
            while( p>buf )
            {
                p--;
                if ( --x < 0 )
                {
                x = win->oswx2;
                y--;
                }
            }
            if ( c == CMD_HOME ) break;
            /*
             *   We're at the start of the line now; fall
             *   through for KILL, UP, and DOWN to the code
             *   which deletes to the end of the line.
             */
            case CMD_DEOL:
            if ( p<eol )
            {
                char *q;
                int   thisx=x, thisy=y;

                for ( q=p ; q<eol ; q++ )
                {
                ossdsp( thisy, thisx, win->oswcolor, " " );
                if ( ++thisx > win->oswx2 )
                {
                    thisx = 0;
                    thisy++;
                }
                }
                eol = p;
                *p = '\0';
            }
            if (cmd) return(cmd);
            break;
            case CMD_END:
            while ( p<eol )
            {
                p++;
                if ( ++x > win->oswx2 )
                {
                x = 0;
                y++;
                }
            }
            break;
            
            default:
            if (cmd = (*cmdfn)(cmdctx, c))
            {
                c = CMD_KILL;
                goto do_kill;
            }
            break;
        }
        break;
        default:
        if ( c >= ' ' && c < 127 && eol<eob )
        {
            if ( p != eol )
            {
            char *q;
            int   thisy=y, thisx=x;
            char  tmpbuf[2];

            for ( q=(++eol) ; q>p ; q-- ) *q=*(q-1);
            *p = c;
            for ( q=p++, tmpbuf[1] = '\0' ; q<eol ; q++ )
            {
                tmpbuf[0] = *q;
                ossdsp( thisy, thisx, win->oswcolor, tmpbuf );
                thisx++;
                if ( thisx > win->oswx2 )
                {
                thisx = 0;
                if ( thisy == win->oswy2 )
                {
                    y--;
                    osdbgsc(win);
                }
                else thisy++;
                }
            }
            if ( ++x > win->oswx2 )
            {
                y++;
                x = 0;
            }
            }
            else
            {
            *p++ = c;
            *p = '\0';
            eol++;
            ossdsp( y, x, win->oswcolor, p-1 );
            if ( ++x > win->oswx2 )
            {
                x = 0;
                if ( y == win->oswy2 )
                osdbgsc(win);
                else y++;
            }
            }
        }
        break;
    }
    }
}


/*
 * End of stuff for debugger
 */

#else   /* USE_STDIO */
    
int
os_init( int *argc, char *argv[], const char *prompt, char *buf, int bufsiz )
{
    return 0;
}

void
os_uninit()
{
}

void
os_term(int rc)
{
    exit(rc);
}

#ifndef USE_HTML
int
os_break(void)
{
    int ret;
    
    ret = break_set;
    break_set = 0;
    return ret;
}

void
os_waitc(void)
{
    getchar();     /* Changed from getkey() by SRG */
}

int
os_getc(void)
{
    return getchar();  /* Changed from getkey() by SRG */
}

int
os_getc_raw(void)
{
    return os_getc();
}
#endif

#endif  /* USE_STDIO */

int
os_paramfile(char *buf)
{
    return 0;
}

/*
 *   os_exeseek  - opens the given .EXE file and seeks to the end of the
 *   executable part of it, on the presumption that a datafile is to be
 *   found there.
 */
osfildef *os_exeseek(const char *exefile, const char *typ )
{
    return((osfildef *)0);
}

/*
 *   os_exfld  - load in an external function from an open file, given
 *   the size of the function (in bytes).  Returns a pointer to the newly
 *   allocated memory block containing the function in memory.
 */
int (*os_exfld( osfildef *fp, unsigned len ))(void *)
{
    return  (int (*)(void *)) 0;
}

/*
 *   Load an external function from a file.  This routine assumes that
 *   the file has the same name as the resource.  
 */
int (*os_exfil(const char *name ))(void *)
{
    return (int (*)(void *)) 0;
}

/*
 *   call an external function, passing it an argument (a string pointer),
 *   and passing back the string pointer returned by the external function
 */
int os_excall(int (*extfn)(void *), void *arg)
{
    return 0;
}

/*
 *   Get the temporary file path.  This should fill in the buffer with a
 *   path prefix (suitable for strcat'ing a filename onto) for a good
 *   directory for a temporary file, such as the swap file.  
 */
void
os_get_tmp_path(char *s)
{
    strcpy(s, "/tmp");
}

/* os_defext(fn, ext) should append the default extension ext to the filename
 *  in fn.  It is assumed that the buffer at fn is big enough to hold the added
 *  characters in the extension.  The result should be null-terminated.  When
 *  an extension is already present in the filename at fn, no action should be
 *  taken.  On systems without an analogue of extensions, this routine should
 *  do nothing.
 *
 * For Unix, we extend this to also prepend the default saved game or game
 * file path name.
 * 
 *    - The TADSSAVE environment variable holds the name of the save file
 *  directory
 *    - The TADSGAME envirnoment variable holds the name of the game file
 *  directory
 *
 * We only prepend  if there are no slashes in the filename already.
 * We don't prepand paths when running as the compiler, because we don't want
 * the output files to go in weird places.
 */
void os_defext( char *fn, const char *ext )
{
    char *p, *n, tmp[1024];
    char *defpath;

    /*
     * Prepend default path
     */
   if (!memicmp(ext, "sav", strlen(ext)))
    defpath = getenv("TADSSAVE");
   else if (!memicmp(ext, "gam", strlen(ext)))
    defpath = getenv("TADSGAME");
   else
    defpath = NULL;

   if (!unix_tc && defpath) {
    /*
     * Look for slashes.  If there are any, don't mess with name.
     */
    n = fn;
    while (*n) {
        if (*n == '/')
            break;
        n++;
    }
    if (!*n) {
        strcpy(tmp, defpath);
        if (defpath[strlen(defpath)] != '/')
            strcat(tmp, "/");
        strcat(tmp, fn);
        strcpy(fn, tmp);
    }  
   }

    p = fn+strlen(fn);
    while ( p>fn )
      {
    p--;
    if ( *p=='.' ) return;      /* already has an extension */
    if ( *p=='/' || *p=='\\' || *p==':'
       ) break;    /* found a path */
      }
    strcat( fn, "." );          /* add a dot */
    strcat( fn, ext );          /* add the extension */
  }

/* os_remext(fn) removes the extension from fn, if present.  The buffer at
 *  fn should be modified in place.  If no extension is present, no action
 *  should be taken.  For systems without an analogue of extensions, this
 *  routine should do nothing.
 */
void os_remext( char *fn )
{
    char *p = fn+strlen(fn);
    while ( p>fn )
      {
    p--;
    if ( *p=='.' )
      {
        *p = '\0';
        return;
      }
    if ( *p=='/' || *p=='\\' || *p==':'
        ) return;
      }
}

/*
 * Add an extension, even if the filename currently has one 
 */
void os_addext(char *fn, const char *ext)
{
    strcat(fn, ".");
    strcat(fn, ext);
}

/*
 * Returns a pointer to the root portion of the filename. Added by SRG.
 */
char *os_get_root_name(const char *buf)
{
    char *p = buf;

    p += strlen(buf) - 1;
    while (*p != '/' && p > buf)
    p--;
    if (p != buf) p++;

    return p;
}

#ifndef USE_HTML
/*
 * This is an extremely unsophisticated version of os_xlat_html4, which translates
 * everything it can into its near-ASCII equivalent. Added by SRG, after code from
 * OSDOS.C.
 */
void os_xlat_html4(unsigned int html4_char, char *result, size_t result_len)
{
    /* default character to use for unknown charaters */
#define INV_CHAR " "

    /* 
     *   Translation table - provides mappings for characters the ISO
     *   Latin-1 subset of the HTML 4 character map (values 128-255).
     *   
     *   Characters marked "(approx)" are approximations where the actual
     *   desired character is not available, but a reasonable approximation
     *   is used.  Characters marked "(approx unaccented)" are accented
     *   characters that are not available; these use the unaccented equivalent
     *   as an approximation, since this will presumably convey more meaning
     *   than a blank.
     *   
     *   Characters marked "(n/a)" have no equivalent (even approximating),
     *   and are mapped to spaces.
     *   
     *   Characters marked "(not used)" are not used by HTML '&' markups.  
     */
    static const char *xlat_tbl[] =
    {
    INV_CHAR,                     /* 128 (not used) */
    INV_CHAR,                     /* 129 (not used) */
    "'",                        /* 130 - sbquo (approx) */
    INV_CHAR,                     /* 131 (not used) */
    "\"",                       /* 132 - bdquo (approx) */
    INV_CHAR,                     /* 133 (not used) */
    INV_CHAR,                     /* 134 - dagger (n/a) */
    INV_CHAR,                     /* 135 - Dagger (n/a) */
    INV_CHAR,                     /* 136 (not used) */
    INV_CHAR,                     /* 137 - permil (n/a) */
    INV_CHAR,                     /* 138 (not used) */
    "<",                       /* 139 - lsaquo (approx) */
    INV_CHAR,                      /* 140 - OElig (n/a) */
    INV_CHAR,                     /* 141 (not used) */
    INV_CHAR,                     /* 142 (not used) */
    INV_CHAR,                     /* 143 (not used) */
    INV_CHAR,                     /* 144 (not used) */
    "'",                        /* 145 - lsquo (approx) */
    "'",                        /* 146 - rsquo (approx) */
    "\"",                       /* 147 - ldquo (approx) */
    "\"",                       /* 148 - rdquo (approx) */
    INV_CHAR,                     /* 149 (not used) */
    "-",                            /* 150 - endash */
    "--",                           /* 151 - emdash */
    INV_CHAR,                     /* 152 (not used) */
    "(tm)",                     /* 153 - trade (approx) */
    INV_CHAR,                     /* 154 (not used) */
    ">",                       /* 155 - rsaquo (approx) */
    INV_CHAR,                      /* 156 - oelig (n/a) */
    INV_CHAR,                     /* 157 (not used) */
    INV_CHAR,                     /* 158 (not used) */
    "Y",                  /* 159 - Yuml (approx unaccented) */
    INV_CHAR,                     /* 160 (not used) */
    "!",                             /* 161 - iexcl */
    INV_CHAR,                   /* 162 - cent (n/a) */
    INV_CHAR,                      /* 163 - pound (n/a) */
    INV_CHAR,                     /* 164 - curren (n/a) */
    INV_CHAR,                    /* 165 - yen (n/a) */
    "|",                          /* 166 - brvbar (n/a) */
    INV_CHAR,                   /* 167 - sect (n/a) */
    INV_CHAR,                    /* 168 - uml (n/a) */
    "(c)",                       /* 169 - copy (approx) */
    INV_CHAR,                   /* 170 - ordf (n/a) */
    INV_CHAR,                      /* 171 - laquo (n/a) */
    INV_CHAR,                    /* 172 - not (n/a) */
    " ",                         /* 173 - shy (n/a) */
    "(R)",                        /* 174 - reg (approx) */
    INV_CHAR,                   /* 175 - macr (n/a) */
    INV_CHAR,                    /* 176 - deg (n/a) */
    INV_CHAR,                     /* 177 - plusmn (n/a) */
    INV_CHAR,                   /* 178 - sup2 (n/a) */
    INV_CHAR,                   /* 179 - sup3 (n/a) */
    "'",                        /* 180 - acute (approx) */
    INV_CHAR,                      /* 181 - micro (n/a) */
    INV_CHAR,                   /* 182 - para (n/a) */
    INV_CHAR,                     /* 183 - middot (n/a) */
    ",",                        /* 184 - cedil (approx) */
    INV_CHAR,                   /* 185 - sup1 (n/a) */
    INV_CHAR,                   /* 186 - ordm (n/a) */
    INV_CHAR,                      /* 187 - raquo (n/a) */
    "1/4",                     /* 188 - frac14 (approx) */
    "1/2",                     /* 189 - frac12 (approx) */
    "3/4",                     /* 190 - frac34 (approx) */
    "?",                       /* 191 - iquest (approx) */
    "A",                /* 192 - Agrave (approx unaccented) */
    "A",                /* 193 - Aacute (approx unaccented) */
    "A",                 /* 194 - Acirc (approx unaccented) */
    "A",                /* 195 - Atilde (approx unaccented) */
    "A",                  /* 196 - Auml (approx unaccented) */
    "A",                 /* 197 - Aring (approx unaccented) */
    "AE",                /* 198 - AElig (approx unaccented) */
    "C",                /* 199 - Ccedil (approx unaccented) */
    "E",                /* 200 - Egrave (approx unaccented) */
    "E",                /* 201 - Eacute (approx unaccented) */
    "E",                 /* 202 - Ecirc (approx unaccented) */
    "E",                  /* 203 - Euml (approx unaccented) */
    "I",                /* 204 - Igrave (approx unaccented) */
    "I",                /* 205 - Iacute (approx unaccented) */
    "I",                 /* 206 - Icirc (approx unaccented) */
    "I",                  /* 207 - Iuml (approx unaccented) */
    INV_CHAR,                    /* 208 - ETH (n/a) */
    "N",                /* 209 - Ntilde (approx unaccented) */
    "O",                /* 210 - Ograve (approx unaccented) */
    "O",                /* 211 - Oacute (approx unaccented) */
    "O",                 /* 212 - Ocirc (approx unaccented) */
    "O",                /* 213 - Otilde (approx unaccented) */
    "O",                  /* 214 - Ouml (approx unaccented) */
    "x",                        /* 215 - times (approx) */
    "O",                /* 216 - Oslash (approx unaccented) */
    "U",                /* 217 - Ugrave (approx unaccented) */
    "U",                /* 218 - Uacute (approx unaccented) */
    "U",                 /* 219 - Ucirc (approx unaccented) */
    "U",                  /* 220 - Uuml (approx unaccented) */
    "Y",                /* 221 - Yacute (approx unaccented) */
    INV_CHAR,                      /* 222 - THORN (n/a) */
    INV_CHAR,                      /* 223 - szlig (n/a) */
    "a",                /* 224 - agrave (approx unaccented) */
    "a",                /* 225 - aacute (approx unaccented) */
    "a",                 /* 226 - acirc (approx unaccented) */
    "a",                /* 227 - atilde (approx unaccented) */
    "a",                  /* 228 - auml (approx unaccented) */
    "a",                 /* 229 - aring (approx unaccented) */
    "ae",                       /* 230 - aelig (approx) */
    "c",                       /* 231 - ccedil (approx) */
    "e",                /* 232 - egrave (approx unaccented) */
    "e",                /* 233 - eacute (approx unaccented) */
    "e",                 /* 234 - ecirc (approx unaccented) */
    "e",                  /* 235 - euml (approx unaccented) */
    "i",                /* 236 - igrave (approx unaccented) */
    "i",                /* 237 - iacute (approx unaccented) */
    "i",                 /* 238 - icirc (approx unaccented) */
    "i",                  /* 239 - iuml (approx unaccented) */
    INV_CHAR,                    /* 240 - eth (n/a) */
    "n",                /* 241 - ntilde (approx unaccented) */
    "o",                /* 242 - ograve (approx unaccented) */
    "o",                /* 243 - oacute (approx unaccented) */
    "o",                 /* 244 - ocirc (approx unaccented) */
    "o",                /* 245 - otilde (approx unaccented) */
    "o",                  /* 246 - ouml (approx unaccented) */
    "/"                    /* 247 - divide (approx) */
    "o",                /* 248 - oslash (approx unaccented) */
    "u",                /* 249 - ugrave (approx unaccented) */
    "u",                /* 250 - uacute (approx unaccented) */
    "u",                 /* 251 - ucirc (approx unaccented) */
    "u",                  /* 252 - uuml (approx unaccented) */
    "y",                /* 253 - yacute (approx unaccented) */
    INV_CHAR,                      /* 254 - thorn (n/a) */
    "y"               /* 255 - yuml (approx unaccented) */
    };

    /*
     *   Map certain extended characters into our 128-255 range.  If we
     *   don't recognize the character, return the default invalid
     *   charater value.  
     */
    if (html4_char > 255)
    {
    switch(html4_char)
    {
    case 338: html4_char = 140; break;
    case 339: html4_char = 156; break;
    case 376: html4_char = 159; break;
    case 352: html4_char = 154; break;
    case 353: html4_char = 138; break;
    case 8211: html4_char = 150; break;
    case 8212: html4_char = 151; break;
    case 8216: html4_char = 145; break;
    case 8217: html4_char = 146; break;
    case 8218: html4_char = 130; break;
    case 8220: html4_char = 147; break;
    case 8221: html4_char = 148; break;
    case 8222: html4_char = 132; break;
    case 8224: html4_char = 134; break;
    case 8225: html4_char = 135; break;
    case 8240: html4_char = 137; break;
    case 8249: html4_char = 139; break;
    case 8250: html4_char = 155; break;
    case 8482: html4_char = 153; break;

    default:
        /* unmappable character - return space */
        result[0] = (unsigned char)' ';
        result[1] = 0;
        return;
    }
    }
    
    /* 
     *   if the character is in the regular ASCII zone, return it
     *   untranslated 
     */
    if (html4_char < 128)
    {
    result[0] = (unsigned char)html4_char;
    result[1] = '\0';
    return;
    }

    /* look up the character in our table and return the translation */
    strcpy(result, xlat_tbl[html4_char - 128]);
}
#endif

/*
 * Simple versions of os_advise_load_charmap and os_gen_charmap_filename. At
 * some point I'll get around to making these do something real. Added by SRG.
 */
void os_advise_load_charmap(char *id, char *ldesc, char *sysinfo)
{
}

void os_gen_charmap_filename(char *filename, char *internal_id, char *argv0)
{
    filename[0] = '\0';
}

/* 
 *   A very simple "millisecond timer." Because of the relatively-low
 *   precision of a long and because we really are only precise to a
 *   second, we store a zero-offset to begin with. Added by SRG.  
 */
long os_get_sys_clock_ms(void)
{
    if (timeZero == 0)
    timeZero = time(0);
    return ((time(0) - timeZero) * 1000);
}

/*
 *   Sleep for a given # of ms. Added by SRG.  
 */
void os_sleep_ms(long delay_in_milliseconds)
{
    usleep(delay_in_milliseconds);
}

#ifndef USE_HTML
/*
 * An empty implementation of os_set_title. Added by SRG.
 */
void os_set_title(const char *title)
{
}
#endif

/*
 * Provide memicmp since it's not a standard libc routine.
 */
int memicmp(const char *s1, const char *s2, int len)
{
    char    *x1, *x2;   
    int result;
    int i;

    x1 = malloc(len);
    x2 = malloc(len);

    if (!x1 || !x2) {
        printf("Out of memory!\n");
        exit(-1);
    }

    for (i = 0; i < len; i++) {
        if (isupper(s1[i]))
            x1[i] = tolower(s1[i]);
        else
            x1[i] = s1[i];

        if (isupper(s2[i]))
            x2[i] = tolower(s2[i]);
        else
            x2[i] = s2[i];
    }

    result = memcmp(x1, x2, len);
    free(x1);
    free(x2);
    return result;
}

int stricmp(const char* s1, const char* s2)
{
    return strcasecmp(s1, s2);
    /*const auto x1 = std::make_unique<char[]>(strlen(s1));
    const auto x2 = std::make_unique<char[]>(strlen(s2));

    for (size_t i = 0; s1[i] != '\0' and s2[i] != '\0'; ++i) {
        x1[i] = tolower(s1[i]);
        x2[i] = tolower(s2[i]);
    }
    return strcmp(x1.get(), x2.get());*/
}

/*
 * memcpy - copy bytes (handles overlap, so we can equivalence memmove() to it.
 */
void *
our_memcpy(void *dst, const void *src, size_t size)
{
    register char *d;
    register const char *s;
    register size_t n;

    if (size == 0)
        return(dst);

    s = src;
    d = dst;
    if (s <= d && s + (size-1) >= d) {
        /* Overlap, must copy right-to-left. */
        s += size-1;
        d += size-1;
        for (n = size; n > 0; n--)
            *d-- = *s--;
    } else
        for (n = size; n > 0; n--)
            *d++ = *s++;

    return(dst);
}

#if !defined(DJGCC_386)
/*
 * dbgu.c requires os_strlwr
 */
char *
os_strlwr(char *s)
{
    char *start;
    start = s;
    while (*s) {
        if (isupper(*s))
            *s = tolower(*s);

        s++;
    }
    return start;
}
#endif

#ifndef DJGCC_386

/*
 * Open file using fopen, stripping of b's from flag string.
 */
#ifdef fopen
#undef fopen
#endif

FILE *
our_fopen(char *filename, char *flags)
{
    static char f[80];
    int     i = 0;

    while (*flags) {
        if (*flags != 'b')
            f[i++] = *flags;

        flags++;
    }
    f[i] = 0;

    return fopen(filename, f);
}
#endif  /* DJGCC_386 */

/* ------------------------------------------------------------------------ */
/*
 *   Get file times 
 */

/*
 *   get file creation time 
 */
int os_get_file_cre_time(os_file_time_t *t, const char *fname)
{
    struct stat info;

    /* get the file information */
    if (stat(fname, &info))
        return 1;

    /* set the creation time in the return structure */
    t->t = info.st_ctime;
    return 0;
}

/*
 *   get file modification time 
 */
int os_get_file_mod_time(os_file_time_t *t, const char *fname)
{
    struct stat info;

    /* get the file information */
    if (stat(fname, &info))
        return 1;

    /* set the modification time in the return structure */
    t->t = info.st_mtime;
    return 0;
}

/* 
 *   get file last access time 
 */
int os_get_file_acc_time(os_file_time_t *t, const char *fname)
{
    struct stat info;

    /* get the file information */
    if (stat(fname, &info))
        return 1;

    /* set the access time in the return structure */
    t->t = info.st_atime;
    return 0;
}

/*
 *   compare two file time structures 
 */
int os_cmp_file_times(const os_file_time_t *a, const os_file_time_t *b)
{
    if (a->t < b->t)
        return -1;
    else if (a->t == b->t)
        return 0;
    else
        return 1;
}

/*
 *   open a file for reading and writing in text mode; do not truncate 
 */
osfildef *osfoprwt(const char *fname, os_filetype_t typ)
{
    osfildef *fp;

    /* try opening an existing file in read/write mode */
    fp = fopen(fname, "r+");

    /* if that failed, create a new file in read/write mode */
    if (fp == 0)
        fp = fopen(fname, "w+");

    /* return the file */
    return fp;
}

/*
 *   open a file for reading and writing in binary mode; do not truncate 
 */
osfildef *osfoprwb(const char *fname, os_filetype_t typ)
{
    osfildef *fp;

    /* try opening an existing file in read/write mode */
    fp = fopen(fname, "r+b");

    /* if that failed, create a new file in read/write mode */
    if (fp == 0)
        fp = fopen(fname, "w+b");

    /* return the file */
    return fp;
}

/*
 *   Check for a special filename 
 */
enum os_specfile_t os_is_special_file(const char *fname)
{
    /* check for '.' */
    if (strcmp(fname, ".") == 0)
        return OS_SPECFILE_SELF;

    /* check for '..' */
    if (strcmp(fname, "..") == 0)
        return OS_SPECFILE_PARENT;

    /* not a special file */
    return OS_SPECFILE_NONE;
}

/*
 *   Build a full path name given a path and a filename
 *   
 *   Copied over from osnoui.c by Suzanne Skinner (tril@igs.net), 2000/Aug/18 
 */
void os_build_full_path(char *fullpathbuf, size_t fullpathbuflen,
                        const char *path, const char *filename)
{
    size_t plen, flen;
    int add_sep;

    /*
     *   Note whether we need to add a separator.  If the path prefix ends
     *   in a separator, don't add another; otherwise, add the standard
     *   system separator character.
     *   
     *   Do not add a separator if the path is completely empty, since this
     *   simply means that we want to use the current directory.  
     */
    plen = strlen(path);
    add_sep = (plen != 0
               && path[plen-1] != OSPATHCHAR
               && strchr(OSPATHALT, path[plen-1]) == 0);

    /* copy the path to the full path buffer, limiting to the buffer length */
    if (plen > fullpathbuflen - 1)
        plen = fullpathbuflen - 1;
    memcpy(fullpathbuf, path, plen);

    /* add the path separator if necessary (and if there's room) */
    if (add_sep && plen + 2 < fullpathbuflen)
        fullpathbuf[plen++] = OSPATHCHAR;

    /* add the filename after the path, if there's room */
    flen = strlen(filename);
    if (flen > fullpathbuflen - plen - 1)
        flen = fullpathbuflen - plen - 1;
    memcpy(fullpathbuf + plen, filename, flen);

    /* add a null terminator */
    fullpathbuf[plen + flen] = '\0';
}

/*
 *   Determine if a path is absolute or relative.  If the path starts with a
 *   path separator character, we consider it absolute, otherwise we
 *   consider it relative.
 *   
 *   Note that, on DOS, an absolute path can also follow a drive letter.
 *   So, if the path contains a letter followed by a colon, we'll consider
 *   the path to be absolute.
 *   
 *   Copied over from osnoui.c by Suzanne Skinner (tril@igs.net),
 *   2000/Aug/18 
 */
int os_is_file_absolute(const char *fname)
{
    /* if the name starts with a path separator, it's absolute */
    if (fname[0] == OSPATHCHAR || strchr(OSPATHALT, fname[0])  != 0)
        return TRUE;

#ifdef MSDOS
    /* on DOS, a file is absolute if it starts with a drive letter */
    if (isalpha(fname[0]) && fname[1] == ':')
        return TRUE;
#endif /* MSDOS */

    /* the path is relative */
    return FALSE;
}

/*
 *   Extract the path from a filename
 *   
 *   Copied over from osnoui.c by tril@igs.net, 2000/Aug/18 
 */
void os_get_path_name(char *pathbuf, size_t pathbuflen, const char *fname)
{
    const char *lastsep;
    const char *p;
    size_t len;

    /* find the last separator in the filename */
    for (p = fname, lastsep = fname ; *p != '\0' ; ++p)
    {
        /*
         *   if it's a path separator character, remember it as the last one
         *   we've found so far 
         */
        if (*p == OSPATHCHAR || strchr(OSPATHALT, *p)  != 0)
            lastsep = p;
    }

    /* get the length of the prefix, not including the separator */
    len = lastsep - fname;

    /* make sure it fits in our buffer (with a null terminator) */
    if (len > pathbuflen - 1)
        len = pathbuflen - 1;

    /* copy it and null-terminate it */
    memcpy(pathbuf, fname, len);
    pathbuf[len] = '\0';
}

/*
 *   Get the name of the character mapping file.
 *   
 *   NOTE - we should use some means to find the actual character set that
 *   the user's terminal is using and return that; maybe some environment
 *   variable or some curses call will tell us what we need to know.  For
 *   now, we'll just return "asc7dflt", which is the 7-bit ASCII character
 *   mapping; this should be safe for any ASCII terminal, although it won't
 *   allow us to display accented characters that the user's terminal might
 *   actually be capable of displaying if we just knew the actual character
 *   set it was using.  
 */
void os_get_charmap(char *mapname, int charmap_id)
{
    strcpy(mapname, "asc7dflt");
}

/*
 *   Reallocate storage at a different size
 */
void *osrealloc(void *buf, size_t len)
{
    return realloc(buf, len);
}

/* Full ansi color is not yet implemented in Unix, but we do handle bold text
 * and status-line attributes.
 */
int ossgetcolor(int fg, int bg, int attrs, int screen_color)
{
    #ifndef USE_STDIO

    /* use the screen color if the background is transparent */
    if (bg == OSGEN_COLOR_TRANSPARENT)
        bg = screen_color;

    /* Check for the special statusline color scheme */
    if (fg == OSGEN_COLOR_STATUSLINE && bg == OSGEN_COLOR_STATUSBG)
        return sdesc_color;
    /* check for bold text */
    else if ((attrs & OS_ATTR_HILITE) != 0)
        return text_bold_color;

    #endif

    return cNORMAL;
}

extern int os_f_plain;

int oss_get_sysinfo(int code, void *param, long *result)
{
    switch (code) {

    case SYSINFO_TEXT_COLORS:
        *result = SYSINFO_TXC_PARAM;
        return TRUE;

    case SYSINFO_TEXT_HILITE:
#ifdef USE_STDIO
        *result = 0;
#else
        *result = (os_f_plain ? 0 : 1);
#endif
        return TRUE;
    }

    return FALSE;
}

/* Get a file's mode/type.  This returns the same information as
 * the 'mode' member of os_file_stat_t from os_file_stat(), so we
 * simply call that routine and return the value.
 */
int
osfmode( const char *fname, int follow_links, unsigned long *mode,
         unsigned long* attr )
{
    os_file_stat_t s;
    int ok;
    if ((ok = os_file_stat(fname, follow_links, &s)) != 0) {
        if (mode != NULL)
            *mode = s.mode;
        if (attr != NULL)
            *attr = s.attrs;
    }
    return ok;
}

/* Get full stat() information on a file.
 */
int
os_file_stat( const char *fname, int follow_links, os_file_stat_t *s )
{
    struct stat buf;
    if ((follow_links ? stat(fname, &buf) : lstat(fname, &buf)) != 0)
        return 0;

    s->sizelo = (uint32_t)(buf.st_size & 0xFFFFFFFF);
    s->sizehi = sizeof(buf.st_size) > 4
                ? (uint32_t)((buf.st_size >> 32) & 0xFFFFFFFF)
                : 0;
    s->cre_time = buf.st_ctime;
    s->mod_time = buf.st_mtime;
    s->acc_time = buf.st_atime;
    s->mode = buf.st_mode;
    s->attrs = 0;

    if (os_get_root_name(fname)[0] == '.') {
        s->attrs |= OSFATTR_HIDDEN;
    }

    // If we're the owner, check if we have read/write access.
    if (geteuid() == buf.st_uid) {
        if (buf.st_mode & S_IRUSR)
            s->attrs |= OSFATTR_READ;
        if (buf.st_mode & S_IWUSR)
            s->attrs |= OSFATTR_WRITE;
        return 1;
    }

    // Check if one of our groups matches the file's group and if so, check
    // for read/write access.

    // Also reserve a spot for the effective group ID, which might
    // not be included in the list in our next call.
    int grpSize = getgroups(0, NULL) + 1;
    // Paranoia.
    if (grpSize > NGROUPS_MAX || grpSize < 0)
        return 0;

	gid_t* groups = malloc(grpSize*sizeof(gid_t));
    if (getgroups(grpSize - 1, groups + 1) < 0) {
        return 0;
    }
    groups[0] = getegid();
    int i;
    for (i = 0; i < grpSize && buf.st_gid != groups[i]; ++i)
        ;
	free(groups);
    if (i < grpSize) {
        if (buf.st_mode & S_IRGRP)
            s->attrs |= OSFATTR_READ;
        if (buf.st_mode & S_IWGRP)
            s->attrs |= OSFATTR_WRITE;
        return 1;
    }

    // We're neither the owner of the file nor do we belong to its
    // group.  Check whether the file is world readable/writable.
    if (buf.st_mode & S_IROTH)
        s->attrs |= OSFATTR_READ;
    if (buf.st_mode & S_IWOTH)
        s->attrs |= OSFATTR_WRITE;
    return 1;
}

/* Open a directory search.
 */
int os_open_dir(const char *dirname, osdirhdl_t *hdl)
{
    return (*hdl = opendir(dirname)) != NULL;
}

/* Read the next result in a directory search.
 */
int os_read_dir(osdirhdl_t hdl, char *buf, size_t buflen)
{
    // Read the next directory entry - if we've exhausted the search,
    // return failure.
    struct dirent *d = readdir(hdl);
    if (d == 0)
        return 0;

    // return this entry
    safe_strcpy(buf, buflen, d->d_name);
    return 1;
}

/* Close a directory search.
 */
void os_close_dir(osdirhdl_t hdl)
{
    closedir(hdl);
}

void os_gen_rand_bytes( unsigned char* buf, size_t len )
{
    int f = open("/dev/urandom", O_RDONLY);
    read(f, (void*)buf, len);
    close(f);
}

static void
canonicalize_path(char* path)
{
    char* p;
    char* start;

    /* keep going until we're done */
    for (start = p = path; ; ++p)
    {
        /* if it's a separator, note it and process the path element */
        if (*p == '\\' || *p == '/' || *p == '\0')
        {
            /*
             *   check the path element that's ending here to see if it's a
             *   relative item - either "." or ".."
             */
            if (p - start == 1 && *start == '.')
            {
                /*
                 *   we have a '.' element - simply remove it along with the
                 *   path separator that follows
                 */
                if (*p == '\\' || *p == '/')
                    memmove(start, p + 1, strlen(p + 1) + 1);
                else if (start > path)
                    *(start - 1) = '\0';
                else
                    *start = '\0';
            }
            else if (p - start == 2 && *start == '.' && *(start + 1) == '.')
            {
                char* prv;

                /*
                 *   we have a '..' element - find the previous path element,
                 *   if any, and remove it, along with the '..' and the
                 *   subsequent separator
                 */
                for (prv = start;
                    prv > path && (*(prv - 1) != '\\' || *(prv - 1) == '/');
                    --prv);

                /* if we found a separator, remove the previous element */
                if (prv > start)
                {
                    if (*p == '\\' || *p == '/')
                        memmove(prv, p + 1, strlen(p + 1) + 1);
                    else if (start > path)
                        *(start - 1) = '\0';
                    else
                        *start = '\0';
                }
            }

            /* note the start of the next element */
            start = p + 1;
        }

        /* stop at the end of the string */
        if (*p == '\0')
            break;
    }
}

int
os_get_abs_filename(char* buf, size_t buflen, const char* filename)
{
    // If the filename is already absolute, copy it; otherwise combine
    // it with the working directory.
    if (os_is_file_absolute(filename))
    {
        // absolute - copy it as-is
        safe_strcpy(buf, buflen, filename);
    }
    else
    {
        // combine it with the working directory to get the path
        char pwd[OSFNMAX];
        if (getcwd(pwd, sizeof(pwd)) != 0)
            os_build_full_path(buf, buflen, pwd, filename);
        else
            safe_strcpy(buf, buflen, filename);
    }

    // canonicalize the result
    canonicalize_path(buf);

    // Try getting the canonical path from the OS (allocating the
    // result buffer).
    const char* newpath = realpath(filename, NULL);
    if (newpath) {
        // Copy the output (truncating if it's too long).
        safe_strcpy(buf, buflen, newpath);
    }

    // realpath() might have failed, but that's okay - realpath() only works
    // if the path refers to an existing file, but it's valid for callers to
    // pass non-existent filenames, such as names of files they're about to
    // create, or hypothetical paths being used for comparison purposes or
    // for future use.  Simply return the canonical path name we generated
    // above.
    free(newpath);
    return 1;
}

int
os_rmdir(const char* dir)
{
    return rmdir(dir) == 0;
}

int
os_mkdir(const char* dir, int create_parents)
{
    //assert(dir != 0);

    if (dir[0] == '\0')
        return 1;

    // Copy the directory name to a new string so we can strip any trailing
    // path seperators.
    size_t len = strlen(dir);
    char* tmp = malloc(len + 1);
    strncpy(tmp, dir, len);
    while (tmp[len - 1] == OSPATHCHAR)
        --len;
    tmp[len] = '\0';

    // If we're creating intermediate diretories, and the path contains
    // multiple elements, recursively create the parent directories first.
    if (create_parents && strchr(tmp, OSPATHCHAR) != 0) {
        char par[OSFNMAX];

        // Extract the parent path.
        os_get_path_name(par, sizeof(par), tmp);
        free(tmp);

        // If the parent doesn't already exist, create it recursively.
        if (osfacc(par) != 0 && !os_mkdir(par, 1)) {
            return 0;
        }
    }

    // Create the directory.
    int ret = mkdir(tmp, S_IRWXU | S_IRWXG | S_IRWXO) == 0;
    free(tmp);
    return ret;
}

osfildef*
osfdup(osfildef* orig, const char* mode)
{
    char realmode[5];
    char* p = realmode;
    const char* m;

    /* verify that there aren't any unrecognized mode flags */
    for (m = mode; *m != '\0'; ++m)
    {
        if (strchr("rw+bst", *m) == 0)
            return 0;
    }

    /* figure the read/write mode - translate r+ and w+ to r+ */
    if ((mode[0] == 'r' || mode[0] == 'w') && mode[1] == '+')
        *p++ = 'r', *p++ = '+';
    else if (mode[0] == 'r')
        *p++ = 'r';
    else if (mode[0] == 'w')
        *p++ = 'w';
    else
        return 0;

    /* end the mode string */
    *p = '\0';

    /* duplicate the handle in the given mode */
    return fdopen(dup(fileno(orig)), mode);
}

static void
resolve_path(char* buf, size_t buflen, const char* path)
{
    // Starting with the full path string, try resolving the path with
    // realpath().  The tricky bit is that realpath() will fail if any
    // component of the path doesn't exist, but we need to resolve paths
    // for prospective filenames, such as files or directories we're
    // about to create.  So if realpath() fails, remove the last path
    // component and try again with the remainder.  Repeat until we
    // can resolve a real path, or run out of components to remove.
    // The point of this algorithm is that it will resolve as much of
    // the path as actually exists in the file system, ensuring that
    // we resolve any links that affect the path.  Any portion of the
    // path that doesn't exist obviously can't refer to a link, so it
    // will be taken literally.  Once we've resolved the longest prefix,
    // tack the stripped portion back on to form the fully resolved
    // path.

    // make a writable copy of the path to work with
    size_t pathl = strlen(path);
    char* mypath = malloc(pathl + 1);
    memcpy(mypath, path, pathl + 1);

    // start at the very end of the path, with no stripped suffix yet
    char* suffix = mypath + pathl;
    char sl = '\0';

    // keep going until we resolve something or run out of path
    for (;;)
    {
        // resolve the current prefix, allocating the result
        char* rpath = realpath(mypath, 0);

        // un-split the path
        *suffix = sl;

        // if we resolved the prefix, return the result
        if (rpath != 0)
        {
            // success - if we separated a suffix, reattach it
            if (*suffix != '\0')
            {
                // reattach the suffix (the part after the '/')
                for (; *suffix == '/'; ++suffix);
                os_build_full_path(buf, buflen, rpath, suffix);
            }
            else
            {
                // no suffix, so we resolved the entire path
                safe_strcpy(buf, buflen, rpath);
            }

            // done with the resolved path
            free(rpath);

            // ...and done searching
            break;
        }

        // no luck with realpath(); search for the '/' at the end of the
        // previous component in the path 
        for (; suffix > mypath && *(suffix - 1) != '/'; --suffix);

        // skip any redundant slashes
        for (; suffix > mypath && *(suffix - 1) == '/'; --suffix);

        // if we're at the root element, we're out of path elements
        if (suffix == mypath)
        {
            // we can't resolve any part of the path, so just return the
            // original path unchanged
            safe_strcpy(buf, buflen, mypath);
            break;
        }

        // split the path here into prefix and suffix, and try again
        sl = *suffix;
        *suffix = '\0';
    }

    // done with our writable copy of the path
    free(mypath);
}

int
os_is_file_in_dir(const char* filename, const char* path,
    int allow_subdirs, int match_self)
{
    char filename_buf[OSFNMAX], path_buf[OSFNMAX];
    size_t flen, plen;

    // Absolute-ize the filename, if necessary.
    if (!os_is_file_absolute(filename)) {
        os_get_abs_filename(filename_buf, sizeof(filename_buf), filename);
        filename = filename_buf;
    }

    // Absolute-ize the path, if necessary.
    if (!os_is_file_absolute(path)) {
        os_get_abs_filename(path_buf, sizeof(path_buf), path);
        path = path_buf;
    }

    // Canonicalize the paths, to remove .. and . elements - this will make
    // it possible to directly compare the path strings.  Also resolve it
    // to the extent possible, to make sure we're not fooled by symbolic
    // links.
    safe_strcpy(filename_buf, sizeof(filename_buf), filename);
    canonicalize_path(filename_buf);
    resolve_path(filename_buf, sizeof(filename_buf), filename_buf);
    filename = filename_buf;

    safe_strcpy(path_buf, sizeof(path_buf), path);
    canonicalize_path(path_buf);
    resolve_path(path_buf, sizeof(path_buf), path_buf);
    path = path_buf;

    // Get the length of the filename and the length of the path.
    flen = strlen(filename);
    plen = strlen(path);

    // If the path ends in a separator character, ignore that.
    if (plen > 0 && path[plen - 1] == OSPATHCHAR)
        --plen;

    // if the names match, return true if and only if we're matching the
    // directory to itself
    if (plen == flen && memcmp(filename, path, flen) == 0)
        return match_self;

    // Check that the filename has 'path' as its path prefix.  First, check
    // that the leading substring of the filename matches 'path', ignoring
    // case.  Note that we need the filename to be at least two characters
    // longer than the path: it must have a path separator after the path
    // name, and at least one character for a filename past that.
    if (flen < plen + 2 || memcmp(filename, path, plen) != 0)
        return 0;

    // Okay, 'path' is the leading substring of 'filename'; next make sure
    // that this prefix actually ends at a path separator character in the
    // filename.  (This is necessary so that we don't confuse "c:\a\b.txt"
    // as matching "c:\abc\d.txt" - if we only matched the "c:\a" prefix,
    // we'd miss the fact that the file is actually in directory "c:\abc",
    // not "c:\a".)
    if (filename[plen] != OSPATHCHAR)
        return 0;

    // We're good on the path prefix - we definitely have a file that's
    // within the 'path' directory or one of its subdirectories.  If we're
    // allowed to match on subdirectories, we already have our answer
    // (true).  If we're not allowed to match subdirectories, we still have
    // one more check, which is that the rest of the filename is free of
    // path separator charactres.  If it is, we have a file that's directly
    // in the 'path' directory; otherwise it's in a subdirectory of 'path'
    // and thus isn't a match.
    if (allow_subdirs) {
        // Filename is in the 'path' directory or one of its
        // subdirectories, and we're allowed to match on subdirectories, so
        // we have a match.
        return 1;
    }

    // We're not allowed to match subdirectories, so scan the rest of
    // the filename for path separators.  If we find any, the file is
    // in a subdirectory of 'path' rather than directly in 'path'
    // itself, so it's not a match.  If we don't find any separators,
    // we have a file directly in 'path', so it's a match.
    const char* p;
    for (p = filename; *p != '\0' && *p != OSPATHCHAR; ++p)
        ;

    // If we reached the end of the string without finding a path
    // separator character, it's a match .
    return *p == '\0';
}

int
os_resolve_symlink(const char* fname, char* target, size_t target_size)
{
    // get the stat() information for the *undereferenced* link; if
    // it's not actually a link, there's nothing to resolve
    struct stat buf;
    if (lstat(fname, &buf) != 0 || (buf.st_mode & S_IFLNK) == 0)
        return 0;

    // read the link contents (maxing out at the buffer size)
    size_t copylen = (size_t)buf.st_size;
    if (copylen > target_size - 1)
        copylen = target_size - 1;
    if (readlink(fname, target, copylen) < 0)
        return 0;

    // null-terminate the result and return success
    target[copylen] = '\0';
    return 1;
}

size_t os_get_root_dirs(char* buf, size_t buflen)
{
    // TODO: Emglken
    static const char ret[] = { '/', 0, 0 };

    // if there's room, copy the root string "/" and an extra null
    // terminator for the overall list
    if (buflen >= sizeof(ret))
        memcpy(buf, ret, sizeof(ret));

    // return the required size
    return sizeof(ret);
}

void os_combine_paths(char* fullpathbuf, size_t fullpathbuflen,
    const char* path, const char* filename)
{
    snprintf(fullpathbuf, fullpathbuflen, "%s/%s", path, filename);
}

void
os_time_ns(os_time_t* seconds, long* nanoseconds)
{
    // Get the current time.
    static const clockid_t clockType = CLOCK_REALTIME;
    struct timespec currTime;
    clock_gettime(clockType, &currTime);

    // return the data
    *seconds = currTime.tv_sec;
    *nanoseconds = currTime.tv_nsec;
}

int
oss_eof_on_stdin()
{
    fd_set rfds;
    struct timeval tv;

    // Zero time: non-blocking operation.
    tv.tv_sec = 0;
    tv.tv_usec = 0;

    FD_ZERO(&rfds);
    // stdin always has a file descriptor of 0.
    FD_SET(0, &rfds);

    return select(1, &rfds, 0, 0, &tv) == -1;
}

#ifndef USE_STDIO

int os_askfile(const char* prompt, char* reply, int replen,
    int /*dialog_type*/, os_filetype_t /*file_type*/)
{
    /* show the prompt */
    os_printz(prompt);
    os_printz(" >");

    /* ask for the filename */
    os_gets((unsigned char*)reply, replen);

    /*
     *   if they entered an empty line, return "cancel"; otherwise, return
     *   success
     */
    return (reply[0] == '\0' ? OS_AFE_CANCEL : OS_AFE_SUCCESS);
}
#endif

#if defined(USE_STDIO) || defined(USE_HTML)
void os_set_save_ext(const char* ext)
{
    /* ignore the setting */
}

#endif
