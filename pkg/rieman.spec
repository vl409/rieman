# rpm specfile for: SuSe, Fedora

%define name        rieman
%define release     0
%define version     4

BuildRoot:      %{_tmppath}/%{name}-%{version}-build
BuildRequires:  tar make gcc
Summary:        Rieman the Pager
License:        GPL-3
Name:           %{name}
Version:        %{version}
Release:        %{release}
Source:         %{name}-%{version}.tar.gz
Prefix:         /usr/local
Group:          x11/misc
URL:            http://github.com

%description
TBD.

%build
tar xvf ../SOURCES/%{name}-%{version}.tar.gz
cd %{name}-%{version}
make PREFIX=$RPM_BUILD_ROOT/usr/local release

%install
cd %{name}-%{version}
make install

%files
/usr/local/bin/rieman
/usr/local/share/rieman

%clean
rm -rf "$RPM_BUILD_ROOT"
