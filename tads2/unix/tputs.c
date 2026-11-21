#ifndef	DJGCC_386

/*
 * Outputting a string with padding.
 */
#include "os.h"

short ospeed;
char PC;

#ifndef	USE_STDIO

/* Actual baud rate if positive; - baud rate / 100 if negative.  */

static int speeds[] = {
#ifdef VMS
    0, 50, 75, 110, 134, 150, -3, -6, -12, -18,
    -20, -24, -36, -48, -72, -96, -192
#else
    0, 50, 75, 110, 135, 150, -2, -3, -6, -12,
    -18, -24, -48, -96, -192, -384
#endif
};

void
Tputs(string, nlines, outfun)
     char *string;
     int nlines;
     int (*outfun) ();
{
	tputs(string, nlines, outfun);
}

#endif

#if 0

void
Tputs(string, nlines, outfun)
     char *string;
     int nlines;
     int (*outfun) ();
{
  register int padcount = 0;

  if (string == (char *) 0)
    return;

  while (*string >= '0' && *string <= '9')
    {
      padcount += *string++ - '0';
      padcount *= 10;
    }
  if (*string == '.')
    {
      string++;
      padcount += *string++ - '0';
    }
  if (*string == '*')
    {
      string++;
      padcount *= nlines;
    }
  while (*string)
    (*outfun) (*string++);

  /* padcount is now in units of tenths of msec.  */
  padcount *= speeds[ospeed];
  padcount += 500;
  padcount /= 1000;
  if (speeds[ospeed] < 0)
    padcount = -padcount;
  else
    {
      padcount += 50;
      padcount /= 100;
    }

  while (padcount-- > 0)
    (*outfun) (PC);
}
#endif

#endif	/* DJGCC_386 */
