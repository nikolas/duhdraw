/****************************************************************************
* 
*          ansitoc.c -  convert an ANSI file to c data format. 
*          
*                    (c) Copyright 1996 Ben Fowler
*                          All Rights Reserved
*
****************************************************************************/

#include <stdio.h>
#include <string.h>

/* convert ansi to c data statement. ansi would be displayed with
*  printf("%s",screendata);
*/

int
main (int argc, char *argv[])
{
  int count, c;
  FILE *in;
  FILE *out;
  char infile[80];
  char outfile[80];

  if (argc != 3)
    {
      printf ("\n%s (c) Copyright 1996 Ben Fowler.\n", argv[0]);
      printf ("Purpose : Convert an ANSI file to c data statements.\n");
      printf ("Usage : %s infile outfile\n\n", argv[0]);
      return 1;
    }

  strcpy (infile, argv[1]);
  strcpy (outfile, argv[2]);
  in = fopen (infile, "rb");
  out = fopen (outfile, "wb");

  count = 0;

  fprintf (out, "/* \n");
  fprintf (out,
	   "* c data file converted from ansi file by ansitoc (c) 1996 Ben Fowler\n");
  fprintf (out, "*/\n\n");
  fprintf (out, "char screendata[]={ ");

  while ((c = fgetc (in)) != EOF)
    {
      fprintf (out, "%d,", c);
      count++;
      if (count > 12)
	{
	  count = 0;
	  fprintf (out, "\n                   ");
	}
    }

  fprintf (out, "0 };\n");
  fclose (in);
  fclose (out);

  return 0;

}
