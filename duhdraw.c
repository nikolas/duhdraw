/*****************************************************************************
*                                  Duh DRAW 
*                                  for Linux
*                          (c) Copyleft February 1996
*                                 Ben Fowler
*
****************************************************************************/

#include <assert.h>
#include <string.h>
#include <strings.h>
#include <stdio.h>
#include <dirent.h>
#include <sys/stat.h>
#include <malloc.h>
#include <time.h>
#include <curses.h>
#include <ctype.h>
#include <stdlib.h>
#include <unistd.h>
#include "ansi-esc.h"
#include "ddlogo.h"

struct dir_entry
{
  char name[255];
  long int size;
  char sauce[50];
};

#define MAX_DIR_ENTRIES 1024

static struct dir_entry dir_entries[MAX_DIR_ENTRIES];

static int dircount;

struct sauce_info
{
  char sig[7];			/* SAUCE00 */
  char workname[35];
  char author[20];
  char group[20];
  char date[8];
  int unknown;
  int bla2;
  int bla3;
};

#define CRSUP 14
#define CRSDOWN 15
#define CRSLEFT 16
#define CRSRIGHT 17
#define HOME 18
#define END 19
#define INSERT 20
#define DELETE 21
#define PAGEDOWN 22
#define PAGEUP 23

/* Current x and y of cursor on screen.  */
static int x = 0;
static int y = 0;

/* Current y of cursor in edit buffer.  */
static int edit_buffer_y;

/* Block start and end positions.  */
static int block_upper_left_x;
static int block_upper_left_y;
static int block_lower_right_x;
static int block_lower_right_y;

static int insert = 0;			/* 0 = overtype , 1 if insert */
static int block = 0;			/* 0 if normal , 1 if blockmode */
static int linedraw = 0;		/* linedraw = 1, normal = 0 */
static int fore = 7;			/* foreground color */
static int back = 0;			/* current backround color */
static int text = 0;			/* 0 if text , 1 if attrib  */
static int blink = 0;			/* foreground is blinking? */
static int set = 0;			/* current highascii character set */
static int page = 0;			/* current page layer */
static int pos = 0;
static int par[8];
static int alastline;			/* actual lastline of ansi */
static FILE *fileout;
static int spacecount;
static int charcount;
static int olddir;
static int newdir;
static int savetype;
static int changed;
static int lastchar;

/*
 * TODO:
 *
 * Please replace magic numbers in the code with the constants
 * below. If there's no existing constant, please invent
 * one. Hopefully some day we can replace these with variables and
 * support buffers that are larger than 80x25. -Pekka, 2003-01-10
 */
#define EDIT_BUFFER_WIDTH  80
#define EDIT_BUFFER_HEIGHT 1000
#define EDIT_BUFFER_SIZE   (EDIT_BUFFER_WIDTH * EDIT_BUFFER_HEIGHT)

#define SCREEN_HEIGHT 23

static char filename[81];		/* name of file being edited */
static char statusline[81];
static char input[255];		/* global buffer for user input */
static unsigned int editbuffer[EDIT_BUFFER_SIZE];

static int ques = 0;
static int npar = 0;
static int attr = 7;
static int maxread = 0;
static int ishome = 0;
static int lattr;
static int lastline;

static char *colors[] =
  { "Black", "Red", "Green", "Brown", "Blue", "Purple", "Cyan", "White" };


static int highascii[15][11] = {
  {218, 191, 192, 217, 196, 179, 195, 180, 193, 194, 197},	/* single */
  {201, 187, 200, 188, 205, 186, 204, 185, 202, 203, 206},	/* double horizontal */
  {214, 183, 211, 189, 196, 186, 199, 182, 208, 210, 215},	/* double vertical */
  {213, 184, 212, 190, 205, 179, 198, 181, 207, 209, 216},	/* double horiz */
  {176, 177, 178, 219, 220, 223, 221, 222, 22, 254, 32},	/* solid blocks */
  {197, 206, 215, 216, 1, 2, 3, 4, 5, 6, 32},
  {16, 17, 18, 19, 21, 23, 25, 29, 30, 31, 32},
  {28, 168, 127, 128, 129, 130, 131, 132, 133, 134, 32},
  {135, 136, 137, 138, 139, 140, 141, 142, 143, 144, 32},
  {145, 146, 147, 148, 149, 150, 151, 152, 153, 154, 32},
  {155, 156, 157, 158, 159, 160, 161, 162, 163, 164, 32},
  {165, 166, 167, 169, 170, 171, 172, 173, 174, 175, 32},
  {224, 225, 226, 227, 228, 229, 230, 231, 232, 233, 32},
  {234, 235, 236, 237, 238, 239, 240, 241, 242, 243, 32},
  {244, 245, 246, 247, 248, 249, 250, 251, 252, 253, 32}
};

static int
dir_entry_compare (const void *d1, const void *d2)
{
  struct dir_entry *f1 = ((struct dir_entry *) d1);
  struct dir_entry *f2 = ((struct dir_entry *) d2);
  if ((f1->size == -1) && (f2->size == -1))
    return (strcmp (f1->name, f2->name));
  if (f1->size == -1)
    return (-1);
  if (f2->size == -1)
    return (1);
  return (strcmp (f1->name, f2->name));
}

struct dir_stats {
  char pathname[255];
  unsigned long total;
};

static void
read_dir_entries (struct dir_stats * stats)
{
  int root;
  char date[8];
  FILE *fp;
  DIR *dirp;
  struct dirent *firp;
  struct stat status;
  struct sauce_info sauceinfo;

  dircount = 0;
  stats->total = 0;
  root = 0;
  if (getcwd(stats->pathname, 255) == NULL) {
      perror("getcwd error");
  }
  if (stats->pathname[0] == '/' && stats->pathname[1] == 0)
    root = 1;
  dirp = opendir (stats->pathname);
  if (dirp != NULL)
    {
      while ((firp = readdir (dirp)) != NULL)
	{
	  if (strcmp (firp->d_name, ".") == 0)
	    continue;
	  if (root && (strcmp (firp->d_name, "..") == 0))
	    continue;
	  stat (firp->d_name, &status);
	  strcpy (dir_entries[dircount].sauce, "");
	  if (status.st_mode & S_IFDIR)
	    {
	      strcpy (dir_entries[dircount].name, firp->d_name);
	      dir_entries[dircount].size = -1;
	    }
	  else
	    {
	      strcpy (dir_entries[dircount].name, firp->d_name);
	      dir_entries[dircount].size = status.st_size;
	      fp = fopen (firp->d_name, "rb");
	      if (NULL == fp)
		{
		  /* Skip this file. */
		  continue;
		}
	      fseek (fp, -128, SEEK_END);
	      if (!fread(&sauceinfo, sizeof (struct sauce_info), 1, fp)) {
              perror("fread error");
          };
	      fclose (fp);
	      if (memcmp (sauceinfo.sig, "SAUCE00", 7) == 0)
		{
		  date[0] = sauceinfo.date[4];
		  date[1] = sauceinfo.date[5];
		  date[2] = '/';
		  date[3] = sauceinfo.date[6];
		  date[4] = sauceinfo.date[7];
		  date[5] = '/';
		  date[6] = sauceinfo.date[2];
		  date[7] = sauceinfo.date[3];
		  sprintf (dir_entries[dircount].sauce,
			   "%-20.20s %-10.10s %-9.9s %-8s",
			   sauceinfo.workname, sauceinfo.author,
			   sauceinfo.group, date);
		}
	    }
	  stats->total = stats->total + status.st_size;
	  dircount++;
	  /*
	   * Better to kill the program than allow buffer overflow.
	   */
	  assert(dircount < MAX_DIR_ENTRIES);
	}
      closedir (dirp);
    }
  qsort (dir_entries, dircount, sizeof (struct dir_entry), dir_entry_compare);
  dircount--;
  if (dircount < 0)
    dircount = 0;
}

static void
print_dir_entry (int h, int entry_y, int curlo)
{
  printf ("\e[%d;1H", entry_y + 1);
  if (h)
    printf ("\e[1;37;44m");
  else
    printf ("\e[0;37;40m");
  printf ("%-20.20s", dir_entries[curlo].name);
  if (dir_entries[curlo].size == -1)
    printf ("\e[1;33m    <DIR> ");
  else
    printf ("\e[1;30m %8ld ", dir_entries[curlo].size);
  printf ("\e[1;36m%-50.50s", dir_entries[curlo].sauce);

}

