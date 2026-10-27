# Application launch icons

The user-supplied final artwork is preserved unchanged at
`assets/branding/nuclear-war-renuked-icon.png` (1254 × 1254 RGBA).
SHA-256: `3ca062ccaa824e4702f8942f6effac9aa1d931ced6bf902b98f111c8e6a5bb15`.
No replacement artwork was generated. The master matches the supplied Downloads PNG.

## Reproduction

Install Pillow in a development virtual environment, then run:

```sh
python3 tools/generate_icons.py
python3 tools/verify_icons.py
```

Pillow 13.0.0 was used for this export. Generated files are committed; normal builds
need neither Python nor Pillow. Images are downsampled with LANCZOS from the master,
with centered transparent square padding if necessary. Nothing is cropped or
stretched. Small representations retain the complete artwork; lettering naturally
becomes less legible at 16 pixels.

## Generated files

All paths are relative to `assets/branding/`:

- `nuclear-war-renuked.ico`: 16, 24, 32, 48, 64, 128, 256 pixel images.
- `nuclear-war-renuked.icns`: 16, 32, 64, 128, 256, 512, 1024 physical pixel
  representations, including Retina slots for 16, 32, 128 and 256 logical pixels.
- `linux/16x16/nuclear-war-renuked.png`
- `linux/32x32/nuclear-war-renuked.png`
- `linux/48x48/nuclear-war-renuked.png`
- `linux/64x64/nuclear-war-renuked.png`
- `linux/128x128/nuclear-war-renuked.png`
- `linux/256x256/nuclear-war-renuked.png`
- `linux/512x512/nuclear-war-renuked.png`
- `manifest.json`: master checksum, export policy and image list.

## Platform integration

`cmake/ApplicationIcons.cmake` controls platform resources:

- Windows: enable RC and compile `packaging/windows/application.rc` into the
  playable executable. Resource 1 embeds the multi-resolution ICO. SDL additionally
  sets the native window icon, covering window/taskbar usage. Windows shell caches
  may require refreshing after replacing an existing executable.
- macOS: `nuclearwar.app` receives the ICNS in `Contents/Resources`, the icon filename
  in Info.plist, and both bundle names set to `Nuclear War: Re-Nuked`. The installed
  bundle also includes game assets under Resources. Launch with
  `open build/nuclearwar.app`; do not launch a leftover pre-bundle binary.
- Linux: install the desktop entry under `share/applications` and all seven sizes
  under `share/icons/hicolor/<size>/apps`. The desktop entry uses the extensionless
  name `nuclear-war-renuked`, `Exec=nuclearwar`, and the requested display name.
  Desktop environments can refresh their icon caches after installation.
- SDL: every Renderer constructor loads the 256px PNG through executable-relative
  asset discovery and calls SDL_SetWindowIcon once. Surface/pixel storage is freed
  after SDL copies it. Window title now uses the official product name.

CMake/CPack include branding resources. On Linux, after a Release build, use
`sh tools/package_appimage.sh build` with CMake, linuxdeploy and its AppImage plugin
on PATH. This stages the normal install into AppDir, bundles dependencies using
linuxdeploy, and supplies the installed desktop file and 256px PNG to the AppImage
exporter. No absolute developer paths are used. AppImage generation requires Linux
and has not been executed on this Mac.

## Validation and limitations

- Master checksum and seven ICO image sizes verified.
- ICNS decoded and seven physical sizes verified; Apple iconutil successfully exported its iconset.
- All seven Linux PNG dimensions and desktop icon/name mapping verified.
- Native built app Info.plist parsed and checked; bundled ICNS byte-matches export;
  bundled SDL icon exists. `plutil -lint` passed.
- Release and UBSan CTest: 16/16 each passed, including renderer and emoji tests
  that execute SDL window-icon loading. No gameplay/audio/animation changes.
- CPack release ZIP checked for the app icon and Info.plist.
- Native macOS app started from `/tmp` without asset overrides and remained running
  until its own test process was terminated.
- Finder, Dock and application-switcher icons were not visually inspected. Windows
  PE resource embedding/Explorer/taskbar and Linux desktop/AppImage execution are
  configured but require native platform verification; no cross-toolchains installed.
- Existing host libpng deployment-target warning remains unrelated to branding.

The original GameReverse workspace was read only. No publishing or shutdown.
