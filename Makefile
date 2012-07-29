#
#	Makefile for fd
#

VERSION	= 3
VERMAJ	= $(VERSION)
PREFIX	= /usr/local
CONFDIR	= /etc
BUILD	=
SHELL	= /bin/sh
MAKE	= make
CC	= cc
HOSTCC	= $(CC)
CPP	= $(CC) -E
CFLAGS	=
HOSTCFLAGS =
CPPFLAGS =
HOSTCPPFLAGS =
LDFLAGS	=
HOSTLDFLAGS =
SED	= sed
ECHO	= echo
CP	= cp
RM	= rm -f
TAR	= tar
LHA	= lha
SHAR	= shar
DICTSRC	=

DEFCFLAGS = -DPREFIX='"'$(PREFIX)'"' \
	-DCONFDIR='"'$(CONFDIR)'"' \
	-DDICTSRC='"''$(DICTSRC)''"' \
	-DFD=$(VERMAJ) \
	-DCCCOMMAND='"'$(CC)'"' \
	-DHOSTCCCOMMAND='"'$(HOSTCC)'"' \
	-DCFLAGS='"''$(CFLAGS)''"' \
	-DHOSTCFLAGS='"''$(HOSTCFLAGS)''"' \
	-DCPPFLAGS='"''$(CPPFLAGS)''"' \
	-DHOSTCPPFLAGS='"''$(HOSTCPPFLAGS)''"' \
	-DLDFLAGS='"''$(LDFLAGS)''"' \
	-DHOSTLDFLAGS='"''$(HOSTLDFLAGS)''"'
DEFAR	= TAR=$(TAR) \
	LHA=$(LHA) \
	SHAR=$(SHAR)
PREDEF	= alpha __alpha __alpha__ \
	ia64 __ia64 __ia64__ x86_64 __x86_64 __x86_64__ \
	s390x __s390x __s390x__ CONFIG_ARCH_S390X \
	PPC __powerpc__ _COMPILER_VERSION \
	SYSTYPE_BSD SYSTYPE_SYSV SYSTYPE_SVR4 _SYSTYPE_SVR4 \
	USGr4 __svr4__ __SVR4 BSD bsd43 __WIN32__ \
	sun __SUNPRO_C sony __sony sgi \
	hpux __hpux __H3050 __H3050R __H3050RX \
	__CLASSIC_C__ __STDC_EXT__ __HP_cc \
	nec_ews _nec_ews nec_ews_svr4 _nec_ews_svr4 \
	uniosu uniosb luna88k _IBMR2 _AIX41 _AIX43 \
	ultrix _AUX_SOURCE DGUX __DGUX__ \
	__uxpm__ __uxps__ NeXT __CYGWIN__ linux \
	__FreeBSD__ __BOW__ __NetBSD__ NetBSD1_0 __NetBSD_Version__ \
	__bsdi__ __386BSD__ __BSD_NET2 __OpenBSD__ \
	__APPLE__ __MACH__ __minix _VCS_REVISION __ANDROID__ __BIONIC__ mips \
	__GNUC__ __GNUC_MINOR__ __clang__
HPREFIX	= H_
MAKES	= makefile.gpc makefile.g98 \
	makefile.dpc makefile.d98 \
	makefile.lpc makefile.l98 \
	makefile.bpc makefile.b98

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
	$(SED) "s:__OSTYPE__:DOSV:g" | \
	$(SED) -f mkmf.sed > $@ || \
	($(RM) $@; exit 1)

makefile.g98: Makefile.in mkmfdosg.sed mkmf.sed
	$(SED) -f mkmfdosg.sed Makefile.in | \
	$(SED) "s:__OSTYPE__:PC98:g" | \
	$(SED) -f mkmf.sed > $@ || \
	($(RM) $@; exit 1)

makefile.dpc: Makefile.in mkmfdosd.sed mkmf.sed
	$(SED) -f mkmfdosd.sed Makefile.in | \
	$(SED) "s:__OSTYPE__:DOSV:g" | \
	$(SED) -f mkmf.sed > $@ || \
	($(RM) $@; exit 1)

makefile.d98: Makefile.in mkmfdosd.sed mkmf.sed
	$(SED) -f mkmfdosd.sed Makefile.in | \
	$(SED) "s:__OSTYPE__:PC98:g" | \
	$(SED) -f mkmf.sed > $@ || \
	($(RM) $@; exit 1)

