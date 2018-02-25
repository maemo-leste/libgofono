Name: libgofono
Version: 2.0.6
Release: 0
Summary: Ofono client library
Group: Development/Libraries
License: BSD
URL: https://git.merproject.org/mer-core/libgofono
Source: %{name}-%{version}.tar.bz2
Requires:   libglibutil >= 1.0.28
BuildRequires:  pkgconfig(glib-2.0)
BuildRequires:  pkgconfig(libglibutil) >= 1.0.28
Requires(post): /sbin/ldconfig
Requires(postun): /sbin/ldconfig

%description
Provides glib-based ofono client API

%package devel
Summary: Development library for %{name}
Requires: %{name} = %{version}
Requires: pkgconfig

%description devel
This package contains the development library for %{name}.

%prep
%setup -q

%build
make KEEP_SYMBOLS=1 release pkgconfig

%install
rm -rf %{buildroot}
make install-dev DESTDIR=%{buildroot}

%post -p /sbin/ldconfig

%postun -p /sbin/ldconfig

%files
%defattr(-,root,root,-)
%{_libdir}/%{name}.so.*

%files devel
%defattr(-,root,root,-)
%{_libdir}/pkgconfig/*.pc
%{_libdir}/%{name}.so
%{_includedir}/gofono/*.h
