Summary: Basic system setup for NIDAS (NCAR In-Situ Data Acquistion Software)
Name: nidas
Version: 1.0
Release: 4
License: GPL
Group: Applications/Engineering
Url: http://www.eol.ucar.edu/
Packager: Gordon Maclean <maclean@ucar.edu>
# becomes RPM_BUILD_ROOT
BuildRoot:  %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)
Vendor: UCAR
BuildArch: noarch
Source: %{name}-%{version}.tar.gz

Requires: xmlrpc++ xerces-c libpcap

# Source: %{name}-%{version}.tar.gz

%description
ld.so.conf setup for NIDAS

%package x86-build
Summary: Package for building nidas on x86 systems with scons.
Requires: nidas gcc-c++ scons xerces-c-devel bluez-libs-devel bzip2-devel flex
Group: Applications/Engineering
%description x86-build
Package for building nidas on x86 systems with scons.

%package daq
Summary: Package for doing data acquisition with NIDAS.
Requires: nidas bluez-utils
Group: Applications/Engineering
%description daq
Package for doing data acquisition with NIDAS.
Contains some udev rules to expand permissions on /dev/tty[A-Z]* and /dev/usbtwod*

%prep
%setup -n nidas

%build

%install
rm -rf $RPM_BUILD_ROOT
install -d $RPM_BUILD_ROOT%{_sysconfdir}/ld.so.conf.d
echo "/opt/local/nidas/x86/lib" > $RPM_BUILD_ROOT%{_sysconfdir}/ld.so.conf.d/nidas.conf

install -m 0755 -d $RPM_BUILD_ROOT%{_sysconfdir}/udev/rules.d
cp etc/udev/rules.d/* $RPM_BUILD_ROOT%{_sysconfdir}/udev/rules.d

%post
/sbin/ldconfig

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root)
%config %{_sysconfdir}/ld.so.conf.d/nidas.conf

%files x86-build

%files daq
%config %attr(0644,root,root) %{_sysconfdir}/udev/rules.d/99-nidas.rules

%changelog
* Thu Jun 10 2010 Gordon Maclean <maclean@ucar.edu> 1.0-4
- updated etc/udev/rules.d/99-nidas.rules based on complaint from udevd:
-   NAME="%k" is superfluous and breaks kernel supplied names, please remove it from /etc/udev/rules.d/99-nidas.rules:9
* Wed Jun  9 2010 Gordon Maclean <maclean@ucar.edu> 1.0-3
- added flex to Requires of nidas-x86-build
* Wed Mar  3 2010 Gordon Maclean <maclean@ucar.edu> 1.0-2
- added udev rule for rfcomm BlueTooth devices
- added bzip2-devel, bluez-libs-devel to Requires list for nidas-x86-build
* Tue May 12 2009 Gordon Maclean <maclean@ucar.edu> 1.0-1
- initial version
