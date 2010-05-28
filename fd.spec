%define lang ja
%define mansec 1

Summary: a file & directory maintenance tool.
Name: FDclone
Version: 3.00f
Release: 1
Group: System Environment/Shells
URL: http://hp.vector.co.jp/authors/VA012337/soft/fd/index.html
Source: FD-%{version}.tar.gz
BuildRoot: %{_tmppath}/%{name}-%{version}-root
Requires: ncurses
License: Freeware

%description
FDclone is a file & directory maintenance tool for the UNIX
based OS.  It is a clone of "FD" for MS-DOS, made by Mr.
A.Idei <SDI00544@niftyserve.or.jp> in 1989.

%description(ja)
FDclone は、1989年に A.Idei <SDI00544@niftyserve.or.jp> 氏の
手によって MS-DOS 用に作成された『ＦＤ』を模した、UNIX ベー
スの OS 汎用のファイル & ディレクトリ管理ツールです。

%prep
%setup -q -n FD-%{version}

%build
make PREFIX=%{_prefix} CONFDIR=%{_sysconfdir} LANG=%{lang}
sed -e '/# launcher definition/,/^$/ {' \
    -e  '/^$/ {' \
    -e   'i \' \
    -e   'launch ".rpm"	"rpm2cpio %C|cpio -tv"	"%a %l %u %g %s %m %d %{yt} %*f"' \
    -e  '}' \
    -e '}' \
    -e '/# archiver definition/,/^$/ {' \
    -e  '/^$/ {' \
    -e   'i \' \
    -e   'arch ".rpm"	"clear; echo ERROR; false"	"rpm2cpio %C|cpio -id %TA"' \
    -e  '}' \
    -e '}' \
_fdrc > fdrc

%install
rm -rf $RPM_BUILD_ROOT
mkdir -p $RPM_BUILD_ROOT/%{_bindir}
mkdir -p $RPM_BUILD_ROOT/%{_mandir}/man%{mansec}
mkdir -p $RPM_BUILD_ROOT/%{_mandir}/%{lang}/man%{mansec}
mkdir -p $RPM_BUILD_ROOT/%{_sysconfdir}
make BUILD=$RPM_BUILD_ROOT \
	BINTOP=%{_bindir} MANTOP=%{_mandir} LANG=%{lang} \
	install

%clean
rm -rf $RPM_BUILD_ROOT

%post
%files
%defattr(-,root,root)
%{_bindir}/fd
%{_bindir}/fdsh
%{_bindir}/fd-unicd.tbl
%{_bindir}/fd-dict.tbl
%{_bindir}/fd-cat.ja
%{_bindir}/fd-cat.C
%{_mandir}/man%{mansec}/fd.%{mansec}*
%{_mandir}/man%{mansec}/fdsh.%{mansec}*
%{_mandir}/%{lang}/man%{mansec}/fd.%{mansec}*
%{_mandir}/%{lang}/man%{mansec}/fdsh.%{mansec}*
%config %{_sysconfdir}/fd2rc

%doc README HISTORY FAQ TECHKNOW Install ToAdmin LICENSES
%doc README.eng HISTORY.eng FAQ.eng TECHKNOW.eng Install.eng ToAdmin.eng LICENSES.eng

%changelog
* Sat May 29 2010 Takashi SHIRAI <shirai@unixusers.net>
- for 3.00f

* Sat Feb 27 2010 Takashi SHIRAI <shirai@unixusers.net>
- for 3.00e

* Sun Nov 30 2008 Takashi SHIRAI <shirai@unixusers.net>
- for 3.00d

* Sun Jul 27 2008 Takashi SHIRAI <shirai@unixusers.net>
- for 3.00c

* Sun Jul  6 2008 Takashi SHIRAI <shirai@unixusers.net>
- for 3.00b

* Sat Jun 21 2008 Takashi SHIRAI <shirai@unixusers.net>
- for 3.00a

* Sat May 31 2008 Takashi SHIRAI <shirai@unixusers.net>
- for 3.00

* Thu Apr 26 2007 Takashi SHIRAI <shirai@unixusers.net>
- for 2.09d

* Fri Mar 16 2007 Takashi SHIRAI <shirai@unixusers.net>
- for 2.09c
- include fd-dict.tbl in files
- change the directory where manuals are installed from /usr/man to %{_mandir}

* Tue Jul  7 2004 Takashi SHIRAI <shirai@unixusers.net>
- for 2.05g
- support the filename in RPM which includes spaces and tabs

* Tue Sep 17 2003 Takashi SHIRAI <shirai@unixusers.net>
- for 2.04b
- extended for portability

* Tue Apr 15 2003 Takashi SHIRAI <shirai@unixusers.net>
- for 2.03a
