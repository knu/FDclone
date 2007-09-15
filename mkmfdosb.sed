s:/:\\:g
s:__PREFIX__::
s:__CONFDIR__::
s:__EXE__:.exe:g
s:__OBJ__:.obj:g
s:__EMUOBJS__:unixemu.obj unixdisk.obj:
s:__IMEOBJS__::
s:__DICTTBL__::
s:__DICTSRC__::
s:__MKDICTOPTION__::
s:__OBJLIST__:@$(ARGS):
s:__SOBJLIST__:@$(SARGS):
s:__DEFRC__:\\"$(DOSRC)\\":
s:__TBLPATH__::
s:__DATADIR__:$(BINDIR):g
s:	__RENAME__:#	ren:
s:	__AOUT2EXE__:#	aout2exe:
s:	__REMOVE__:#	del:
s:__ECHO__:command /c echo:
s:__COPY__:copy /y:
s:__RM__:del:
s:__LANGDIR__::
s:__INSTALL__:copy /y:
s:__INSTSTRIP__::
s:__LN__:copy /y:
s:__CC__:bcc:
s:__CCOPTIONS__:-O -N -d -w-par -w-rch -w-ccc -w-pia:
s:__HOSTCC__:$(CC):
s:__HOSTCCOPTIONS__:-O -N -d -w-par -w-rch -w-ccc -w-pia:
s:__FDSETSIZE__::
s:__MEM__:-ml:
s:__SHMEM__:-mm:
s:__BSHMEM__:-mm:
s:__OUT__:-o$@:
s:__LNK__:-e$@:
s:__TERMLIBS__::
s:__REGLIBS__::
s:__OTHERLIBS__::
s:__KCODEOPTION__:-s:
s:__MSBOPTION__::
s:__UNITBL__:$(UNITBL):
s:__PREFIXOPTION__::
s:[	 ]*$::
/^[	 ][	 ]*-*\$(RM)$/d
