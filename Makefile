#
#	Makefile for fd
#

MAKE	= make
CC	= cc
CPP	= cc -E
SED	= sed

goal:	Makefile.tmp
	$(MAKE) -f Makefile.tmp

Makefile.tmp: Makefile.in mkmakefile.sed
	$(SED) -f mkmakefile.sed Makefile.in > Makefile.tmp

mkmakefile.sed: mkmakefile.sed.c machine.h
	$(CPP) mkmakefile.sed.c | $(SED) "/^#/d" > mkmakefile.sed

install: Makefile.tmp
	$(MAKE) -f Makefile.tmp install

depend: Makefile.tmp
	$(MAKE) -f Makefile.tmp depend

test: Makefile.tmp
	$(MAKE) -f Makefile.tmp test

getkey: Makefile.tmp
	$(MAKE) -f Makefile.tmp getkey

tar: Makefile.tmp
	$(MAKE) -f Makefile.tmp tar

lzh: Makefile.tmp
	$(MAKE) -f Makefile.tmp lzh

shar: Makefile.tmp
	$(MAKE) -f Makefile.tmp shar

clean: Makefile.tmp
	$(MAKE) -f Makefile.tmp clean
