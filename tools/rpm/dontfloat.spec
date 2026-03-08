%define name dontfloat
%define version 0.0.0.1
%define release 1
%define buildroot %{_tmppath}/%{name}-%{version}-%{release}-root

Summary: Аудиоредактор для выравнивания BPM
Name: %{name}
Version: %{version}
Release: %{release}%{?dist}
License: GPL-3.0+
Group: Applications/Multimedia
Source0: %{name}-%{version}.tar.gz
URL: https://github.com/yourusername/DONTFLOAT
BuildRequires: cmake >= 3.16
BuildRequires: qt6-qtbase-devel
BuildRequires: qt6-qtmultimedia-devel
BuildRequires: qt6-qttools-devel
Requires: qt6-qtbase >= 6.8.0
Requires: qt6-qtmultimedia >= 6.8.0
BuildArch: x86_64

%description
DONTFLOAT - это аудиоредактор для анализа и выравнивания BPM аудиофайлов.

Основные возможности:
- Автоматический анализ BPM с использованием алгоритмов Mixxx
- Визуализация звуковой волны
- Выравнивание долей по тактовой сетке
- Метки сжатия/растяжения для временной коррекции
- Метроном с настраиваемым BPM
- Питч-сетка для анализа тональности

%prep
%setup -q

%build
mkdir -p build
cd build
cmake -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_INSTALL_PREFIX=%{_prefix} \
      ..
make %{?_smp_mflags}

%install
rm -rf %{buildroot}
cd build
make install DESTDIR=%{buildroot}

%files
%defattr(-,root,root,-)
%{_bindir}/DONTFLOAT
%{_datadir}/applications/dontfloat.desktop
%{_datadir}/icons/hicolor/*/apps/dontfloat.png
%{_datadir}/dontfloat/translations/*.qm
%doc README.md LICENSE

%changelog
* Sat Feb 14 2026 DONTFLOAT Project <maintainer@example.com> - 0.0.0.1-1
- Первый релиз DONTFLOAT
- Анализ и выравнивание BPM
- Визуализация волны и битов
- Метки сжатия/растяжения
- Метроном и питч-сетка
