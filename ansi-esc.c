/*
 * Copyright (C) 1996  Ben Fowler
 * Copyright (C) 2003  Pekka Enberg <penberg@cs.helsinki.fi>
 *
 * Contents of this file originated from Linux sources sometime late
 * 1995.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 */

#include "ansi-esc.h"

/*
 * Global variables used by the ANSI escape code translator. The list
 * is long; please feel free to send patches to Walt ;-).
 */
static int fore;
static int back;
static int need_wrap;
static int def_color;
static int par[8];
static int intensity;
static int underline;
static int blink;
static int reverse;
static int saved_x;
static int saved_y;
static int s_intensity;
static int s_underline;
static int s_blink;
static int s_reverse;
static int s_color;
static int npar;
static int y;
static int x;
static int color;
static int pos;
static int vc_state;
static int ques;
static int attr;
static int tab_stop[5];


void
default_attr ()
{
    intensity = 1;
    underline = 0;
    reverse = 0;
    blink = 0;
    color = 7;
    fore = 7;
    back = 0;
}


void
update_attr ()
{
    attr = color;

    if (blink)
        attr ^= 0x80;

    if (intensity == 2)
        attr ^= 0x08;
}


void
csi_m ()
{
    int i;

    for (i = 0; i <= npar; i++)
        switch (par[i])
            {
            case 0:			/* all attributes off */
                default_attr ();
                break;
            case 1:
                intensity = 2;
                break;
            case 2:
                intensity = 0;
                break;
            case 4:
                underline = 1;
                break;
            case 5:
                blink = 1;
                break;
            case 7:
                reverse = 1;
                break;
            case 10:			/* ANSI X3.64-1979 (SCO-ish?)
                                 * Select primary font, don't display
                                 * control chars if defined, don't set
                                 * bit 8 on output.
                                 */
                break;
            case 11:			/* ANSI X3.64-1979 (SCO-ish?)
                                 * Select first alternate font, let's
                                 * chars < 32 be displayed as ROM chars.
                                 */
                break;
            case 12:			/* ANSI X3.64-1979 (SCO-ish?)
                                 * Select second alternate font, toggle
                                 * high bit before displaying as ROM char.
                                 */
                break;
            case 21:
            case 22:
                intensity = 1;
                break;
            case 24:
                underline = 0;
                break;
            case 25:
                blink = 0;
                break;
            case 27:
                reverse = 0;
                break;
            case 38:			/* ANSI X3.64-1979 (SCO-ish?)
                                 * Enables underscore, white foreground
                                 * with white underscore (Linux - use
                                 * default foreground).
                                 */
                color = (def_color & 0x0f) | back;
                underline = 1;
                break;
            case 39:			/* ANSI X3.64-1979 (SCO-ish?)
                                 * Disable underline option.
                                 * Reset colour to default? It did this
                                 * before...
                                 */
                color = (def_color & 0x0f) | back;
                underline = 0;
                break;
            case 49:
                color = (def_color & 0xf0) | fore;
                break;
            default:
                if (par[i] >= 30 && par[i] <= 37)
                    {
                        fore  = (par[i] - 30) & 0x07;
                        color = (back << 4) | fore;
                    }
                else if (par[i] >= 40 && par[i] <= 47)
                    {
                        back  = (par[i] - 40) & 0x07;
                        color = (back << 4) | fore;
                    }
                break;
            }
    update_attr ();
}


void
gotoxy (int new_x, int new_y, unsigned long width, unsigned long height)
{
    if (new_x < 0)
        x = 0;
    else if (new_x >= width)
        x = width - 1;
    else
        x = new_x;

    if (new_y < 0)
        y = 0;
    else if (new_y >= height)
        y = height - 1;
    else
        y = new_y;

    pos = (y * width) + x;
    need_wrap = 0;
}


void
save_cur ()
{
    saved_x = x;
    saved_y = y;
    s_intensity = intensity;
    s_underline = underline;
    s_blink = blink;
    s_reverse = reverse;
    s_color = color;
}


