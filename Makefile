#
#	Makefile for fd
#

MAKE	= make
CC	= cc
CPP	= $(CC) -E
SED	= sed

goal:	Makefile.tmp
	$(MAKE) -f Makefile.tmp CC=$(CC)

Makefile.tmp: Makefile.in mkmf.sed
	$(SED) -f mkmf.sed Makefile.in > Makefile.tmp

mkmf.sed: mkmf.sed.c machine.h
	$(CPP) mkmf.sed.c | $(SED) -e '/^#/d' -e 's/\"//g' > mkmf.sed

install: Makefile.tmp
	$(MAKE) -f Makefile.tmp install

depend: Makefile.tmp
	$(MAKE) -f Makefile.tmp depend

test: Makefile.tmp
	$(MAKE) -f Makefile.tmp test CC=$(CC)

getkey: Makefile.tmp getkey.o term.o
	$(MAKE) -f Makefile.tmp getkey CC=$(CC)

tar: Makefile.tmp
	$(MAKE) -f Makefile.tmp tar

lzh: Makefile.tmp
	$(MAKE) -f Makefile.tmp lzh

shar: Makefile.tmp
	$(MAKE) -f Makefile.tmp shar

clean: Makefile.tmp
	$(MAKE) -f Makefile.tmp clean
