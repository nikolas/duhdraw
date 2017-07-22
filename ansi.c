/*****************************************************************************
 *                          ANSI Viewer for Linux
 *                               Ben Fowler
 *                              December  1995
 *     Derived largely in part from /usr/src/linux/driver/char/console.c
 *
 *
 ****************************************************************************/

#include <stdio.h>
#include <string.h>
#include <curses.h>
#include <stdlib.h>
#include "ansi-esc.h"

static int rows = 25;			/* default terminal lines */
static int cols = 80;			/* default terminal columns */

static int x = 0;
static int y = 0;
static int pos = 0;
static int par[8];
static int splos = 0;

#define EDIT_BUFFER_HEIGHT 1000

static unsigned int * editbuffer;

static int ques = 0;
static int npar = 0;
static int attr = 7;
static int maxread = 0;
static int ishome = 0;
static int lattr;
static int lastline;

static void
clearbuf (unsigned int * buffer, unsigned long size)
{
    int i;

    for (i = 0; i < size; i++)
        {
            buffer[i] = 0x0720;	/* default color ' ' */
        }
}

static void
printattr (void)
{
    int fg;
    int bg;
    int bold;
    int flash;

    flash = 25;
    fg = (attr & 0x07);
    bg = (attr >> 4) & 0x07;
    fg = fg + 30;
    bg = bg + 40;
    bold = 0;
    if (attr & 0x08)
        bold = 1;
    if (attr & 0x80)
        flash = 5;
    printf ("\e[0;%d;%d;%d;%dm", bold, flash, fg, bg);
}

static void
print_line (void)			/* print one line to screen */
{
    int co;
    for (co = 0; co < cols; co++)
        {
            attr = (editbuffer[pos + co] >> 8);
            if (lattr != attr)
                {
                    printattr ();
                    lattr = attr;
                }
            putc (editbuffer[pos + co] & 0xFF, stdout);
        }
}

static void
ansiscrollup (void)
{
    if ((pos - (rows * cols)) >= 0)
        {
            splos = pos;
            pos = pos - (rows * cols);
            printf ("\e[0;0H\e[L\e[0;0H");	/* insert a line */
            print_line ();
            pos = splos - cols;
            ishome = 0;
        }
    else
        ishome = 1;
}

static void
ansiscrolldown (void)
{
    if ((pos + cols) >= maxread)
        return;
    if (lastline < rows)
        return;

    {
        printf ("\e[%d;0H\n", rows);
        pos += cols;
        print_line ();
        ishome = 0;
    }
}

static void
home (void)
{
    int i, zz;
    if (ishome == 1)		/* if already at top skippit */
        return;
    zz = rows;
    if (lastline < rows)
        zz = lastline;
    printf ("\e[2J\e[0;0H");
    for (i = 0; i < zz; i++)
        {
            pos = i * cols;
            print_line ();
        }
    ishome = 1;
}

static void
end (void)
{
    int i, zz;
    if (lastline < rows)
        return;
    if ((pos + cols) >= maxread)	/* if already at end skippit */
        return;

    zz = lastline - rows;
    if (zz < 0)
        zz = 0;
    zz = zz * cols;
    printf ("\e[2J\e[0;0H");
    for (i = 0; i < rows; i++)
        {
            pos = zz + (i * cols);
            print_line ();
        }
    ishome = 0;
}

static void
pagedown (void)
{
    int i;
    i = pos / cols;
    if ((lastline - i) <= rows)
        {
            end ();
        }
    else
        {
            printf ("\e[2J\e[0;0H");
            for (i = 0; i < rows; i++)
                {
                    pos = pos + cols;
                    print_line ();
                }
            ishome = 0;
        }
}

static void
pageup (void)
{
    int i;
    if ((pos - (2 * rows * cols)) >= 0)
        {
            printf ("\e[2J\e[0;0H");
            pos = pos - (2 * rows * cols);
            for (i = 0; i < rows; i++)
                {
                    pos = pos + cols;
                    print_line ();		/* leave it pointing at last line */
                }
        }
    else
        home ();
}

