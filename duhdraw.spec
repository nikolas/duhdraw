
%define version	2.8.12
%define release	1
%define name	duhdraw
%define dirTarget $RPM_BUILD_ROOT%{_prefix}/bin

Summary: linux-based ANSI editor
Name: %{name}
Version: %{version}
Release: %{release}
Copyright: GNU Copyleft
Group: Applications/Editors
Source: http://www.wwco.com/~wls/opensource/duhdraw-%{version}.tar.gz
URL: http://www.wwco.com/~wls/opensource/duhdraw.php
BuildRoot: /var/tmp/%{name}-%{version}
Packager: marco antonio cabazal <nightshiphter@yahoo.com>

%description
DuhDraw, written by Ben Fowler and maintained by Walt Stoneburner, is 
a Linux based Open Source clone of The Draw, a DOS based ASCII art editor.


%prep
%setup

%build
make all

%install
rm -rf $RPM_BUILD_ROOT
install -d %{dirTarget}
install -m 555 ansi %{dirTarget}/ansi
install -m 555 ansitoc %{dirTarget}/ansitoc
install -m 555 duhdraw %{dirTarget}/duhdraw
  
%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root)
%doc CREDITS HISTORY READ.ME Whatdone
%{_prefix}/bin/*
