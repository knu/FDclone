#
#	Makefile for fd
#

VERSION	= 2
PREFIX	= /usr/local
BUILD	=
SHELL	= /bin/sh
MAKE	= make
CC	= cc
HOSTCC	= $(CC)
SED	= sed

all: Makefile.tmp
	$(MAKE) -f Makefile.tmp

debug: Makefile.tmp
	$(MAKE) CC=gcc DEBUG=-DDEBUG ALLOC='-L. -lmalloc' -f Makefile.tmp

shdebug: Makefile.tmp
	$(MAKE) CC=gcc DEBUG=-DDEBUG ALLOC='-L. -lmalloc' -f Makefile.tmp sh

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

makefile.dpc: Makefile.in mkmfdosd.sed
	$(SED) -f mkmfdosd.sed Makefile.in |\
	$(SED) 's/__OSTYPE__/DOSV/g' > $@ ||\
	(rm -f $@; exit 1)

makefile.d98: Makefile.in mkmfdosd.sed
	$(SED) -f mkmfdosd.sed Makefile.in |\
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

makefile.bpc: Makefile.in mkmfdosb.sed
	$(SED) -f mkmfdosb.sed Makefile.in |\
	$(SED) 's/__OSTYPE__/DOSV/g' > $@ ||\
	(rm -f $@; exit 1)

makefile.b98: Makefile.in mkmfdosb.sed
	$(SED) -f mkmfdosb.sed Makefile.in |\
	$(SED) 's/__OSTYPE__/PC98/g' > $@ ||\
	(rm -f $@; exit 1)

mkmf.sed: mkmfsed
	./mkmfsed > mkmf.sed

mkmfsed: mkmfsed.c fd.h machine.h config.h
	$(HOSTCC) -DFD=$(VERSION) $(CFLAGS) -DCCCOMMAND='"$(CC)"' -DHOSTCCCOMMAND='"$(HOSTCC)"' -DPREFIX='"'$(PREFIX)'"' -o $@ mkmfsed.c

config.h: config.hin
	cp config.hin config.h

install catman catman-b compman compman-b \
ecatman ecatman-b ecompman ecompman-b: Makefile.tmp
	$(MAKE) BUILD=$(BUILD) -f Makefile.tmp $@

fd.doc README.doc HISTORY.doc FAQ.doc LICENSES.doc \
depend sh bsh clean: Makefile.tmp
	$(MAKE) -f Makefile.tmp $@

config: rmconfig Makefile.tmp
	$(MAKE) SHELL=$(SHELL) -f Makefile.tmp $@

rmconfig:
	cp config.hin config.h

ipk: Makefile.tmp
	$(MAKE) STRIP=$(STRIP) -f Makefile.tmp $@

everything: Makefile.tmp
	$(MAKE) -f Makefile.tmp sh bsh all

tar gtar shtar lzh shar: Makefile.tmp makefile.gpc makefile.g98 \
makefile.dpc makefile.d98 \
makefile.lpc makefile.l98 \
makefile.bpc makefile.b98
	$(MAKE) -f Makefile.tmp $@

realclean: Makefile.tmp
	$(MAKE) -f Makefile.tmp clean
	-rm -f Makefile.tmp mkmf.sed config.h
	-rm -f makefile.gpc makefile.g98
	-rm -f makefile.dpc makefile.d98
	-rm -f makefile.lpc makefile.l98
	-rm -f mkmfsed
