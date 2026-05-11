Name:           justcolor
Version:        1.0
Release:        1%{?dist}
Summary:        justcolor system tray application
License:        GPLv3

Source0:        justcolor
Source1:        justcolor.png
Source2:        justcolor.desktop

%description
Justcolor is a color picker app

%install
rm -rf %{buildroot}
mkdir -p %{buildroot}/usr/bin/
mkdir -p %{buildroot}/usr/share/icons/
mkdir -p %{buildroot}/usr/share/applications/
install -m 755 %{SOURCE0} %{buildroot}/usr/bin/
install -m 644 %{SOURCE1} %{buildroot}/usr/share/icons/
install -m 644 %{SOURCE2} %{buildroot}/usr/share/applications/

%files
/usr/bin/justcolor
/usr/share/icons/justcolor.png
/usr/share/applications/justcolor.desktop

%changelog
* Mon Jun 02 2025 Developer <you@example.com> - 1.0-1
- Initial RPM release
