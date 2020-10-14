Name:       commhistory-daemon
Summary:    Communications event history database daemon
Version:    0.8
Release:    1
License:    LGPLv2
URL:        https://git.sailfishos.org/mer-core/commhistory-daemon
Source0:    %{name}-%{version}.tar.bz2
Source1:    %{name}.privileges
BuildRequires:  pkgconfig(Qt5Core)
BuildRequires:  pkgconfig(Qt5DBus)
BuildRequires:  pkgconfig(Qt5Contacts)
BuildRequires:  pkgconfig(Qt5Versit)
BuildRequires:  pkgconfig(Qt5Test)
BuildRequires:  pkgconfig(commhistory-qt5) >= 1.10.2
BuildRequires:  pkgconfig(contactcache-qt5)
BuildRequires:  pkgconfig(qtcontacts-sqlite-qt5-extensions)
BuildRequires:  pkgconfig(TelepathyQt5) >= 0.9.8
BuildRequires:  pkgconfig(mlite5)
BuildRequires:  pkgconfig(mlocale5)
BuildRequires:  pkgconfig(mce)
BuildRequires:  pkgconfig(ngf-qt5)
BuildRequires:  pkgconfig(qt5-boostable)
BuildRequires:  pkgconfig(nemonotifications-qt5) >= 1.2.2
BuildRequires:  qt5-qttools
BuildRequires:  qt5-qttools-linguist
BuildRequires:  libqofono-qt5-devel >= 0.89
BuildRequires:  libqofonoext-devel
BuildRequires:  python3-base
BuildRequires:  systemd
Requires:  libcommhistory-qt5 >= 1.9.33
Requires:  libqofono-qt5 >= 0.66
Requires:  mapplauncherd-qt5
Requires:  nemo-qml-plugin-notifications-qt5 >= 1.2.2

Obsoletes: smshistory <= 0.1.8
Provides: smshistory > 0.1.8
Obsoletes: voicecallhistory <= 0.1.5
Provides: voicecallhistory > 0.1.5

%package tests
Summary: Unit tests for %{name}

%description tests
Unit tests for %{name}

%package ts-devel
Summary: Translation source for %{name}

%description ts-devel
Translation source for %{name}

%description
Daemon for logging communications (IM, SMS and call) in history database.

%prep
%setup -q -n %{name}-%{version}

%build
unset LD_AS_NEEDED
%qmake5
make %{?_smp_mflags}

%install
rm -rf %{buildroot}
%qmake5_install

mkdir -p %{buildroot}%{_userunitdir}/user-session.target.wants
ln -s ../commhistoryd.service %{buildroot}%{_userunitdir}/user-session.target.wants/

mkdir -p %{buildroot}%{_datadir}/mapplauncherd/privileges.d
install -m 644 -p %{SOURCE1} %{buildroot}%{_datadir}/mapplauncherd/privileges.d

%files
%defattr(-,root,root,-)
%license LICENSE
%{_bindir}/commhistoryd
%{_userunitdir}/commhistoryd.service
%{_userunitdir}/user-session.target.wants/commhistoryd.service
%{_datadir}/translations/*.qm
%{_datadir}/lipstick/notificationcategories/*
%{_datadir}/telepathy/clients/CommHistory.client
%{_sysconfdir}/dbus-1/system.d/*.conf
%{_datadir}/mapplauncherd/privileges.d/*

%files tests
%defattr(-,root,root,-)
/opt/tests/%{name}

%files ts-devel
%defattr(-,root,root,-)
%{_datadir}/translations/source/*.ts