makefile.lpc: Makefile.in mkmfdosl.sed mkmf.sed
	$(SED) -f mkmfdosl.sed Makefile.in | \
	$(SED) "s:__OSTYPE__:DOSV:g" | \
	$(SED) -f mkmf.sed > $@ || \
	($(RM) $@; exit 1)

makefile.l98: Makefile.in mkmfdosl.sed mkmf.sed
	$(SED) -f mkmfdosl.sed Makefile.in | \
	$(SED) "s:__OSTYPE__:PC98:g" | \
	$(SED) -f mkmf.sed > $@ || \
	($(RM) $@; exit 1)

makefile.bpc: Makefile.in mkmfdosb.sed mkmf.sed
	$(SED) -f mkmfdosb.sed Makefile.in | \
	$(SED) "s:__OSTYPE__:DOSV:g" | \
	$(SED) -f mkmf.sed > $@ || \
	($(RM) $@; exit 1)

makefile.b98: Makefile.in mkmfdosb.sed mkmf.sed
	$(SED) -f mkmfdosb.sed Makefile.in | \
	$(SED) "s:__OSTYPE__:PC98:g" | \
	$(SED) -f mkmf.sed > $@ || \
	($(RM) $@; exit 1)

mkmf.sed: mkmfsed
	./mkmfsed > mkmf.sed

mkmfsed: mkmfsed.c
	$(HOSTCC) $(HOSTCFLAGS) $(HOSTCPPFLAGS) $(DEFCFLAGS) -o $@ mkmfsed.c

mkmfsed: hmachine.h config.h fd.h headers.h machine.h
mkmfsed: depend.h version.h

fd.h:
	-[ -f $@ ] || $(CP) headers.h $@

config.h: config.hin
	$(CP) config.hin config.h

hmachine.h: Makefile machine.h hmachine.sed
	$(SED) -n -e 's:machine.h:$@:g' -e '1,/^$$/p' machine.h > $@
	@if [ "$(CC)" != "$(HOSTCC)" ]; then \
		for i in $(PREDEF); do \
			$(ECHO) "#ifdef	$${i}"; \
			$(ECHO) "__DEFINE__	$(HPREFIX)$${i} $${i}"; \
			$(ECHO) "#endif"; \
		done | $(CPP) - | \
		$(SED) -n -e 's:__DEFINE__:#define:p' >> $@; \
		$(ECHO) >> $@; \
	fi
	$(SED) -f hmachine.sed machine.h >> $@

hmachine.sed: Makefile
	$(ECHO) '1,/^$$/d' > $@
	@if [ "$(CC)" != "$(HOSTCC)" ]; then \
		for i in $(PREDEF); do \
			$(ECHO) "s:($${i}):($(HPREFIX)$${i}):g"; \
		done >> $@; \
	fi

install install-table install-man install-jman \
catman catman-b compman compman-b \
jcatman jcatman-b jcompman jcompman-b: Makefile.tmp
	$(MAKE) BUILD=$(BUILD) -f Makefile.tmp $@

sh bsh nsh \
fd.doc README.doc HISTORY.doc FAQ.doc LICENSES.doc \
depend lint: Makefile.tmp
	$(MAKE) -f Makefile.tmp $@

config: rmconfig Makefile.tmp
	$(MAKE) SHELL=$(SHELL) -f Makefile.tmp $@

rmconfig:
	$(CP) config.hin config.h

ipk: Makefile.tmp
	$(MAKE) STRIP=$(STRIP) -f Makefile.tmp $@

everything: Makefile.tmp
	$(MAKE) -f Makefile.tmp sh bsh nsh all

makes: $(MAKES)

tar gtar shtar lzh shar: Makefile.tmp $(MAKES)
	$(MAKE) $(DEFAR) -f Makefile.tmp $@

rmdict: Makefile.tmp
	$(MAKE) -f Makefile.tmp $@
	-$(RM) Makefile.tmp mkmf.sed
	-$(RM) mkmfsed mkmfsed.exe

clean: Makefile.tmp
	$(MAKE) -f Makefile.tmp $@
	-$(RM) Makefile.tmp mkmf.sed
	-$(RM) mkmfsed mkmfsed.exe
	-$(RM) hmachine.h hmachine.sed

distclean: clean
	-$(RM) config.h

realclean: distclean
	-$(RM) makefile.gpc makefile.g98
	-$(RM) makefile.dpc makefile.d98
	-$(RM) makefile.lpc makefile.l98
	-$(RM) makefile.bpc makefile.b98