void
restore_cur (unsigned long width, unsigned long height)
{

    gotoxy (saved_x, saved_y, width, height);
    intensity = s_intensity;
    underline = s_underline;
    blink = s_blink;
    reverse = s_reverse;
    color = s_color;
    update_attr ();
    need_wrap = 0;
}


static void
lf (unsigned long width, unsigned long height)
{
    if (y < height - 1)
        {
            y++;
            pos += width;
        }
    need_wrap = 0;
}


void
cr ()
{
    pos -= x;
    need_wrap = 0;
    x = 0;
}


/* DOS end-of-file character.  */
#define DOS_EOF 26

unsigned long
ansi_esc_translate (FILE * fp, unsigned int * editbuffer,
                    unsigned long width, unsigned long height)
{
    int c, ok;

    /*
     * Some of these initial values might not be needed but right now
     * it's better to be safe than sorry. -Pekka, 2003-01-11
     */
    y = 0;
    x = 0;
    intensity = 1;
    underline = 0;
    blink = 0;
    reverse = 0;
    saved_x = 0;
    saved_y = 0;
    s_intensity = 0;
    s_underline = 0;
    s_blink = 0;
    s_reverse = 0;
    s_color = 0;
    color = 0;
    npar = 0;
    need_wrap = 0;
    def_color = 0x07;
    pos = 0;
    fore = 7;
    back = 0;
    vc_state = ESnormal;
    ques = 0;
    attr = 7;

    tab_stop[0] = 0x01010100;
    tab_stop[1] = 0x01010101;
    tab_stop[2] = 0x01010101;
    tab_stop[3] = 0x01010101;
    tab_stop[4] = 0x01010101;

    /* Read file, translate escape codes to color codes, and store them
       into an array.  */
    while (((c = getc (fp)) != -1) && (c != DOS_EOF) && (pos < width * height))
        {
            ok = (c >= 32);	/* less than ' ' ignored */

            if ((vc_state == ESnormal) && ok)
                {
                    if (need_wrap)
                        {
                            cr ();
                            lf (width, height);
                        }
                    editbuffer[pos] = ((attr << 8) + c);
                    if (x == width - 1)
                        need_wrap = 1;	/* decawm; */
                    else
                        {
                            x++;
                            pos++;
                        }
                    continue;
                }
            switch (c)
                {
                case 7:
                    continue;
                case 8:
                    continue;
                case 9:
                    pos -= x;
                    while (x < width - 1)
                        {
                            x++;
                            if (tab_stop[x >> 5] & (1 << (x & 31)))
                                break;
                        }
                    pos += x;
                    continue;
                case 10:
                case 11:
                case 12:
                    cr ();
                    lf (width, height);
                    continue;
                case 13:
                    continue;
                case 14:
                    continue;
                case 15:
                    continue;
                case 24:
                case 26:
                    vc_state = ESnormal;
                    continue;
                case 27:
                    vc_state = ESesc;
                    continue;
                case 127:
                    continue;
                case 128 + 27:
                    vc_state = ESsquare;
                    continue;
                }

            switch (vc_state)
                {
                case ESesc:
                    vc_state = ESnormal;
                    switch (c)
                        {
                        case '[':
                            vc_state = ESsquare;
                            continue;
                        case ']':
                            vc_state = ESnonstd;
                            continue;
                        case '%':
                            vc_state = ESpercent;
                            continue;
                        case 'E':
                            cr ();
                            lf (width, height);
                            continue;
                        case 'M':
                            /* nothing */
                            continue;
                        case 'D':
                            lf (width, height);
                            continue;
                        case 'H':
                            continue;
                        case 'Z':
                            continue;
                        case '7':
                            save_cur ();
                            continue;
                        case '8':
                            restore_cur (width, height);
                            continue;
                        case '(':
                            vc_state = ESsetG0;
                            continue;
                        case ')':
                            vc_state = ESsetG1;
                            continue;
                        case '#':
                            vc_state = EShash;
                            continue;
                        case 'c':
                            continue;
                        case '>':	/* Numeric keypad */
                            continue;
                        case '=':	/* Appl. keypad */
                            continue;
                        }
                    continue;
                case ESnonstd:
                    if (c == 'P')
                        {		/* palette escape sequence */
                            for (npar = 0; npar < 7; npar++)
                                par[npar] = 0;
                            npar = 0;
                            vc_state = ESpalette;
                            continue;
                        }
                    else if (c == 'R')
                        {		/* reset palette */
                            vc_state = ESnormal;
                        }
                    else
                        vc_state = ESnormal;
                    continue;
                case ESpalette:
                    if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F')
                        || (c >= 'a' && c <= 'f'))
                        {
                            par[npar++] = (c > '9' ? (c & 0xDF) - 'A' + 10 : c - '0');
                        }
                    else
                        vc_state = ESnormal;
                    continue;
                case ESsquare:
                    for (npar = 0; npar < 7; npar++)
                        par[npar] = 0;
                    npar = 0;
                    vc_state = ESgetpars;
                    if (c == '[')
                        {		/* Function key */
                            vc_state = ESfunckey;
                            continue;
                        }
                    ques = (c == '?');
                    if (ques)
                        continue;
                case ESgetpars:
                    if (c == ';' && npar < 6)
                        {
                            npar++;
                            continue;
                        }
                    else if (c >= '0' && c <= '9')
                        {
                            par[npar] *= 10;
                            par[npar] += c - '0';
                            continue;
                        }
                    else
                        vc_state = ESgotpars;
                case ESgotpars:
                    vc_state = ESnormal;
                    switch (c)
                        {
                        case 'h':
                            continue;
                        case 'l':
                            continue;
                        case 'n':
                            continue;
                        }
                    if (ques)
                        {
                            ques = 0;
                            continue;
                        }
                    switch (c)
                        {
                        case 'G':
                        case '`':
                            if (par[0])
                                par[0]--;
                            gotoxy (par[0], y, width, height);
                            continue;
                        case 'A':
                            if (!par[0])
                                par[0]++;
                            gotoxy (x, y - par[0], width, height);
                            continue;
                        case 'B':
                        case 'e':
                            if (!par[0])
                                par[0]++;
                            gotoxy (x, y + par[0], width, height);
                            continue;
                        case 'C':
                        case 'a':
                            if (!par[0])
                                par[0]++;
                            gotoxy (x + par[0], y, width, height);
                            continue;
                        case 'D':
                            if (!par[0])
                                par[0]++;
                            gotoxy (x - par[0], y, width, height);
                            continue;
                        case 'E':
                            if (!par[0])
                                par[0]++;
                            gotoxy (0, y + par[0], width, height);
                            continue;
                        case 'F':
                            if (!par[0])
                                par[0]++;
                            gotoxy (0, y - par[0], width, height);
                            continue;
                        case 'd':
                            if (par[0])
                                par[0]--;
                            gotoxy (x, par[0], width, height);
                            continue;
                        case 'H':
                        case 'f':
                            if (par[0])
                                par[0]--;
                            if (par[1])
                                par[1]--;
                            gotoxy (par[1], par[0], width, height);
                            continue;
                        case 'J':
                            continue;
                        case 'K':
                            continue;
                        case 'L':
                            continue;
                        case 'M':
                            continue;
                        case 'P':
                            continue;
                        case 'c':
                            continue;
                        case 'g':
                            continue;
                        case 'm':
                            csi_m ();
                            continue;
                        case 'q':	/* DECLL - but only 3 leds */
                            continue;
                        case 'r':
                            continue;
                        case 's':
                            save_cur ();
                            continue;
                        case 'u':
                            restore_cur (width, height);
                            continue;
                        case 'X':
                            continue;
                        case '@':
                            continue;
                        case ']':	/* setterm functions */
                            continue;
                        }
                    continue;
                case ESpercent:
                    vc_state = ESnormal;
                    continue;
                case ESfunckey:
                    vc_state = ESnormal;
                    continue;
                case EShash:
                    vc_state = ESnormal;
                    continue;
                case ESsetG0:
                    vc_state = ESnormal;
                    continue;
                case ESsetG1:
                    vc_state = ESnormal;
                    continue;
                default:
                    vc_state = ESnormal;
                }
        }
    return pos; /* return number of bytes stored in buffer.  */
}
