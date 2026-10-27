#!/bin/sh
# Requires a Linux Release build, linuxdeploy and its AppImage plugin on PATH.
set -eu
build_dir=${1:-build}
app_dir="$build_dir/AppDir"
DESTDIR="$(cd "$build_dir" && pwd)/AppDir" cmake --install "$build_dir" --prefix /usr --strip --config Release
linuxdeploy --appdir "$app_dir" --desktop-file "$app_dir/usr/share/applications/nuclear-war-renuked.desktop" --icon-file "$app_dir/usr/share/icons/hicolor/256x256/apps/nuclear-war-renuked.png" --output appimage