static void
print_dir_stats (struct dir_stats * stats)
{
  printf ("\e[25;1H\e[0;30;47m");
  printf ("%-42.42s %11ld KB total (D)elete (Q)uit ",
	  stats->pathname, stats->total / 1024);
}

static void
refreshdir (struct dir_stats * stats)
{
  int diry;
  int curlo;

  printf ("\e[0m\e[2J\e[1;1H");
  print_dir_stats (stats);
 
  curlo = 0;
  diry  = 0;

  while ((curlo <= dircount) && (curlo < 24))
    {
      print_dir_entry (0, diry, curlo);
      diry++;
      curlo++;
    }
  print_dir_entry (1, 0, 0);
}

static void
cgoto ()
{
  printf ("\033[%02d;%02dH", y + 1, x + 1);
}

static void
ccolor (void)
{
  printf ("\e[0;%c;%s;%d;%dm", (fore > 7) ? '1' : '0',
	  (blink == 1) ? "5" : "25", (fore & 7) + 30, back + 40);
}

static int
mygetchar (void)
{
  int c;
  int ex = 0;
  int scrollstate = ESnormal;
  fflush (stdout);
  while (ex != 1)
    {
      c = getchar ();
      if ((c == 27) && (scrollstate == ESesc))
	{
	  return (c);
	}
      if (c == 27)
	{
	  scrollstate = ESesc;
	  continue;
	}

      switch (scrollstate)
	{
	case ESesc:
	  scrollstate = ESnormal;
	  if ((c >= 'a') && (c <= 'z'))
	    {
	      c = c + 256;
	      ex = 1;
	      continue;
	    }
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
	    case '>':		/* Numeric keypad */
	      continue;

	    case '.':
	      c = 256 + '>';
	      ex = 1;
	      continue;

	    case ',':
	      c = 256 + '<';
	      ex = 1;
	      continue;

	    case '-':
	      c = 256 + '-';
	      ex = 1;
	      continue;
	    case '=':
	      c = 256 + '+';
	      ex = 1;
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
	      c = CRSUP;
	      ex = 1;
	      continue;
	    case 'B':
	      c = CRSDOWN;
	      ex = 1;

	      continue;
	    case 'C':
	    case 'a':
	      c = CRSRIGHT;
	      ex = 1;
	      continue;
	    case 'D':
	      c = CRSLEFT;
	      ex = 1;
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

	    case '~':		/* home and function keys */
	      if (par[0] == 1)
		{
		  c = HOME;
		  ex = 1;
		}
	      if (par[0] == 2)
		{
		  c = INSERT;
		  ex = 1;
		}
	      if (par[0] == 3)
		{
		  c = DELETE;
		  ex = 1;
		}
	      if (par[0] == 4)
		{
		  c = END;
		  ex = 1;
		}
	      if (par[0] == 5)
		{
		  c = PAGEUP;
		  ex = 1;
		}
	      if (par[0] == 6)
		{
		  c = PAGEDOWN;
		  ex = 1;
		}
	      if (par[0] > 6)
		{
		  c = 256 + par[0];
		  ex = 1;
		}
	      continue;
	    }
	  continue;
	case ESpercent:
	  scrollstate = ESnormal;
	  continue;
	case ESfunckey:
	  scrollstate = ESnormal;
	  switch (c)
	    {
	    case 'A':
	      c = 256;
	      ex = 1;
	      /* F1 */
	      continue;

	    case 'B':		/* F2 */
	      c = 257;
	      ex = 1;
	      continue;
	    case 'C':		/* F3 */
	      c = 258;
	      ex = 1;
	      continue;
	    case 'D':		/* F4 */
	      c = 259;
	      ex = 1;
	      continue;
	    case 'E':		/* F5 */
	      c = 260;
	      ex = 1;
	      continue;

	    default:
	      continue;
	    }
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
	  if (c != -1)
	    ex = 1;

	}			/* end switch */
    }				/* end while */
  return (c);

}				/* end getchar() */

static struct tm *
getdatetime ()
{
  time_t timer;
  time (&timer);
  return localtime (&timer);
}

/* ask yes or no, v is the default if enter is pressed  1=yes  0=no*/
static int
iyesno (int v)			/* do yes no quistion */
{
  int c;
  while (1)			/* Get a character until we have an answer */
    {
      c = mygetchar ();
      if (c == 13)
	return (v);		/* return?  take the default */
      if ((c == ' ') && (v == 1))	/* space and default is 1, go cursor right */
	{
	  c = CRSRIGHT;
	}
      if ((c == ' ') && (v == 0))	/* space and default is 0, go cursor left */
	{
	  c = CRSLEFT;
	}

      if ((c == 'Y') || (c == 'y'))	/* If Y or y, return 1 [yes] */
	{
	  printf ("\e[9D");
	  printf ("\e[1;37;44m Yes \e[0m No ");
	  return (1);
	}

      if ((c == 'N') || (c == 'n'))	/* If N or n, return 0 [no] */
	{
	  printf ("\e[9D");
	  printf ("\e[0m Yes \e[1;37;44m No ");
	  return (0);
	}

      if ((c == CRSRIGHT) && (v == 1))
	{
	  printf ("\e[9D");
	  printf ("\e[0m Yes \e[1;37;44m No ");
	  v = 0;
	}

      if ((c == CRSLEFT) && (v == 0))
	{
	  printf ("\e[9D");
	  printf ("\e[1;37;44m Yes \e[0m No ");
	  v = 1;
	}
    }
}

/* highlight yes, unhighlight no */
static int
yesno ()
{
  printf ("\e[1;37;44m Yes \e[0m No ");
  return (iyesno (1));
}

/* unhighlight yes, highlight no */
static int
noyes ()
{
  printf ("\e[0m Yes \e[1;37;44m No ");
  return (iyesno (0));
}

int
static redrawline (int maxlen)
{
  int i;
  printf ("\e[u");		/* restore cursor location */
  i = 0;
  while ((input[i] != 0) && (i < maxlen))
    {
      printf ("%c", input[i]);
      i++;
    }
  if (i < maxlen)
    printf (" \e[D");		/* erase last position ? */

  return (i);
}

static void
mygetline (int maxlen)
{
  char redo[255];
  int c, i, here, end;

  if (maxlen > 255)
    maxlen = 255;

  strcpy (redo, input);
  printf ("\e[s");		/* save location */
  here = redrawline (maxlen);
  end = here;


  while ((c = mygetchar ()))
    {
      c = c & 255;
      switch (c)
	{

	case 13:
	  return;

	case 24:		/* ctrl-X */
	  input[0] = 0;
	  printf ("\e[u");
	  for (i = 0; i < maxlen; i++)
	    printf (" ");
	  printf ("\e[u");
	  here = 0;
	  end = 0;
	  break;

	case 1:		/* ctrl-A */
	  strcpy (input, redo);
	  printf ("\e[u");
	  for (i = 0; i < maxlen; i++)
	    printf (" ");
	  here = redrawline (maxlen);
	  end = here;
	  break;

	case 8:
	case 127:

	  if (here > 0)
	    {
	      here--;
	      i = here;
	      while (i < end)
		{
		  input[i] = input[i + 1];
		  i++;
		}
	      end--;
	      input[end] = 0;
	      printf ("\e[1D \e[1D");	/* cursor left */
	      if (here < end)
		{
		  end = redrawline (maxlen);
		  printf ("\e[%dD", end - here);
		}

	      break;
	    }

	  break;



	case CRSLEFT:

	  if (here > 0)
	    {
	      here--;
	      printf ("\e[1D");	/* cursor back */
	    }
	  break;


	case CRSRIGHT:
	  if (here < end)
	    {
	      here++;
	      printf ("\e[1C");
	      break;
	    }
	  break;



	default:
	  if ((here == 0) && (c == ' '))
	    break;
	  if ((c == ' ') && (input[here - 1] == ' '))
	    break;

	  if ((end < maxlen) && (c > 31) && (c != 127))
	    {

	      if (here != end)
		{
		  i = end;
		  while (i >= here)
		    {
		      input[i + 1] = input[i];
		      i--;
		    }
		  end++;
		}

	      input[here] = c;
	      here++;
	      end++;
	      input[end] = 0;
	      if (here < end)
		{
		  end = redrawline (maxlen);
		  i = end - here;	/* */

		  printf ("\e[%dD", end - here);
		}
	      else
		{
		  putc (c, stdout);
		}

	    }
	  break;

	}			/* end switch */
    }				/* end while */

}



static void
ccolorbar (int fg, int bg)
{
  printf ("\e[0;%c;%s;%d;%dm", (fg & 8) ? '1' : '0', (fg & 16) ? "5" : "25",
	  (fg & 7) + 30, bg + 40);
}


