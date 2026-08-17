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
BuildRequires: gcc-c++
BuildRequires: make
BuildRequires: qt6-qtbase-devel
BuildRequires: qt6-qtmultimedia-devel
BuildRequires: qt6-qttools-devel
Requires: qt6-qtbase >= 6.8.0
Requires: qt6-qtmultimedia >= 6.8.0
# Иконки интерфейса — SVG: без плагина qsvg QIcon их не отрисует
Requires: qt6-qtsvg >= 6.8.0
Requires: hicolor-icon-theme
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
# LIBDIR задаём явно (на x86_64 это lib64) — секция files ждёт плагины там же.
# mini-DAW и plugin_tester — инструменты разработчика, в пакет не идут;
# VST3 требует проприетарный Steinberg SDK, поэтому OFF.
cmake -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_INSTALL_PREFIX=%{_prefix} \
      -DCMAKE_INSTALL_LIBDIR=%{_lib} \
      -DDONTFLOAT_BUILD_PLUGINS=ON \
      -DDONTFLOAT_BUILD_CLAP=ON \
      -DDONTFLOAT_BUILD_LV2=ON \
      -DDONTFLOAT_BUILD_VST3=OFF \
      -DDONTFLOAT_BUILD_MINI_DAW=OFF \
      -DDONTFLOAT_BUILD_PLUGIN_TESTER=OFF \
      ..
make %{?_smp_mflags}

%install
rm -rf %{buildroot}
cd build
make install DESTDIR=%{buildroot}

# Секция ниже должна покрывать ВСЁ, что положил install, иначе rpmbuild падает
# с «Installed (but unpackaged) files found».
%files
%defattr(-,root,root,-)
%{_bindir}/DONTFLOAT
%{_datadir}/applications/dontfloat.desktop
%{_datadir}/icons/hicolor/scalable/apps/dontfloat.svg
# Каталог переводов — по имени проекта (PROJECT_NAME), т.е. в верхнем регистре
%{_datadir}/DONTFLOAT/
%{_datadir}/doc/DONTFLOAT/
# Плагины для DAW (CLAP + LV2-бандлы)
%{_libdir}/clap/*.clap
%{_libdir}/lv2/dontfloat*.lv2/
%doc README.md LICENSE

%changelog
* Sat Feb 14 2026 DONTFLOAT Project <maintainer@example.com> - 0.0.0.1-1
- Первый релиз DONTFLOAT
- Анализ и выравнивание BPM
- Визуализация волны и битов
- Метки сжатия/растяжения
- Метроном и питч-сетка
