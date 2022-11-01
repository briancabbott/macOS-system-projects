# Note that this is NOT a relocatable package
%define ver      1.2.10
%define  RELEASE 1
%define  rel     %{?CUSTOM_RELEASE} %{!?CUSTOM_RELEASE:%RELEASE}
%define prefix   /usr

Summary: Handy library of utility functions
Name: glib
Version: %ver
Release: %rel
Copyright: LGPL
Group: Libraries
Source: ftp://ftp.gimp.org/pub/gtk/v1.1/glib-%{ver}.tar.gz
BuildRoot: /var/tmp/glib-%{PACKAGE_VERSION}-root
URL: http://www.gtk.org
Docdir: %{prefix}/doc

%description
Handy library of utility functions.  Development libs and headers
are in glib-devel.

%package devel
Summary: GIMP Toolkit and GIMP Drawing Kit support library
Group: X11/Libraries

%description devel
Static libraries and header files for the support library for the GIMP's X
libraries, which are available as public libraries.  GLIB includes generally
useful data structures.


%changelog

* Thu Feb 11 1999 Michael Fulbright <drmike@redhat.com>
- added libgthread to file list

* Fri Feb 05 1999 Michael Fulbright <drmike@redhat.com>
- version 1.1.15

* Wed Feb 03 1999 Michael Fulbright <drmike@redhat.com>
- version 1.1.14

* Mon Jan 18 1999 Michael Fulbright <drmike@redhat.com>
- version 1.1.13

* Wed Jan 06 1999 Michael Fulbright <drmike@redhat.com>
- version 1.1.12

* Wed Dec 16 1998 Michael Fulbright <drmike@redhat.com>
- updated in preparation for the GNOME freeze

* Mon Apr 13 1998 Marc Ewing <marc@redhat.com>
- Split out glib package

%prep
%setup

%build
CFLAGS="$RPM_OPT_FLAGS" ./configure --prefix=%prefix
make

%install
rm -rf $RPM_BUILD_ROOT

make prefix=$RPM_BUILD_ROOT%{prefix} install

%clean
rm -rf $RPM_BUILD_ROOT

%post -p /sbin/ldconfig

%postun -p /sbin/ldconfig

%files
%defattr(-, root, root)

%doc AUTHORS COPYING ChangeLog NEWS README
%{prefix}/lib/libglib-1.2.so.*
%{prefix}/lib/libgthread-1.2.so.*
%{prefix}/lib/libgmodule-1.2.so.*

%files devel
%defattr(-, root, root)

%{prefix}/lib/lib*.so
%{prefix}/lib/*a
%{prefix}/lib/glib
%{prefix}/include/*
%{prefix}/man/man1/
%{prefix}/share/aclocal/*
%{prefix}/bin/*