static void
drawbar ()
{
  printf ("\e[u");
  if (back > 0)
    ccolorbar (back, 0);
  else
    ccolorbar (7, 0);
  printf ("\e[A\e[%dC B ", back + 1);

  printf ("\e[u");
  if ((fore != 0) && (fore != 16))
    ccolorbar (fore, 0);
  else
    ccolorbar (7, 0);

  printf ("\e[B\e[%dC F ", fore + 1);

  printf ("\e[u\e[36C");
  ccolorbar (7, 0);
  printf ("\e[K");
  ccolorbar (fore, back);
  printf ("%s%s on %s", (fore & 8) ? "B." : "", colors[fore & 7],
	  colors[back & 7]);
}


static void
colorbar ()
{
  int c, i;
  if (blink == 1)
    fore = fore + 16;

  printf ("\e[s  ");		/* save current location, bump it two */
  c = 219;

  for (i = 0; i < 32; i++)
    {
      ccolorbar (i, 0);
      putc (c, stdout);
    }

  drawbar ();

  while ((c = mygetchar ()) != 13)
    switch (c)
      {

      case CRSDOWN:
	if (back > 0)
	  {
	    back--;
	    drawbar ();

	  }
	break;

      case CRSUP:
	if (back < 7)
	  {
	    back++;
	    drawbar ();
	  }
	break;


      case CRSLEFT:
	if (fore > 0)
	  {
	    fore--;
	    drawbar ();
	  }
	break;



      case CRSRIGHT:
	if (fore < 31)
	  {
	    fore++;
	    drawbar ();
	  }
	break;

      default:
	break;

      }

  if (fore > 15)
    {
      fore = fore - 16;
      blink = 1;
    }
  else
    blink = 0;

}

static void
clr ()
{
  for (pos = 0; pos < EDIT_BUFFER_SIZE; pos++)
    {
      editbuffer[pos] = 0x0720;	/* defualt color ' ' */
    }
  printf ("\e[0m\e[1;1H\e[2J");


  fore = 7;
  back = 0;
  changed = 0;
  pos = 0;

}

static void
readfile ()
{
  FILE *fp;

  x = 0;
  y = 0;
  edit_buffer_y = 0;

  if ((fp = fopen (filename, "rb")) != NULL)
    {
      pos = ansi_esc_translate (fp, editbuffer, EDIT_BUFFER_WIDTH,
				EDIT_BUFFER_HEIGHT);
      fclose (fp);
    }
}

static void
printxy ()
{
  printf ("\e[25;2H\e[0m\e[0;1;31;40m%2d", x + 1);
  printf ("\e[25;5H\e[0m\e[0;1;31;40m%4d", edit_buffer_y + 1);
  ccolor ();
  cgoto ();
}


static void
status ()			/* print status line */
{
  int i;
/* print status bar on bottom */
  printf ("\e[0m\e[24;1H\e[K%s", statusline);
  printf ("\e[25;1H");
  printf
    ("\e[0;1;31;40m(  ,    )                  \e[1;33;40mPage:1 Set:  \e[0;36;44m1=  2=  3=  4=  5=  6=  7=  8=  9=  10= ");
  printf ("\e[25;10H\e[0m ");
  printf ("\e[%c;%s;%d;%dm", (fore > 7) ? '1' : '0',
	  (blink == 0) ? "25" : "5", (fore & 7) + 30, back + 40);
  printf (" Color ");
  printf ("\e[0m ");
  if (text == 0)
    strcpy (input, "\e[1;33;40m Text \e[0m ");
  else
    strcpy (input, "\e[1;37;44m Attr \e[0m ");
  if (linedraw == 1)
    strcpy (input, "\e[1;34;40m Line \e[0m ");
  if (block == 1)
    strcpy (input, "\e[1;32;40m Block\e[0m ");
  printf ("%s", input);

  if (insert == 1)
    printf ("\e[1;37;40mIn\e[0m");

/* fill in the character set */

  printf ("\e[25;33H\e[0;1;33;40m%-2d", page + 1);
  printf ("\e[25;39H\e[0;1;33;40m%-2d", set + 1);
  printf ("\e[25;43H\e[0;1;33;44m");
  for (i = 0; i < 9; i++)
    printf ("%c\e[C\e[C\e[C", highascii[set][i]);
  printf ("\e[C%c", highascii[set][9]);
  printxy ();

}


static void
fprintattr ()
{
  int pre;
  int lbg;
  int lfg;
  int lbold;
  int lflash;
  int fg;
  int bg;
  int abold;
  int aflash;
  aflash = 25;

  if (lattr == attr)		/* certain cases :) */
    return;

  if (attr == 7)
    {
      fprintf (fileout, "\e[0m");
      return;
    }
  fg = (attr & 7) + 30;
  bg = ((attr >> 4) & 7) + 40;
  lfg = (lattr & 7) + 30;
  lbg = ((lattr >> 4) & 7) + 40;
  lflash = 25;
  lbold = 0;
  abold = 0;
  if (attr & 8)
    abold = 1;
  if (lattr & 8)
    lbold = 1;
  if (attr & 128)
    aflash = 5;
  if (lattr & 128)
    lflash = 5;

  pre = 0;
  fprintf (fileout, "\e[");

  if (lbold != abold)
    {
      fprintf (fileout, "%d", abold);
      if (abold == 0)
	{
	  if (aflash == 5)
	    fprintf (fileout, ";%d", aflash);
	  if (fg != 7)
	    fprintf (fileout, ";%d", fg);
	  if (bg != 0)
	    fprintf (fileout, ";%d", bg);
	  fputc ('m', fileout);
	  return;
	}
      else
	{
	  if (aflash != lflash)
	    fprintf (fileout, ";%d", aflash);
	  if (lfg != fg)
	    fprintf (fileout, ";%d", fg);
	  if (lbg != bg)
	    fprintf (fileout, ";%d", bg);
	  fputc ('m', fileout);
	  return;
	}
    }



  if (lflash != aflash)
    {
      fprintf (fileout, "%d", aflash);
      pre = 1;
    }

  if (lfg != fg)
    {
      if (pre == 1)
	fputc (';', fileout);
      fprintf (fileout, "%d", fg);
      pre = 1;
    }

  if (lbg != bg)
    {
      if (pre == 1)
	fputc (';', fileout);
      fprintf (fileout, "%d", bg);
    }

  fputc ('m', fileout);

}

static void
fdospace ()
{
  char foo[90];
  if (spacecount == 0)
    return;
  strcpy (foo,
	  "                                                                                 ");
  if ((((lattr >> 4) & 7) == 0) && (spacecount > 4) && (savetype == 1))
    fprintf (fileout, "\e[%dC", spacecount);
  else
    {
      foo[spacecount] = 0;
      fprintf (fileout, "%s", foo);
    }
  charcount = charcount + spacecount;
  spacecount = 0;
}

static void
fdumpline (int w)		/* print one line to file */
{
  int co;
  int com;
  int c;
  spacecount = 0;
  charcount = 0;
  co = 0;
  com = 79;
  if (w)
    {
      co = block_upper_left_x;
      com = block_lower_right_x;
    }

  for (; co <= com; co++)
    {
      attr = (editbuffer[pos + co] >> 8);
      if ((lattr != attr) || ((pos + co) == 0))
	{
	  fdospace ();

	  if (savetype == 1)
	    fprintattr ();

	  lattr = attr;
	}

      c = editbuffer[pos + co] & 0xFF;
      if (c != 32)
	{
	  fdospace ();
	  fputc (c, fileout);
	  charcount++;
	}
      else
	spacecount++;
    }

  if (w)
    if ((block_lower_right_x - block_upper_left_x) < 79)
      {
	fprintf (fileout, "\r\n");
	return;
      }

  if ((lattr >> 4) & 7)
    fdospace ();
  else if (charcount < (com + 1))
    fprintf (fileout, "\r\n");

}

static void
printattr ()
{
  int fg;
  int bg;
  int abold;
  int aflash;
  aflash = 25;
  fg = (attr & 7);
  bg = (attr >> 4) & 7;
  fg = fg + 30;
  bg = bg + 40;
  abold = 0;
  if (attr & 8)
    abold = 1;
  if (attr & 128)
    aflash = 5;
  printf ("\e[%d;%d;%d;%dm", abold, aflash, fg, bg);

}

static void
dumpline (int start_pos)
{
  int co;
  for (co = 0; co < EDIT_BUFFER_WIDTH; co++)
    {
      attr = (editbuffer[start_pos + co] >> 8);

      if ((lattr != attr) || (co == 0))
	{
	  printattr ();
	  lattr = attr;
	}
      putchar (editbuffer[start_pos + co] & 0xFF);
    }
  putchar ('\n');
  putchar ('\r');
}

