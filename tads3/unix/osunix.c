/*
Name
  osunix.c - extra unix support routines for TADS 3
Function
  Defines some osifc functions that are used only by TADS 3, and thus
  are not needed as part of the generic TADS 2 osifc implementation.

  This file is common to all TADS 3 programs for Unix.

  -- tril@igs.net
Notes
  
Modified
  08/19/00 MJRoberts  - Creation
*/

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include "lib.h"

/* Dummy break handler routine */
void os_instbrk(int install){}

/* 
 *   Get the executable's full pathname given argv[0] 
 */
int os_get_exe_filename(char *buf, size_t buflen, const char *argv0)
{
    const char *fn;

    for (fn = argv0+strlen(argv0)-1;
         fn > argv0 && *fn != '/';
         fn--) {}
         if (*fn == '/')
             fn++;
         if (strlen(BINDIR) + strlen(fn) + 1 >= buflen)
             return FALSE;
         sprintf(buf, "%s/%s", BINDIR, fn);
         return TRUE;
}

/*
 *   Get a special path (e.g. path to standard include files or libraries).
 *   Valid id values include OS_GSP_T3_RES, OS_GSP_T3_LIB, and
 *   OS_GSP_T3_INC.
 */
void os_get_special_path(char *buf, size_t buflen, const char *argv0, int id)
{
    const char *builtin, *env, *str;

    switch(id) {
    case OS_GSP_T3_RES:
        builtin = RESDIR;
        env = getenv("T3_RESDIR");
        break;
    case OS_GSP_T3_LIB:
        builtin = LIBDIR;
        env = getenv("T3_LIBDIR");
        break;
    case OS_GSP_T3_INC:
        builtin = INCDIR;
        env = getenv("T3_INCDIR");
        break;
    case OS_GSP_T3_USER_LIBS:
        builtin = "";
        env = getenv("T3_USERLIBDIR");
        break;
	case OS_GSP_T3_SYSCONFIG:
        builtin = "config";
        env = getenv("T3_SYSCONFIG");
        break;
	case OS_GSP_LOGFILE:
        builtin = "log.txt";
        env = getenv("LOGFILE");
        break;
	case OS_GSP_T3_APP_DATA:
        builtin = RESDIR;
        env = getenv("T3_APPDATA");
        break;
    default:
        /*
         *   If we're called with another identifier, it must mean that
         *   we're out of date.  Fail with an assertion.
         */
        assert(FALSE);
    }

    if (env && env[0] != '\0')
        str = env;
    else
        str = builtin;
    if (strlen(str) >= buflen)
        assert(FALSE);
    strcpy(buf, str);
}

/*
 * Dummy implementations of the file-searching routines.  It enables us
 * to build the compiler, but adding resources with t3make won't work.
 * Someone should fix this as soon as possible.
 */
void *os_find_first_file(const char *dir,
                         char *outbuf, size_t outbufsiz, int *isdir,
                         char *outpathbuf, size_t outpathbufsiz)
{
    return NULL;
}

void *os_find_next_file(void *ctx, char *outbuf, size_t outbufsiz,
                        int *isdir, char *outpathbuf, size_t outpathbufsiz)
{
    return NULL;
}

void os_find_close(void *ctx)
{
}
