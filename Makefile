# These are the production settings
CC=gcc -O2 -s -Wall
STRIP=strip
LIBS=-lncurses

# I use these to debug
# CC=g++ -g -O0
# STRIP=ls
# LIBS=-lncurses_g

all:	duhdraw ansitoc ansi

ansi:	cleanansi
	$(CC) -o ansi ansi.c ansi-esc.c $(LIBS)
	$(STRIP) ansi

duhdraw:	cleandd	
	$(CC) -o duhdraw duhdraw.c ansi-esc.c $(LIBS) 
	$(STRIP) duhdraw

ansitoc:	cleanansitoc	
	$(CC) -o ansitoc ansitoc.c
	$(STRIP) ansitoc

cleanansi:	
	rm -f ansi

cleanansitoc:	
	rm -f ansitoc

cleandd:	
	rm -f duhdraw
 
clean:	cleanansi cleanansitoc cleandd
	rm -f *~