static void
screen_redraw (int redraw_pos)
{
  int i;

  printf ("\e[2J\e[0;0H");
  for (i = 0; i < SCREEN_HEIGHT; i++)
    {
      dumpline (redraw_pos + i * EDIT_BUFFER_WIDTH);
    }
}

static void
ansiscrollup ()
{
  if ((pos - (SCREEN_HEIGHT * EDIT_BUFFER_WIDTH)) >= 0)
    {
      pos -= 80;
      screen_redraw (pos - SCREEN_HEIGHT * EDIT_BUFFER_WIDTH);

      ishome = 0;
    }
  else
    ishome = 1;
}

static void
ansiscrolldown ()
{
  if ((pos + EDIT_BUFFER_WIDTH) >= maxread)
    return;

  if (lastline < SCREEN_HEIGHT)
    return;

  pos += EDIT_BUFFER_WIDTH;
  screen_redraw (pos - SCREEN_HEIGHT * EDIT_BUFFER_WIDTH);

  ishome = 0;
}

static void
ansihome ()
{
  if (ishome == 1)
    return;

  edit_buffer_y = 0;
  y = 0;
  ishome = 1;
  pos = EDIT_BUFFER_WIDTH * (lastline < SCREEN_HEIGHT ?
			     lastline : SCREEN_HEIGHT);

  screen_redraw (0);
}

static void
ansiend ()
{
  int zz;

  if (lastline < SCREEN_HEIGHT)
    return;

  if ((pos + EDIT_BUFFER_WIDTH) >= maxread)
    return;

  zz = lastline - SCREEN_HEIGHT;
  if (zz < 0)
      zz = 0;
  zz *= EDIT_BUFFER_WIDTH;

  pos = zz + (SCREEN_HEIGHT + 1) * EDIT_BUFFER_WIDTH;
  screen_redraw (pos - SCREEN_HEIGHT * EDIT_BUFFER_WIDTH);

  ishome = 0;
}

static void
ansipagedown ()
{
  if ((lastline - (pos / EDIT_BUFFER_WIDTH)) <= SCREEN_HEIGHT)
    {
      ansiend ();
    }
  else
    {
      screen_redraw (pos);
      pos += EDIT_BUFFER_WIDTH * SCREEN_HEIGHT;
      edit_buffer_y = edit_buffer_y + SCREEN_HEIGHT;
      ishome = 0;
    }
}

static void
ansipageup ()
{
  if ((pos - (EDIT_BUFFER_WIDTH * SCREEN_HEIGHT * 2)) >= 0)
    {
      pos -= EDIT_BUFFER_WIDTH * SCREEN_HEIGHT;
      screen_redraw (pos - EDIT_BUFFER_WIDTH * SCREEN_HEIGHT);
      edit_buffer_y = edit_buffer_y - SCREEN_HEIGHT;
    }
  else
    ansihome ();
}

static void
docr ()
{
  x = 0;
  if (edit_buffer_y < 999)
    {
      edit_buffer_y = edit_buffer_y + 1;
      if (y < 23)
	{
	  y = y + 1;
	  printxy ();
	}
      else
	{
	  ansiscrolldown ();
	  status ();
	}
    }
}

static void
dochar (int c)
{

  int lx;

  lx = 78;

  attr = fore + (back * 16) + (blink * 128);
  attr = attr << 8;

  if (insert)
    while (lx >= x)
      {
	editbuffer[(edit_buffer_y * 80) + lx + 1] =
	    editbuffer[(edit_buffer_y * 80) + lx];
	lx--;
      }

  editbuffer[(edit_buffer_y * 80) + x] = (c & 255) | attr;

  if (insert)
    {
      printf ("\e[%d;1H", y + 1);
      dumpline ((edit_buffer_y * EDIT_BUFFER_WIDTH) - 
		  (SCREEN_HEIGHT * EDIT_BUFFER_WIDTH));
    }
  else
    putc (c, stdout);

  if (x < 79)
    {
      x = x + 1;
      printxy ();
    }
  else
    {
      docr ();
    }
  if (edit_buffer_y > alastline)
    alastline = edit_buffer_y;
  changed = 1;

}

static void
doattr ()
{
  int c;
  attr = fore + (back * 16) + (blink * 128);
  attr = attr << 8;
  c = editbuffer[(edit_buffer_y * 80) + x] & 255;
  editbuffer[(edit_buffer_y * 80) + x] = c | attr;
  changed = 1;
  ccolor ();
  cgoto ();
  putc (c, stdout);
  cgoto ();
}

void
doline (char *ptr)
{
  printf ("\e[0m\e[24;1H\e[K%s", ptr);
}

static void
savefile (int w)		/* 0 = whole file,1 = box */
{
  int i, mi;
  doline ("Save file ? ");
  if (yesno () == 0)
    goto zzyz;
  savetype = 0;
  doline ("Save file as ANSI ? ");
  if (yesno ())
    savetype = 1;
  doline ("Filename to save as : ");
  if (w)
    strcpy (input, "");
  else
    strcpy (input, filename);
  mygetline (60);
  if (input[0] == 0)
    goto zzyz;
  strcpy (filename, input);
  fileout = fopen (input, "wb");
  if (fileout == NULL)
    goto zzyz;
  if (savetype == 1)
    {
      doline ("Clear screen before ? ");
      if (yesno ())
	fprintf (fileout, "\e[0m\e[2J");
      doline ("Home cursor ? ");
      if (yesno ())
	fprintf (fileout, "\e[1;1H");
    }

  i = 0;
  mi = alastline;
  if (w)
    {
      i = block_upper_left_y;
      mi = block_lower_right_y;
    }
  pos = i * 80;
  if (w)
    attr = (editbuffer[pos + block_upper_left_x] >> 8);
  else
    attr = (editbuffer[pos] >> 8);
  if (savetype == 1)
    {
      fprintf (fileout, "\e[%s;%s;%d;%dm", (attr & 8) ? "1" : "0",
	       (attr & 128) ? "5" : "25", (attr & 7) + 30,
	       ((attr >> 4) & 7) + 40);
      lattr = attr;
    }

  for (; i <= mi; i++)
    {
      pos = i * 80;
      fdumpline (w);		/* 0 = full line,1 = box */
    }
  fclose (fileout);
  if (w == 0)
    changed = 0;
zzyz:
  status ();
}

static void
dodelete ()
{
  int i;

  i = x;
  while (i < 80)
    {
      editbuffer[(edit_buffer_y * 80) + i] =
	  editbuffer[(edit_buffer_y * 80) + i + 1];
      i++;
    }
  editbuffer[(edit_buffer_y * 80) + 79] = 0x0720;
  printf ("\e[%d;1H", y + 1);
  pos = edit_buffer_y * 80;
  dumpline (pos);
  ccolor ();
  cgoto ();
  changed = 1;
}

static void
dorefresh ()
{
  screen_redraw ((edit_buffer_y - y) * EDIT_BUFFER_WIDTH);
  status ();
}