static void
scrollit (void)
{
    int c, ok;
    int ex = 0;
    int scrollstate = ESnormal;

    printf ("\e[?25l");		/* turn cursor off */
    printf ("\e[2J\e[0;0H");	/* clear screen and home cursor */
    printf ("\e(U");		/* set linux virtual console to IBM_PC graphics set */
    ishome = 0;

    lastline = pos / cols;
    c = lastline * cols;
    ok = pos - c;
    if (ok > 0)
        lastline++;
    maxread = pos;
    pos = 0;
    c = 0;
    ok = 0;
    home ();

    while (fflush (stdout) || (((ex != 1) && (c = getchar ()) != 255)))
        {
            switch (c)
                {
                case 7:
                    continue;
                case 8:
                    continue;
                case 9:		/* tab key */
                    continue;
                case 11:
                case 12:
                    continue;
                case 10:
                case 13:
                    ansiscrolldown ();
                    continue;
                case 14:
                    continue;
                case 15:
                    continue;
                case 24:
                case 26:
                    scrollstate = ESnormal;
                    continue;
                case 27:
                    scrollstate = ESesc;
                    continue;
                case 4:
                case 'Q':
                case 'q':
                case 'X':
                case 'x':
                    ex = 1;
                    continue;
                case ' ':
                    pagedown ();
                    continue;
                case 127:
                    continue;
                case 128 + 27:
                    scrollstate = ESsquare;
                    continue;
                }

            switch (scrollstate)
                {
                case ESesc:
                    scrollstate = ESnormal;
                    switch (c)
                        {
                        case '[':
                            scrollstate = ESsquare;
                            continue;
                        case ']':
                            scrollstate = ESnonstd;
                            continue;
                        case '%':
                            scrollstate = ESpercent;
                            continue;
                        case 'E':
                            continue;
                        case 'M':
                            continue;
                        case 'D':
                            continue;
                        case 'H':
                            continue;
                        case 'Z':
                            continue;
                        case '7':
                            continue;
                        case '8':
                            continue;
                        case '(':
                            scrollstate = ESsetG0;
                            continue;
                        case ')':
                            scrollstate = ESsetG1;
                            continue;
                        case '#':
                            scrollstate = EShash;
                            continue;
                        case 'c':
                            continue;
                        case '>':		/* Numeric keypad */
                            continue;
                        case '=':		/* Appl. keypad */
                            continue;
                        }
                    continue;
                case ESnonstd:
                    if (c == 'P')
                        {			/* palette escape sequence */
                            for (npar = 0; npar < 7; npar++)
                                par[npar] = 0;
                            npar = 0;
                            scrollstate = ESpalette;
                            continue;
                        }
                    else if (c == 'R')
                        {			/* reset palette */
                            scrollstate = ESnormal;
                        }
                    else
                        scrollstate = ESnormal;
                    continue;
                case ESpalette:
                    if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F')
                        || (c >= 'a' && c <= 'f'))
                        {
                            par[npar++] = (c > '9' ? (c & 0xDF) - 'A' + 10 : c - '0');
                        }
                    else
                        scrollstate = ESnormal;
                    continue;
                case ESsquare:
                    for (npar = 0; npar < 7; npar++)
                        par[npar] = 0;
                    npar = 0;
                    scrollstate = ESgetpars;
                    if (c == '[')
                        {			/* Function key */
                            scrollstate = ESfunckey;
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
                        scrollstate = ESgotpars;
                case ESgotpars:
                    scrollstate = ESnormal;
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
                            continue;
                        case 'A':
                            ansiscrollup ();
                            continue;
                        case 'B':
                        case 'e':
                            ansiscrolldown ();
                            continue;
                        case 'C':
                        case 'a':
                            continue;
                        case 'D':
                            continue;
                        case 'E':
                            continue;
                        case 'F':
                            continue;
                        case 'd':
                            continue;
                        case 'H':
                        case 'f':
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
                            continue;
                        case 'q':		/* DECLL - but only 3 leds */
                            continue;
                        case 'r':
                            continue;
                        case 's':
                            continue;
                        case 'u':
                            continue;
                        case 'X':
                            continue;
                        case '@':
                            continue;
                        case ']':		/* setterm functions */
                            continue;
                        case '~':		/* home keys */
                            if (par[0] == 1)
                                home ();
                            if (par[0] == 4)
                                end ();
                            if (par[0] == 5)
                                pageup ();
                            if (par[0] == 6)
                                pagedown ();
                            continue;
                        }
                    continue;
                case ESpercent:
                    scrollstate = ESnormal;
                    continue;
                case ESfunckey:
                    scrollstate = ESnormal;
                    continue;
                case EShash:
                    scrollstate = ESnormal;
                    continue;
                case ESsetG0:
                    scrollstate = ESnormal;
                    continue;
                case ESsetG1:
                    scrollstate = ESnormal;
                    continue;
                default:
                    scrollstate = ESnormal;
                }
        }				/* end switch(scrollstate) */

    printf ("\e(B");		/* set unix character set */
    printf ("\e[2J\e[0;0H\n");
    printf
        ("\e[0;1;32;40m Linux Ansi Viewer by \e[0;1;37;40mBen Fowler\e[0m\n\n");
    printf ("\e[?25h");		/* turn cursor on */
}				/* end scrollit() */

static void
usage (void)
{
    printf ("Usage: ansi [FILE]...\n");
    printf ("View ANSI escape coded FILEs on your terminal.\n\n");
    printf ("Commands while viewing:\n");
    printf ("  Up    - Scroll up\n");
    printf ("  Down  - Scroll down\n");
    printf ("  Pg Up - Up one page\n");
    printf ("  Pg Dn - Down one page\n");
    printf ("  Home  - Go to top of ansi\n");
    printf ("  End   - Go to end of ansi\n");
    printf ("  X     - Exit viewer / View next file\n");
}

void
view_ansi (FILE * fp)
{
    savetty ();
    initscr ();
    nonl ();
    cbreak ();
    noecho ();
    clear ();

    getmaxyx(stdscr, rows, cols);

    x = 0;
    y = 0;
    pos = 0;
    splos = 0;

    editbuffer = (unsigned int *) malloc(sizeof(unsigned int) * cols * EDIT_BUFFER_HEIGHT);
    if (NULL == editbuffer)
        {
            fprintf (stderr, "Could not allocate memory for buffer.\n");
            exit (1);
        }

    clearbuf (editbuffer, cols * EDIT_BUFFER_HEIGHT);
    pos = ansi_esc_translate (fp, editbuffer, cols, EDIT_BUFFER_HEIGHT);
    scrollit ();
    free (editbuffer);

    echo ();
    nl ();
    resetty ();
    endwin ();
}

int
main (int argc, char *argv[])
{
    FILE *fp;
    int arg;

    if (argc < 2)
        {
            usage ();
            return EXIT_SUCCESS;
        }

    for (arg = 1; arg < argc; arg++)
        {
            if ((fp = fopen (argv[arg], "r")) != NULL)
                {
                    view_ansi (fp);
                    fclose (fp);
                }
            else
                {
                    printf ("\nCould not open '%s'.\n", argv[arg]);
                    usage ();
                    return EXIT_FAILURE;
                }
        }

    return EXIT_SUCCESS;
}
