%global php_apiver %((echo 0; php -i 2>/dev/null | sed -n 's/^PHP API => //p') | tail -1)
%global php_extdir %(php-config --extension-dir 2>/dev/null || echo "undefined")
%global php_version %(php-config --version 2>/dev/null || echo 0)

Name: php-bee
Version: 0.1.0.0
Release: 1%{?dist}
Summary: PECL PHP driver for Bee/Db
Group: Development/Languages
License: BSD 2-Clause
URL: https://github.com/bee/bee-php/
Source0: bee-php-%{version}.tar.gz
BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)
BuildRequires: php-devel
Requires: php(zend-abi) = %{php_zend_api}
Requires: php(api) = %{php_apiver}

%global ini_name 50-bee.ini

%description
PECL PHP driver for Bee/Db
Bee is an in-memory database and Lua application server.
This package provides PECL PHP driver for Bee/Db.

%prep
%setup -q -n bee-php-%{version}

cat > %{ini_name} << 'EOF'
; Enable bee extension module
extension=bee.so

; ----- Configuration options
; https://github.com/bee/bee-php/README.md

EOF

%build
%{_bindir}/phpize
%configure
make %{?_smp_mflags}

%install
rm -rf $RPM_BUILD_ROOT
make install INSTALL_ROOT=$RPM_BUILD_ROOT
# Drop in the bit of configuration
install -D -m 644 %{ini_name} %{buildroot}%{php_inidir}/%{ini_name}

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root,-)
%{php_extdir}/bee.so
%config(noreplace) %{php_inidir}/%{ini_name}