static void
dohelp ()
{
  printf ("\e[0m\e[2J\e[1;1H");
  printf ("                                  \e[1;37mDuh DRAW Help\r\n\r\n");
  printf
    ("\e[1;37mAlt-A   \e[0mToggle Attribute Drawing mode    \e[1;37mAlt-N \e[0m\r\n");
  printf
    ("\e[1;37mAlt-B   \e[0mBlock Select Mode                \e[1;37mAlt-O \e[0m\r\n");
  printf
    ("\e[1;37mAlt-C   \e[0mUse Character from Alt-P         \e[1;37mAlt-P \e[0m Pickup Character under cursor\r\n");
  printf
    ("\e[1;37mAlt-D   \e[0mToggle Line Drawing mode         \e[1;37mAlt-Q \e[0m\r\n");
  printf
    ("\e[1;37mAlt-E   \e[0mErase current page layer         \e[1;37mAlt-R \e[0m\r\n");
  printf
    ("\e[1;37mAlt-F   \e[0mSelect fore and back colors      \e[1;37mAlt-S \e[0m Save file\r\n");
  printf
    ("\e[1;37mAlt-G   \e[0m                                 \e[1;37mAlt-T \e[0m \r\n");
  printf
    ("\e[1;37mAlt-H   \e[0mThis help screen                 \e[1;37mAlt-U \e[0m Pickup Color Under cursor\r\n");
  printf
    ("\e[1;37mAlt-I   \e[0mInsert line                      \e[1;37mAlt-V \e[0m\r\n");
  printf
    ("\e[1;37mAlt-J   \e[0mInsert Column                    \e[1;37mAlt-W \e[0m\r\n");
  printf
    ("\e[1;37mAlt-K   \e[0mDelete Column                    \e[1;37mAlt-X \e[0m Exit DuhDraw\r\n");
  printf
    ("\e[1;37mAlt-L   \e[0mLoad file                        \e[1;37mAlt-Y \e[0m Delete Line\r\n");
  printf
    ("\e[1;37mAlt-M   \e[0m                                 \e[1;37mAlt-Z \e[0m\r\n");
  printf
    ("\e[1;37mAlt-[   \e[0mDecrement Page layer             \e[1;37mAlt-] \e[0m Increment Page Layer.\r\n");
  printf
    ("\e[1;37mAlt-<   \e[0mDecrement HighAscii Set          \e[1;37mAlt-> \e[0m Increment HighAscii Set\r\n");
  printf
    ("\e[1;37mShift-F1 \e[0mthru \e[1;37mF10  \e[0mSelect HighAscii Set. \e[1;37mHome   \e[0mBeginning of Line/Ansi\r\n");
  printf
    ("\e[1;37mInsert  \e[0mToggle Insert mode               \e[1;37mEnd   \e[0m End of Line/Ansi\r\n");
  printf
    ("\e[1;37mPG UP   \e[0mUp one page                      \e[1;37mPG DN \e[0m Down one page\r\n");
  printf ("\e[25;1H\e[1;37;44m%-56.56s", filename);
  printf ("%24.24s", asctime (getdatetime ()));
  printf ("\e[22;1H\e[0;1;37m                                  Press a key.");
  mygetchar ();
  dorefresh ();
}

static void
dolinedraw ()
{
  int c = 0;
  attr = fore + (back * 16) + (blink * 128);
  attr = attr << 8;
  switch (newdir)
    {
    case 1:			/* crs up */
      c = highascii[set][5];
      if (olddir == 3)
	c = highascii[set][2];
      if (olddir == 4)
	c = highascii[set][3];
      break;

    case 2:			/* crs down */
      c = highascii[set][5];
      if (olddir == 3)
	c = highascii[set][0];
      if (olddir == 4)
	c = highascii[set][1];
      break;

    case 3:			/* crs left */
      c = highascii[set][4];
      if (olddir == 1)
	c = highascii[set][1];
      if (olddir == 2)
	c = highascii[set][3];
      break;

    case 4:			/* crs right */
      c = highascii[set][4];
      if (olddir == 1)
	c = highascii[set][0];
      if (olddir == 2)
	c = highascii[set][2];
      break;

    default:
      break;

    }

  editbuffer[(edit_buffer_y * 80) + x] = c | attr;
  ccolor ();
  cgoto ();
  putc (c, stdout);
  cgoto ();
  olddir = newdir;
  changed = 1;
}

static void
fixvars ()
{
  int c, ok;
  strcpy (statusline, "");
  ishome = 0;
  lastline = pos / 80;
  c = lastline * 80;
  ok = pos - c;
  if (ok > 0)
    lastline++;
  alastline = lastline;
  maxread = pos;
  pos = 0;
  c = 0;
  ok = 0;
  lastline = 999;
  maxread = 79999;
  linedraw = 0;
  block = 0;
  y = 0;
  x = 0;
  edit_buffer_y = y;
  changed = 0;
  ansihome ();
  status ();
}

static void
loadfile ()
{
  struct dir_stats stats;
  int curlo;
  int diry;
  int ex;
  int c;

  curlo = 0;
  diry  = 0;
  ex    = 0;

  read_dir_entries (&stats);
  refreshdir (&stats);

  while (ex != 1)
    {
      c = mygetchar ();
      switch (c)
	{
	case 'q':
	case 'Q':
	  ex = 1;		/* escape */
	  dorefresh ();
	  break;

	case 'd':
	case 'D':		/* delete file */
	  if (dir_entries[curlo].size == -1)
	    break;
	  printf ("\e[25;1H\e[0;1;37m\e[KDelete %s ? ",
		  dir_entries[curlo].name);
	  if (yesno ())
	    {
	      remove (dir_entries[curlo].name);
	      read_dir_entries (&stats);
	      refreshdir (&stats);
	    }
	  else
	    {
	      print_dir_stats (&stats);
	    }
	  break;

	case 13:		/* enter */
	  if (dir_entries[curlo].size == -1)
	    {
          if (chdir(dir_entries[curlo].name)) {
              perror("chdir error");
          };
	      read_dir_entries (&stats);
	      refreshdir (&stats);
	    }
	  else
	    {
	      strcpy (filename, dir_entries[curlo].name);
	      clr ();
	      readfile ();
	      fixvars ();
	      ex = 1;
	    }
	  break;

	case CRSUP:
	  if (curlo > 0)
	    {
	      print_dir_entry (0, diry, curlo);
	      curlo--;
	      if (diry > 0)
		{
		  diry--;
		  print_dir_entry (1, diry, curlo);
		}
	      else
		{
		  printf ("\e[1;1H\e[L");	/* insert line */
		  print_dir_stats (&stats);
		  print_dir_entry (1, diry, curlo);
		}
	    }
	  break;

	case CRSDOWN:
	  if (curlo < dircount)
	    {
	      print_dir_entry (0, diry, curlo);
	      curlo++;
	      if (diry < 23)	/* 0 thru 23 = 1 thru 24 */
		{
		  diry++;
		  print_dir_entry (1, diry, curlo);
		}
	      else
		{
		  printf ("\e[25;1H\e[0m\e[K");	/* erase stat */
		  diry++;
		  print_dir_entry (1, diry, curlo);
		  diry--;
		  printf ("\r\n");
		  print_dir_stats (&stats);
		}
	    }
	  break;

	default:
	  break;
	}
    }
}

static void
insertline ()
{
  int i;
  if (alastline < 999)
    {
      memmove (&(editbuffer[(edit_buffer_y + 1) * 80]),
	       &(editbuffer[edit_buffer_y * 80]),
	       (999 - edit_buffer_y) * 160);
      for (i = 0; i < 80; i++)
	editbuffer[(edit_buffer_y * 80) + i] = 0x0720;
      alastline++;
      dorefresh ();
    }
  changed = 1;
}

static void
deleteline ()
{
  if (alastline > 0)
    {
      memmove (&(editbuffer[edit_buffer_y * 80]),
	       &(editbuffer[(edit_buffer_y + 1) * 80]),
	       (999 - edit_buffer_y) * 160);
      alastline--;
      dorefresh ();
    }
  changed = 1;
}

static void
deletecolumn ()
{
  int i, j;
  for (j = x; j < 80; j++)
    for (i = 0; i < 1000; i++)
      editbuffer[(i * 80) + j] = editbuffer[(i * 80) + j + 1];
  for (i = 0; i < 1000; i++)
    editbuffer[(i * 80) + 79] = 0x0720;
  dorefresh ();
  changed = 1;
}

static void
insertcolumn ()
{
  int i, j;
  if (x == 79)
    return;
  for (j = 78; j >= x; j--)
    for (i = 0; i < 1000; i++)
      editbuffer[(i * 80) + j + 1] = editbuffer[(i * 80) + j];
  for (i = 0; i < 1000; i++)
    editbuffer[(i * 80) + x] = 0x0720;
  dorefresh ();
  changed = 1;
}

static int
flipchar (int cc)
{
  int at;
  at = cc & 0xFF00;
  cc = cc & 0xFF;
  switch (cc)
    {
    case 220:
      cc = 223;
      break;
    case 223:
      cc = 220;
      break;
    case 24:
      cc = 25;
      break;
    case 25:
      cc = 24;
      break;
    case 30:
      cc = 31;
      break;
    case 31:
      cc = 30;
      break;
    case 33:
      cc = 173;
      break;
    case 173:
      cc = 33;
      break;
    case 63:
      cc = 168;
      break;
    case 168:
      cc = 63;
      break;
    case 183:
      cc = 189;
      break;
    case 184:
      cc = 190;
      break;
    case 187:
      cc = 188;
      break;
    case 188:
      cc = 187;
      break;
    case 189:
      cc = 183;
      break;
    case 190:
      cc = 184;
      break;
    case 191:
      cc = 217;
      break;
    case 192:
      cc = 218;
      break;
    case 193:
      cc = 194;
      break;
    case 194:
      cc = 193;
      break;
    case 200:
      cc = 201;
      break;
    case 201:
      cc = 200;
      break;
    case 202:
      cc = 203;
      break;
    case 203:
      cc = 202;
      break;
    case 207:
      cc = 209;
      break;
    case 209:
      cc = 207;
      break;
    case 208:
      cc = 210;
      break;
    case 210:
      cc = 208;
      break;
    case 211:
      cc = 214;
      break;
    case 212:
      cc = 213;
      break;
    case 213:
      cc = 212;
      break;
    case 214:
      cc = 211;
      break;
    case 217:
      cc = 191;
      break;
    case 218:
      cc = 192;
      break;

    }
  return (at | cc);
}

