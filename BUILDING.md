# Building and running

## Requirements

- C++17 compiler: Apple Clang, GCC/Clang, or Visual Studio2022 C++ tools.
- CMake3.20 or later and a platform build tool.
- SDL2 2.0.22+ development package and libpng1.6+ development package.
- Python3 only for optional asset/data regeneration, validation and parity tools.

No Python interpreter or original Amiga file is needed by the built game.

### macOS (Apple Silicon or Intel)

```sh
brew install cmake sdl2 libpng
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel 4
./build/nuclearwar
```

Use dependencies built for the same architecture as your compiler. If dependency
packages are outside CMake's search prefixes, pass `-DCMAKE_PREFIX_PATH=...`.
Do not link arm64 dependencies into an x86_64 build or vice versa.

### Linux x86_64

```sh
sudo apt-get install build-essential cmake libsdl2-dev libpng-dev
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel 4
./build/nuclearwar
```

A functioning SDL video/audio backend is needed for interactive play.

### Windows x64

Install Visual Studio C++ tools, CMake and vcpkg. From PowerShell, with your vcpkg
root in `VCPKG_ROOT`:

```powershell
vcpkg install sdl2:x64-windows libpng:x64-windows
cmake -S . -B build -A x64 -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"
cmake --build build --config Release --parallel 4
.\build\Release\nuclearwar.exe
```

Keep SDL2/libpng runtime DLLs discoverable beside the executable or on PATH;
vcpkg's app-local deployment normally handles build-tree DLLs. The project uses
`SDL_MAIN_HANDLED` and explicitly prepares SDL, so the application keeps an
ordinary portable `main` function.

## Debug and undefined-behavior checks

```sh
cmake -S . -B build-debug -DCMAKE_BUILD_TYPE=Debug
cmake --build build-debug --config Debug --parallel 4
ctest --test-dir build-debug -C Debug --output-on-failure

cmake -S . -B build-ubsan -DCMAKE_BUILD_TYPE=Debug -DNWR_ENABLE_UBSAN=ON
cmake --build build-ubsan --parallel 4
ctest --test-dir build-ubsan --output-on-failure
```

UBSan is enabled for Clang/GCC. MSVC ignores this option; use its supported
sanitizer tools separately. Tests use dummy SDL video where appropriate; this
verifies rendering/input logic, not audible output or monitor behavior.

## Install and package

```sh
cmake --install build --config Release --prefix ./dist
./dist/bin/nuclearwar
cd build
cpack --config CPackConfig.cmake
cpack --config CPackSourceConfig.cmake
```

The installed executable searches `../share/nuclearwar/assets`. Build-tree
executables search adjacent `assets`. macOS app-style `../Resources/assets` is
also supported. `NWR_ASSETS` may explicitly select a validated asset directory.
Runtime shared libraries remain platform dependencies; these ZIP recipes are not
claimed to create fully self-contained, signed/notarized binaries.

High scores retain the existing SDL preference location (`NuclearWar/NuclearWar`)
and original170-byte storage format. New-session reset preserves that storage.

## Data and asset tools

```sh
python3 tools/validate_assets.py
python3 tools/generate_data.py
```

Normal builds use committed generated headers and verify their fingerprints.
Regenerate after intentional JSON changes. Do not hand-edit generated headers.
Original-source conversion is optional and explicit:

```sh
python3 tools/convert_assets.py /path/to/GameReverse --output .
python3 tools/generate_data.py
```

The converter reads that source and writes only its output project. It does not
launch or analyze the original executable. PNGs preserve original RGBA pixels;
WAVs reversibly re-bias signed8 source samples into standard RIFF PCM8.

The CI matrix covers Linux, Windows and both macOS architectures. Runner labels
were checked against [GitHub's runner reference](https://docs.github.com/en/actions/reference/runners/github-hosted-runners).
These jobs are configured but have not been published or run remotely.

Application icon exports, macOS bundle launching and Linux AppImage staging are
documented in [APPLICATION_ICONS.md](docs/APPLICATION_ICONS.md).
