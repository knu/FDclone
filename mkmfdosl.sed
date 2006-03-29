s:/:\\:g
s:__PREFIX__::
s:__CONFDIR__::
s:__EXE__:.exe:g
s:__OBJ__:.obj:g
s:__OBJS__:unixemu.obj unixdisk.obj:
s:__OBJLIST__:@$(ARGS):
s:__DEFRC__:\\\\\\"$(DOSRC)\\\\\\":
s:__UNITBLPATH__::
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
s:__CC__:lcc86:
s:__CCOPTIONS__:-O:
s:__HOSTCC__:$(CC):
s:__HOSTCCOPTIONS__:-O:
s:__FDSETSIZE__::
s:__MEM__:-ml -h -k"-s 3c00":
s:__SHMEM__:-mp -h -k"-s 8000":
s:__BSHMEM__:-ms -k"-s 8000":
s:__OUT__:-o $@:
s:__LNK__:-o $@:
s:__TERMLIBS__::
s:__REGLIBS__::
s:__OTHERLIBS__:-lintlib -ltinymain.obj:
s:__KCODEOPTION__:-s:
s:__MSBOPTION__::
s:__UNITBL__:$(UNITBL):
s:__PREFIXOPTION__::