static void
flipblock ()
{
  int ty1, ty2, tx;
  int tempbuf[80];
  ty1 = block_upper_left_y;
  ty2 = block_lower_right_y;

  while (ty1 <= ty2)
    {
      for (tx = block_upper_left_x; tx <= block_lower_right_x; tx++)
	{
	  tempbuf[tx] = editbuffer[(ty1 * 80) + tx];	/* save line */
	  editbuffer[(ty1 * 80) + tx] = flipchar (editbuffer[(ty2 * 80) + tx]);	/* move bottom to top */
	  editbuffer[(ty2 * 80) + tx] = flipchar (tempbuf[tx]);
	}
      ty1++;
      ty2--;
    }
}

static int
mirrorchar (int cc)
{
  int ta;
  ta = cc & 0xFF00;
  cc = cc & 0xFF;
  switch (cc)
    {
    case 26:
      cc = 27;
      break;
    case 27:
      cc = 26;
      break;
    case 10:
      cc = 11;
      break;
    case 11:
      cc = 10;
      break;
    case 40:
      cc = 41;
      break;
    case 41:
      cc = 40;
      break;
    case 60:
      cc = 62;
      break;
    case 62:
      cc = 60;
      break;
    case 91:
      cc = 93;
      break;
    case 93:
      cc = 91;
      break;
    case 123:
      cc = 125;
      break;
    case 125:
      cc = 123;
      break;
    case 47:
      cc = 92;
      break;
    case 92:
      cc = 47;
      break;
    case 169:
      cc = 170;
      break;
    case 170:
      cc = 169;
      break;
    case 174:
      cc = 175;
      break;
    case 175:
      cc = 174;
      break;
    case 221:
      cc = 222;
      break;
    case 222:
      cc = 221;
      break;
    case 180:
      cc = 195;
      break;
    case 181:
      cc = 198;
      break;
    case 182:
      cc = 199;
      break;
    case 183:
      cc = 214;
      break;
    case 184:
      cc = 213;
      break;
    case 185:
      cc = 204;
      break;
    case 187:
      cc = 201;
      break;
    case 188:
      cc = 200;
      break;
    case 189:
      cc = 211;
      break;
    case 190:
      cc = 212;
      break;
    case 191:
      cc = 218;
      break;
    case 192:
      cc = 217;
      break;
    case 195:
      cc = 180;
      break;
    case 198:
      cc = 181;
      break;
    case 199:
      cc = 182;
      break;
    case 200:
      cc = 188;
      break;
    case 201:
      cc = 187;
      break;
    case 204:
      cc = 185;
      break;
    case 211:
      cc = 189;
      break;
    case 212:
      cc = 190;
      break;
    case 213:
      cc = 184;
      break;
    case 214:
      cc = 183;
      break;
    case 217:
      cc = 192;
      break;
    case 218:
      cc = 191;
      break;
    case 242:
      cc = 243;
      break;
    case 243:
      cc = 242;
      break;
    }
  return (ta | cc);
}

static void
mirrorblock ()			/* left <> right mirror */
{
  int c1, c2, ty, tx1, tx2;
  for (ty = block_upper_left_y; ty <= block_lower_right_y; ty++)
    {
      tx1 = block_upper_left_x;
      tx2 = block_lower_right_x;
      while (tx1 <= tx2)
	{
	  c1 = editbuffer[(ty * 80) + tx1];
	  c2 = editbuffer[(ty * 80) + tx2];
	  editbuffer[(ty * 80) + tx1] = mirrorchar (c2);
	  editbuffer[(ty * 80) + tx2] = mirrorchar (c1);
	  tx1++;
	  tx2--;
	}
    }
}

static void
eraseblock ()
{
  int i, ty;
  for (ty = block_upper_left_y; ty <= block_lower_right_y; ty++)
    for (i = block_upper_left_x; i <= block_lower_right_x; i++)
      editbuffer[(ty * 80) + i] = 0x0720;
}

static void
transcolor (int tc, int nc)
{
  int ty, i;
  int ebg, efg;
  unsigned int c;

  for (ty = block_upper_left_y; ty <= block_lower_right_y; ty++)
    for (i = block_upper_left_x; i <= block_lower_right_x; i++)
      {
	c = editbuffer[(ty * 80) + i];
	ebg = (c & 0x7000) >> 12;
	efg = (c & 0x0700) >> 8;
	if (ebg == tc)
	  ebg = nc;
	if (efg == tc)
	  efg = nc;
	c = c & 0x88FF;
	c = c | (efg << 8);
	c = c | (ebg << 12);
	editbuffer[(ty * 80) + i] = c;
      }
  changed = 1;
}

static void
clear_status_line ()
{
  strcpy (statusline, "");
}

#define NO_BLOCK                   0
#define GET_BLOCK_START_POSITION   1
#define GET_BLOCK_END_POSITION     2
#define GET_BLOCK_COPY_DESTINATION 3

static void
doblock ()
{
  int c, ex, ty, i;
  int tc, nc;

  if (block_upper_left_x > block_lower_right_x)
    {
      c = block_lower_right_x;
      block_lower_right_x = block_upper_left_x;
      block_upper_left_x = c;
    }
  if (block_upper_left_y > block_lower_right_y)
    {
      c = block_lower_right_y;
      block_lower_right_y = block_upper_left_y;
      block_upper_left_y = c;
    }
  ex = 0;
  block = 0;
  strcpy (statusline,
	  "(S)ave,(L)oad,(I)nvert,(M)irror,(C)opy,(F)ill,(E)rase,(T)rans,"
	  "(ENTER)=Abort : ");
  status ();
  while (ex != 1)
    {
      c = mygetchar ();
      switch (c)
	{
	case 13:
	  ex = 1;
	  break;

	case 'S':		/* save block */
	case 's':
	  savefile (1);
	  ex = 1;
	  break;

	case 'l':
	case 'L':		/* load block */
	  ex = 1;
	  break;

	case 'm':
	case 'M':		/* Mirror block */
	  mirrorblock ();
	  changed = 1;
	  ex = 1;
	  break;

	case 'I':
	case 'i':		/* invert - flip block */
	  flipblock ();
	  changed = 1;
	  ex = 1;
	  break;

	case 'e':
	case 'E':		/* erase block */
	  eraseblock ();
	  changed = 1;
	  ex = 1;
	  break;

	case 'f':
	case 'F':		/* fill block */
	  doline ("(F)oreground,Bac(K)ground,(B)oth,(C)haracter : ");
	  c = mygetchar ();
	  switch (c)
	    {
	    case 'F':
	    case 'f':
	      for (ty = block_upper_left_y; ty <= block_lower_right_y; ty++)
		for (i = block_upper_left_x; i <= block_lower_right_x; i++)
		  {
		    c = editbuffer[(ty * 80) + i];
		    c = c & 0x70FF;
		    c = c | (fore << 8);
		    if (blink)
		      c = c | 0x8000;
		    editbuffer[(ty * 80) + i] = c;
		  }
	      changed = 1;
	      break;

	    case 'K':
	    case 'k':
	      for (ty = block_upper_left_y; ty <= block_lower_right_y; ty++)
		for (i = block_upper_left_x; i <= block_lower_right_x; i++)
		  {
		    c = editbuffer[(ty * 80) + i];
		    c = c & 0x8FFF;
		    c = c | (back << 12);
		    editbuffer[(ty * 80) + i] = c;
		  }
	      changed = 1;
	      break;

	    case 'B':
	    case 'b':
	      attr = fore | (back << 4);
	      if (blink)
		attr = attr | 128;
	      attr = attr << 8;
	      for (ty = block_upper_left_y; ty <= block_lower_right_y; ty++)
		for (i = block_upper_left_x; i <= block_lower_right_x; i++)
		  {
		    c = editbuffer[(ty * 80) + i] & 0xFF;
		    c = c | attr;
		    editbuffer[(ty * 80) + i] = c;
		  }
	      changed = 1;
	      break;

	    case 'C':
	    case 'c':
	      doline ("Press character to fill block with : ");
	      c = mygetchar ();
	      switch (c)
		{
		case 256:	/* F1 */
		  c = highascii[set][0];
		  break;
		case 257:	/* F2 */
		  c = highascii[set][1];
		  break;
		case 258:	/* F3 */
		  c = highascii[set][2];
		  break;
		case 259:	/* F4 */
		  c = highascii[set][3];
		  break;
		case 260:	/* F5 */
		  c = highascii[set][4];
		  break;
		case 256 + 17:	/* F6 */
		  c = highascii[set][5];
		  break;
		case 256 + 18:	/* F7 */
		  c = highascii[set][6];
		  break;
		case 256 + 19:	/* F8 */
		  c = highascii[set][7];
		  break;
		case 256 + 20:	/* F9 */
		  c = highascii[set][8];
		  break;
		case 256 + 21:	/* F10 */
		  c = highascii[set][9];
		  break;
		case 256 + 'c':	/* alt-C */
		  c = lastchar;

		default:
		  break;
		}

	      if (c > 255)
		break;

	      attr = fore | (back << 4);
	      if (blink)
		attr = attr | 128;
	      attr = attr << 8;
	      c = c | attr;
	      for (ty = block_upper_left_y; ty <= block_lower_right_y; ty++)
		for (i = block_upper_left_x; i <= block_lower_right_x; i++)
		  editbuffer[(ty * 80) + i] = c;
	      changed = 1;
	      break;

	    default:
	      break;
	    }
	  ex = 1;
	  break;

	case 'C':
	case 'c':		/* copy block */
	  doline ("Goto x and y position for upper left corner of block "
		  "and press enter.");
	  block = GET_BLOCK_COPY_DESTINATION;
	  return;

	case 't':
	case 'T':		/* translate color scheme to color scheme */
	  ex = 1;
	  doline
	    ("Select color to operate on \e[31m1 \e[32m2 \e[33m3 \e[34m4 \e[35m5 \e[36m6 \e[37m7 \e[0m: ");
	  c = mygetchar ();
	  if ((c < '0') || (c > '7'))

	    break;
	  tc = c - '0';

	  doline
	    ("Select color to convert to \e[31m1 \e[32m2 \e[33m3 \e[34m4 \e[35m5 \e[36m6 \e[37m7 \e[0m: ");
	  c = mygetchar ();
	  if ((c < '0') || (c > '7'))
	    break;
	  nc = c - '0';
	  transcolor (tc, nc);
	  changed = 1;
	  break;

	default:
	  break;

	}

    }

  clear_status_line();
  dorefresh();
}

