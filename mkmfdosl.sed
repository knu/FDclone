s:/:\\:g
s:__PREFIX__::
s:__CONFDIR__::
s:__EXE__:.exe:g
s:__OBJ__:.obj:g
s:__DOSOBJS__:$(DOSOBJS):
s:__IMEOBJS__::
s:__DICTTBL__::
s:__DICTSRC__::
s:__MKDICTOPTION__::
s:__CATTBL__:$(CATTBL) $(ECATTBL):
s:__SOCKETOBJS__::
s:__SOCKETLIBS__::
s:__OBJLIST__:@$(ARGS):
s:__SOBJLIST__:@$(SARGS):
s:__NOBJLIST__:@$(NARGS):
s:__DEFRC__:\\\\\\"$(DOSRC)\\\\\\":
s:__TBLPATH__::
s:__DATADIR__:$(BINDIR):g
s:__DATADIR2__:$(BINDIR):g
s:__SLEEP__:#:
s:__DJGPP1__:#:
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
s:__MEM__:-ml -h -k"-s 3000":
s:__SHMEM__:-mp -h -k"-s 8000":
s:__BSHMEM__:-ms -k"-s 8000":
s:__NSHMEM__:-mp -h -k"-s 8000":
s:__OUT__:-o $@:
s:__LNK__:-o $@:
s:__TERMLIBS__::
s:__REGLIBS__::
s:__OTHERLIBS__:-lintlib -ltinymain.obj:
s:__KCODEOPTION__:-s:
s:__MSBOPTION__::
s:__UNITBL__:$(UNITBL):
s:__PREFIXOPTION__::
s:[	 ]*$::
/^[	 ][	 ]*-*\$(RM)$/d
