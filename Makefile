#
#	Makefile for fd
#

MAKE	= make
CC	= cc
CPP	= $(CC) -E
SED	= sed

goal:	Makefile.tmp
	$(MAKE) -f Makefile.tmp

Makefile.tmp: Makefile.in mkmf.sed
	$(SED) -f mkmf.sed Makefile.in > $@ ||\
	(rm -f $@; exit 1)

makefile.gpc: Makefile.in mkmfdosg.sed
	$(SED) -f mkmfdosg.sed Makefile.in |\
	$(SED) 's/__OSTYPE__/DOSV/g' > $@ ||\
	(rm -f $@; exit 1)

makefile.g98: Makefile.in mkmfdosg.sed
	$(SED) -f mkmfdosg.sed Makefile.in |\
	$(SED) 's/__OSTYPE__/PC98/g' > $@ ||\
	(rm -f $@; exit 1)

makefile.lpc: Makefile.in mkmfdosl.sed
	$(SED) -f mkmfdosl.sed Makefile.in |\
	$(SED) 's/__OSTYPE__/DOSV/g' > $@ ||\
	(rm -f $@; exit 1)

makefile.l98: Makefile.in mkmfdosl.sed
	$(SED) -f mkmfdosl.sed Makefile.in |\
	$(SED) 's/__OSTYPE__/PC98/g' > $@ ||\
	(rm -f $@; exit 1)

mkmf.sed: mkmfsed.c machine.h config.h
	$(CPP) -DCCCOMMAND=$(CC) mkmfsed.c |\
	$(SED) -n -e 's/[\" 	]*:[\" 	]*/:/g' -e '/^s:.*:.*:g*$$/p' > mkmf.sed

config.h: config.hin
	cp config.hin config.h

install catman catman-b compman compman-b \
fd.doc history.doc \
depend config: Makefile.tmp
	$(MAKE) -f Makefile.tmp $@

tar lzh shar: Makefile.tmp makefile.gpc makefile.g98 \
makefile.lpc makefile.l98
	$(MAKE) -f Makefile.tmp $@

clean: Makefile.tmp
	$(MAKE) -f Makefile.tmp clean
	rm -f makefile.gpc makefile.g98 Makefile.tmp
	rm -f makefile.lpc makefile.l98 mkmf.sed config.h