static int
get_block_height ()
{
  /* If start and end positions are the same, we have a block of size
     one.  */
  return (block_lower_right_y - block_upper_left_y + 1);
}

static int
get_block_width ()
{
  /* If start and end positions are the same, we have a block of size
     one.  */
  return (block_lower_right_x - block_upper_left_x + 1);
}

static int
get_block_size ()
{
  return (get_block_height() * get_block_width());
}

#define EDIT_BUFFER_OFFSET(_x, _y) ((_y * EDIT_BUFFER_WIDTH) + _x)

static unsigned int
get_edit_buffer (int buffer_x, int buffer_y)
{
  return editbuffer[EDIT_BUFFER_OFFSET(buffer_x, buffer_y)];
}

static void
put_edit_buffer (int buffer_x, int buffer_y, unsigned int value)
{
  editbuffer[EDIT_BUFFER_OFFSET(buffer_x, buffer_y)] = value;
}

#define BLOCK_BUFFER_OFFSET(_x, _y) ((_y * get_block_width()) + _x)

static unsigned int
get_block_buffer (unsigned int * buffer, int buffer_x, int buffer_y)
{
  return buffer[BLOCK_BUFFER_OFFSET(buffer_x, buffer_y)];
}

static void
put_block_buffer (unsigned int * buffer, int buffer_x, int buffer_y,
		  unsigned int value)
{
  buffer[BLOCK_BUFFER_OFFSET(buffer_x, buffer_y)] = value;
}

static void
copy_block_from_edit_buffer (unsigned int * block_buffer)
{
  int buffer_x, buffer_y;

  for (buffer_y = 0; buffer_y < get_block_height(); buffer_y++)
    {
      if ((buffer_y + block_upper_left_y) >= EDIT_BUFFER_HEIGHT)
	break;

      for (buffer_x = 0; buffer_x < get_block_width(); buffer_x++)
	{
	  if ((buffer_x + block_upper_left_x) >= EDIT_BUFFER_WIDTH)
	    break;
	  
	  put_block_buffer(block_buffer, buffer_x, buffer_y,
			   get_edit_buffer(buffer_x + block_upper_left_x,
					   buffer_y + block_upper_left_y));
	}
    }
}

static void
copy_block_to_edit_buffer(unsigned int * block_buffer, int destination_x,
			  int destination_y)
{
  int block_x, block_y;

  for (block_y = 0; block_y < get_block_height(); block_y++)
    {
      if ((block_y + destination_y) >= EDIT_BUFFER_HEIGHT)
	break;

      for (block_x = 0; block_x < get_block_width(); block_x++)
	{	
	  if ((block_x + destination_x) >= EDIT_BUFFER_WIDTH)
	    break;

	  put_edit_buffer(block_x + destination_x, block_y + destination_y,
			  get_block_buffer(block_buffer, block_x, block_y));
	}
    }
}

static void
copyblock ()
{
  unsigned int * block_buffer;

  assert(block_upper_left_x <= block_lower_right_x);
  assert(block_upper_left_y <= block_lower_right_y);

  block_buffer = (unsigned int *) malloc(get_block_size() * sizeof(unsigned int));

  if (block_buffer == NULL)
    {
      doline ("Block copy operation aborted: Malloc failed.");
      mygetchar();
      return;
    }

  copy_block_from_edit_buffer(block_buffer);
  copy_block_to_edit_buffer(block_buffer, x, edit_buffer_y);

  free(block_buffer);
}

