#
#	Makefile for fd
#

MAKE	= make
CC	= cc
CPP	= $(CC) -E
SHELL	= /bin/sh
SED	= sed

goal:	Makefile.tmp
	$(MAKE) -f Makefile.tmp

Makefile.tmp: Makefile.in mkmf.sed
	$(SED) -f mkmf.sed Makefile.in > Makefile.tmp ||\
	(rm -f Makefile.tmp; exit 1)

mkmf.sed: mkmf.sed.c machine.h config.h
	$(CPP) -DCCCOMMAND=$(CC) mkmf.sed.c |\
	$(SED) -n -e 's/[\" 	]*:[\" 	]*/:/g' -e '/^s:.*:.*:$$/p' > mkmf.sed

config.h: config.h.in
	cp config.h.in config.h

install catman catman-b compman compman-b \
depend config tar lzh shar: Makefile.tmp
	$(MAKE) -f Makefile.tmp $@

clean: Makefile.tmp
	$(MAKE) -f Makefile.tmp clean
	rm -f Makefile.tmp mkmf.sed config.h
