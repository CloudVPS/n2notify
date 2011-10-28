%define version 1.0.6

Name: n2notify
Summary: n2notify
Group: Development
Version: %version
Source0: n2notify-%{version}.tar.gz
Source1: grace-1.0.3.tar.gz
Release: 1
License: GPLv3
BuildRoot: /var/tmp/%{name}-buildroot
Requires: n2rxd
BuildRequires: zlib-devel
%description
This is the n2 notification framework+daemon.

%prep
%setup -q -a 1

%build
BUILD_ROOT=$RPM_BUILD_ROOT
echo 1.0.3 > grace-1.0.3/src/libgrace/version.id
pushd grace-1.0.3/src/libgrace && ../../util/mkversion version.cpp && popd
pushd grace-1.0.3/src/libgrace && ./configure && make && ar cr ../../lib/libgrace.a application.o atoll.o cgi.o checksum.o cmdtoken.o commonkeys.o configdb.o currency.o daemon.o defaults.o dictionary.o eventq.o file.o filesystem.o fswatch.o http.o httpd.o httpd_fileshare.o lock.o md5.o netdb.o process.o reg.o regexpression.o retain.o ringbuffer.o session.o smtp.o smtpd.o socketpool.o statstring.o stringdict.o str.o strutil.o system.o terminal.o tcpsocket.o thread.o timeofday.o timestamp.o tolower.o udpsocket.o valuable.o value.o value_ascii.o value_ini.o value_ip.o value_grace.o value_json.o value_php.o value_plist.o value_sort.o value_strformat.o value_xml.o value_csv.o value_cxml.o value_shox.o version.o xmlschema_root.o xmlschema_base.o xmlschema_misc.o xmlschema.o validator.o valueindex.o && popd
GRACEINC=grace-1.0.3/include LIBGRACE=grace-1.0.3/lib/libgrace.a ./configure
make n2notifyd n2event

%install
BUILD_ROOT=$RPM_BUILD_ROOT
rm -rf ${BUILD_ROOT}
mkdir -p ${BUILD_ROOT}/usr/sbin
mkdir -p ${BUILD_ROOT}/usr/bin
mkdir -p ${BUILD_ROOT}/etc/n2
mkdir -p ${BUILD_ROOT}/etc/init.d
install -b -m 0755 rpm/n2notifyd.init ${BUILD_ROOT}/etc/init.d/n2notifyd
install -d -m 0750 ${BUILD_ROOT}/etc/n2
install -b -m 0644 n2notifyd.conf ${BUILD_ROOT}/etc/n2/n2notifyd.example.conf
install -m 0755 n2notifyd ${BUILD_ROOT}/usr/sbin/
install -m 0755 n2event ${BUILD_ROOT}/usr/bin/

%post
install -d -o root -g n2 -m 0750 /etc/n2
install -d -o n2 -g n2 -m 0750 /var/log/n2
chkconfig --level 2345 n2notifyd on

%preun
if [ "$1" = 0 ] ; then
	service n2notifyd stop > /dev/null 2>&1
	chkconfig --del n2notifyd
fi
exit 0

%postun
if [ "$1" -ge 1 ]; then
	service n2notifyd condrestart > /dev/null 2>&1
fi
exit 0 

%files
%defattr(-,root,root)
/etc/init.d/n2notifyd
/etc/n2
/usr/sbin/n2notifyd
/usr/bin/n2event
