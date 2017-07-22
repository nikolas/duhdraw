#ifndef _ANSI_ESC_H_
#define _ANSI_ESC_H_ 1

#include <stdio.h>

enum
{
  ESnormal, ESesc, ESsquare, ESgetpars, ESgotpars, ESfunckey,
  EShash, ESsetG0, ESsetG1, ESpercent, ESignore, ESnonstd,
  ESpalette
};

/*
 * Returns number of bytes stored in buffer.
 */
unsigned long ansi_esc_translate (FILE * fp, unsigned int * editbuffer,
				  unsigned long width, unsigned long height);

#endif
