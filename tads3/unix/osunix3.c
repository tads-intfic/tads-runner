/*
Name
  osunix3.c - extra unix support routines for TADS 3
Function
  Defines some routines for TADS 3 on Unix.  These routines are not
  required by TADS, but rather are required by the Unix osifc
  implementation.  In particular, the Unix osifc implementation makes some
  calls to TADS 2 portable functions, which are not available in TADS 3
  executables; to avoid linking errors, we provide these dummy
  implementations.

  IMPORTANT: This file should be linked only when building a program
  that links in TADS 3 ONLY.  This should NOT be linked when the TADS 2 VM
  is linked in as well.  So, this file should be linked in to essentially
  everything except the combined v2+v3 interpreter.  For anything that
  links in the TADS 2 VM, do NOT link with this file.
Notes

Modified
  08/19/00 MJRoberts  - Creation
*/

#include <stdio.h>
#include <stdlib.h>

#include "voc.h"

/*
 *   The unix OS files require the symbol 'main_voc_ctx', which is the
 *   context structure pointer that the OS routine that intercepts crash
 *   signals passes to fiosav().  This global is irrelevant for tads 3, but
 *   is required for linking the unix OS code, so provide a dummy
 *   definition.  
 */
voccxdef *main_voc_ctx = 0;


/*
 *   The Unix osifc implementation calls fiosav(), the TADS 2 function that
 *   saves the current game, if a crash (such as a SEGV) occurs.  This
 *   function was TADS 2-specific, and is not present in TADS 3; this dummy
 *   implementation provides the symbol to avoid linker errors.  This is a
 *   dummy version - it doesn't do anything; the only loss of functionality
 *   is that the TADS 3 interpreter will not actually save the current game
 *   if the interpreter crashes.  
 */
int fiosav(voccxdef *vctx, char *fname, char *game_fname)
{
    /* indicate failure */
    return 1;
}