static void
editloop ()
{
  lastchar = 32;

  for (;;)
    {
      int c;
      c = mygetchar ();
      switch (c)
	{
	case 'l':
	case 'L':
	  if ((block != NO_BLOCK) && (block != GET_BLOCK_COPY_DESTINATION))
	    {
	      /*
	       * Block start and end position are already in the global
	       * variables. Just reuse them.
	       */
	      doblock ();
	    }
	  else
	    {
	      dochar (c);
	    }
	  break;

	case 8:
	case 127:
	  if (x > 0)
	    {
	      x = x - 1;
	      editbuffer[(edit_buffer_y * 80) + x] = 0x0720;
	      cgoto ();
	      printf ("\e[0m");
	      putc (' ', stdout);
	      ccolor ();
	      cgoto ();
	    }
	  break;
	case 13:
	  if (block == GET_BLOCK_COPY_DESTINATION)
	    {
	      copyblock();

	      block = NO_BLOCK;
	      changed = 1;

	      clear_status_line();
	      dorefresh();
	    }
	  else
	    {
	      docr ();
	    }
	  break;

	case PAGEUP:
	  ansipageup ();
	  status ();
	  break;

	case PAGEDOWN:
	  ansipagedown ();
	  status ();
	  break;

	case HOME:
	  if (x > 0)
	    x = 0;
	  else
	    {
	      ansihome ();
	      edit_buffer_y = 0;
	      y = 0;
	    }
	  status ();
	  break;

	case INSERT:
	  if (insert)
	    insert = 0;
	  else
	    insert = 1;
	  status ();
	  break;

	case DELETE:
	  dodelete ();
	  break;

	case END:
	  if (x < (EDIT_BUFFER_WIDTH - 1))
	    x = EDIT_BUFFER_WIDTH - 1;
	  else
	    {
	      ansiend ();
	      edit_buffer_y = EDIT_BUFFER_HEIGHT - 1;
	      y = SCREEN_HEIGHT - 1;
	    }
	  status ();
	  break;


	case CRSLEFT:
	  if (x > 0)
	    {
	      newdir = 3;
	      if (linedraw)
		dolinedraw ();
	      x = x - 1;
	      if ((text) && !(linedraw))
		doattr ();
	      printxy ();
	    }
	  else if (edit_buffer_y > 0)
	    {
	      x = 79;
	      edit_buffer_y = edit_buffer_y - 1;
	      if (y > 0)
		y = y - 1;
	      else
		ansiscrollup ();
	      newdir = 3;
	      if (linedraw)
		dolinedraw ();
	      if ((text) && !(linedraw))
		doattr ();
	      printxy ();
	    }
	  break;

	case CRSRIGHT:

	  if (x < 79)
	    {
	      newdir = 4;
	      if (linedraw)
		dolinedraw ();
	      x = x + 1;
	      if ((text) && !(linedraw))
		doattr ();
	      printxy ();
	    }
	  else if (edit_buffer_y < 999)
	    {
	      edit_buffer_y = edit_buffer_y + 1;
	      x = 0;
	      newdir = 4;
	      if (y < 22)
		y = y + 1;
	      else
		ansiscrolldown ();
	      if (linedraw)
		dolinedraw ();
	      if ((text) && !(linedraw))
		doattr ();
	      printxy ();
	    }
	  break;

	case CRSDOWN:

	  if (edit_buffer_y < 999)
	    {
	      newdir = 2;
	      if (linedraw)
		dolinedraw ();


	      edit_buffer_y = edit_buffer_y + 1;

	      if (y < 22)
		{
		  y = y + 1;
		  printxy ();
		}
	      else
		{
		  ansiscrolldown ();
		  status ();
		}

	      if ((text) && !(linedraw))
		doattr ();

	    }
	  break;

	case CRSUP:
	  if (edit_buffer_y > 0)
	    {
	      newdir = 1;
	      if (linedraw)
		dolinedraw ();


	      edit_buffer_y = edit_buffer_y - 1;
	      if (y > 0)
		{
		  y = y - 1;
		  printxy ();
		}
	      else
		{
		  ansiscrollup ();
		  status ();
		}

	      if ((text) && !(linedraw))
		doattr ();

	    }
	  break;

	case 256:		/* F1 */
	  dochar (highascii[set][0]);
	  break;
	case 257:		/* F2 */
	  dochar (highascii[set][1]);
	  break;
	case 258:		/* F3 */
	  dochar (highascii[set][2]);
	  break;
	case 259:		/* F4 */
	  dochar (highascii[set][3]);
	  break;
	case 260:		/* F5 */
	  dochar (highascii[set][4]);
	  break;
	case 256 + 17:		/* F6 */
	  dochar (highascii[set][5]);
	  break;
	case 256 + 18:		/* F7 */
	  dochar (highascii[set][6]);
	  break;
	case 256 + 19:		/* F8 */
	  dochar (highascii[set][7]);
	  break;
	case 256 + 20:		/* F9 */
	  dochar (highascii[set][8]);
	  break;
	case 256 + 21:		/* F10 */
	  dochar (highascii[set][9]);
	  break;

	case 256 + '<':		/* alt-< */
	  if (set > 0)
	    set = set - 1;
	  status ();
	  break;
	case 256 + '>':		/* alt-> */
	  if (set < 14)
	    set = set + 1;
	  status ();
	  break;

	case 256 + 'a':		/* alt-a ? */
	  if (text)
	    text = 0;
	  else
	    text = 1;
	  status ();
	  break;
	case 256 + 'b':		/* alt-b block mode */
	  strcpy (statusline,
		  "Use (L)ast block or move to upper left corner and "
		  "press space.");
	  block = GET_BLOCK_START_POSITION;
	  linedraw = 0;
	  text = 0;
	  status ();
	  break;

	case 256 + 'c':		/* alt-c use character */
	  dochar (lastchar);
	  break;

	case 256 + 'd':		/* toggle line drawing mode */
	  if (linedraw)
	    linedraw = 0;
	  else
	    linedraw = 1;
	  status ();
	  break;

	case 256 + 'e':		/* erase page layer */
	  sprintf (input, "Erase page %d ? ", page + 1);
	  doline (input);
	  if (noyes ())
	    {
	      clr ();
	      ansihome ();
	      fixvars ();
	    }
	  status ();
	  break;

	case 256 + 'f':		/* color selection */
	  printf ("\e[23;1H\e[0m\e[K\e[24;1H\e[K\e[25;1H\e[K\e[24;1H");
	  printf ("\e[0;37;40mSelect colors");
	  colorbar ();
	  dorefresh ();
	  break;

	case 256 + 'g':		/* ? */
	  break;

	case 256 + 'h':		/* help screen */
	  dohelp ();
	  break;

	case 256 + 'i':		/* insert line */
	  insertline ();
	  break;

	case 256 + 'j':		/* insert column */
	  insertcolumn ();
	  break;

	case 256 + 'k':		/* delete column */
	  deletecolumn ();
	  break;

	case 256 + 'l':		/* loadfile */
	  loadfile ();
	  break;

	case 256 + 'm':		/* ? */
	  break;

	case 256 + 'n':		/* ? */
	  break;

	case 256 + 'o':		/* ? */
	  break;

	case 256 + 'p':		/* ? */
	  lastchar = editbuffer[(edit_buffer_y * 80) + x] & 0xFF;
	  break;

	case 256 + 'q':		/* ? */
	  break;

	case 256 + 'r':		/* ? */
	  break;

	case 256 + 's':		/* save file */
	  savefile (0);
	  break;

	case 256 + 't':		/* ? */
	  break;

	case 256 + 'u':		/* alt-u */
	  attr = editbuffer[(edit_buffer_y * 80) + x];
	  attr = attr >> 8;
	  fore = attr & 15;
	  back = (attr >> 4) & 7;
	  if (attr & 128)
	    blink = 1;
	  else
	    blink = 0;
	  status ();
	  break;

	case 256 + 'v':		/* ? */
	  break;

	case 256 + 'w':		/* ? */
	  break;

	case 256 + 'x':		/* exit */
	  doline ("Quit Duh DRAW ? ");
	  if (noyes ())
	    {
	      if (changed)
		savefile (0);
	      return;
	    }
	  status ();
	  break;

	case 256 + 'y':		/* delete (yank line ) */
	  deleteline ();
	  break;

	case 256 + 'z':		/* ? */
	  break;

	case 256 + 23:		/* shift F1 */
	  set = 0;
	  status ();
	  break;
	case 256 + 24:		/* shift F2 */
	  set = 1;
	  status ();
	  break;
	case 256 + 25:		/* shift F3 */
	  set = 2;
	  status ();
	  break;
	case 256 + 26:		/* shift F4 */
	  set = 3;
	  status ();
	  break;
	case 256 + 28:		/* shift F5 */
	  set = 4;
	  status ();
	  break;
	case 256 + 29:		/* shift F6 */
	  set = 5;
	  status ();
	  break;
	case 256 + 31:		/* shift F7 */
	  set = 6;
	  status ();
	  break;
	case 256 + 32:		/* shift F8 */
	  set = 7;
	  status ();
	  break;
	case 256 + 33:		/* shift F9 */
	  set = 8;
	  status ();
	  break;
	case 256 + 34:		/* shift F10 */
	  set = 9;
	  status ();
	  break;

	case 32:
	  if (block == GET_BLOCK_END_POSITION)
	    {
	      block_lower_right_y = edit_buffer_y;
	      block_lower_right_x = x;
	      doblock ();
	    }
	  else if (block == GET_BLOCK_START_POSITION)
	    {
	      block_upper_left_y = edit_buffer_y;
	      block_upper_left_x = x;
	      block = GET_BLOCK_END_POSITION;
	      strcpy (statusline,
		      "Move to lower right corner and press space.");
	      status ();
	    }
	  else
	    {
	      dochar (32);
	    }
	  break;

	default:
	  if ((c > 32) && (c < 128) && (block == 0))
	    {
	      dochar (c);
	    }
	  break;
	}
    }
}

int
main (int argc, char *argv[])
{
  savetty ();
  initscr ();

  noecho ();			/* shuttoff tty echoing */
  nonl ();			/* don't wait for carriage return */
  cbreak ();

  printf ("\e(U");		/* set linux virtual console to IBM_PC graphics set */
  clr ();			/* clear buffer and screen */

  strcpy (filename, "");
  if (argc > 1)
    {
      if (strcmp (argv[1], "-help") == 0)
	dohelp ();
      else
	{
	  strcpy (filename, argv[1]);
	  readfile ();
	}
    }

  printf ("\e[0m\e[2J\e[1;34m");
  printf ("%s", screendata);

  mygetchar ();
  fixvars ();
  editloop ();
  printf ("\e(B");		/* set unix character set */


  printf ("\e[0m\e[2J\e[8;32H");
  printf ("\e[0;1;32;40mThanks for using\e[0;1;37;40m\e[11;36HDuh Draw");
  printf
    ("\e[13;39H\e[0;1;32;40mby\e[15;35H\e[1;37;40mBen Fowler\e[0m\n\n\r");
  resetty ();

  endwin ();			/* put the terminal back to normal */

  return 0;
}
