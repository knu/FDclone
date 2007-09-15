#
#	Makefile for fd
#

VERSION	= 2
VERMAJ	= $(VERSION)
PREFIX	= /usr/local
CONFDIR	= /etc
BUILD	=
SHELL	= /bin/sh
MAKE	= make
CC	= cc
HOSTCC	= $(CC)
SED	= sed
RM	= rm -f
DICTSRC	=

DEFCFLAGS = -DPREFIX='"'$(PREFIX)'"' \
	-DCONFDIR='"'$(CONFDIR)'"' \
	-DDICTSRC='"''$(DICTSRC)''"' \
	-DFD=$(VERMAJ) \
	-DCCCOMMAND='"'$(CC)'"' \
	-DHOSTCCCOMMAND='"'$(HOSTCC)'"'

all: Makefile.tmp
	$(MAKE) -f Makefile.tmp

debug: Makefile.tmp
	$(MAKE) CC=gcc DEBUG=-DDEBUG ALLOC='-L. -lmalloc' -f Makefile.tmp

shdebug: Makefile.tmp
	$(MAKE) CC=gcc DEBUG=-DDEBUG ALLOC='-L. -lmalloc' -f Makefile.tmp sh

Makefile.tmp: Makefile.in mkmf.sed
	$(SED) -f mkmf.sed Makefile.in > $@ || \
	($(RM) $@; exit 1)

makefile.gpc: Makefile.in mkmfdosg.sed mkmf.sed
	$(SED) -f mkmfdosg.sed Makefile.in | \
	$(SED) "s/__OSTYPE__/DOSV/g" | \
	$(SED) -f mkmf.sed > $@ || \
	($(RM) $@; exit 1)

makefile.g98: Makefile.in mkmfdosg.sed mkmf.sed
	$(SED) -f mkmfdosg.sed Makefile.in | \
	$(SED) "s/__OSTYPE__/PC98/g" | \
	$(SED) -f mkmf.sed > $@ || \
	($(RM) $@; exit 1)

makefile.dpc: Makefile.in mkmfdosd.sed mkmf.sed
	$(SED) -f mkmfdosd.sed Makefile.in | \
	$(SED) "s/__OSTYPE__/DOSV/g" | \
	$(SED) -f mkmf.sed > $@ || \
	($(RM) $@; exit 1)

makefile.d98: Makefile.in mkmfdosd.sed mkmf.sed
	$(SED) -f mkmfdosd.sed Makefile.in | \
	$(SED) "s/__OSTYPE__/PC98/g" | \
	$(SED) -f mkmf.sed > $@ || \
	($(RM) $@; exit 1)

makefile.lpc: Makefile.in mkmfdosl.sed mkmf.sed
	$(SED) -f mkmfdosl.sed Makefile.in | \
	$(SED) "s/__OSTYPE__/DOSV/g" | \
	$(SED) -f mkmf.sed > $@ || \
	($(RM) $@; exit 1)

makefile.l98: Makefile.in mkmfdosl.sed mkmf.sed
	$(SED) -f mkmfdosl.sed Makefile.in | \
	$(SED) "s/__OSTYPE__/PC98/g" | \
	$(SED) -f mkmf.sed > $@ || \
	($(RM) $@; exit 1)

makefile.bpc: Makefile.in mkmfdosb.sed mkmf.sed
	$(SED) -f mkmfdosb.sed Makefile.in | \
	$(SED) "s/__OSTYPE__/DOSV/g" | \
	$(SED) -f mkmf.sed > $@ || \
	($(RM) $@; exit 1)

makefile.b98: Makefile.in mkmfdosb.sed mkmf.sed
	$(SED) -f mkmfdosb.sed Makefile.in | \
	$(SED) "s/__OSTYPE__/PC98/g" | \
	$(SED) -f mkmf.sed > $@ || \
	($(RM) $@; exit 1)

mkmf.sed: mkmfsed
	./mkmfsed > mkmf.sed

mkmfsed: mkmfsed.c fd.h machine.h config.h version.h
	$(HOSTCC) $(CFLAGS) $(CPPFLAGS) $(DEFCFLAGS) -o $@ mkmfsed.c

fd.h:
	-if [ ! -f $@ ]; then \
		echo '#include <stdio.h>' > fd.h; \
		echo '#include <string.h>' >> fd.h; \
		echo '#include "machine.h"' >> fd.h; \
	fi

config.h: config.hin
	cp config.hin config.h

install catman catman-b compman compman-b \
jcatman jcatman-b jcompman jcompman-b: Makefile.tmp
	$(MAKE) BUILD=$(BUILD) -f Makefile.tmp $@

sh bsh \
fd.doc README.doc HISTORY.doc FAQ.doc LICENSES.doc \
depend: Makefile.tmp
	$(MAKE) -f Makefile.tmp $@

config: rmconfig Makefile.tmp
	$(MAKE) SHELL=$(SHELL) -f Makefile.tmp $@

rmconfig:
	cp config.hin config.h

ipk: Makefile.tmp
	$(MAKE) STRIP=$(STRIP) -f Makefile.tmp $@

everything: Makefile.tmp
	$(MAKE) -f Makefile.tmp sh bsh all

tar gtar shtar lzh shar: Makefile.tmp \
makefile.gpc makefile.g98 \
makefile.dpc makefile.d98 \
makefile.lpc makefile.l98 \
makefile.bpc makefile.b98
	$(MAKE) -f Makefile.tmp $@

clean rmdict: Makefile.tmp
	$(MAKE) -f Makefile.tmp $@
	-$(RM) Makefile.tmp mkmf.sed
	-$(RM) mkmfsed mkmfsed.exe

realclean: Makefile.tmp
	$(MAKE) -f Makefile.tmp clean
	-$(RM) Makefile.tmp mkmf.sed config.h
	-$(RM) makefile.gpc makefile.g98
	-$(RM) makefile.dpc makefile.d98
	-$(RM) makefile.lpc makefile.l98
	-$(RM) makefile.bpc makefile.b98
	-$(RM) mkmfsed mkmfsed.exe
