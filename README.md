# Nuclear War: Re-Nuked
<img width="1983" height="793" alt="Nuclear War_ Re-Nuked Beach Apocalypse" src="https://github.com/user-attachments/assets/497d0ad0-03fc-4240-8017-0d79850ee13d" />

A C++17/SDL2 recreation of the Amiga Nuclear War, organized as a standalone source
project. This is a source modernization of the completed GameReverse game, not a
new gameplay implementation. The original pixel art, text, sound payloads, native
integer rules and completed presentation corrections are retained.

## Build and play

Install a C++17 compiler, CMake3.20+, SDL2 and libpng, then:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel 4
./build/nuclearwar
```

Windows multi-configuration builds place `nuclearwar.exe` in `build/Release`.
Assets are copied beside the executable. You can launch from any working directory.
Python, Amiga executables, disk images and Ghidra are **not runtime dependencies**.

See [BUILDING.md](BUILDING.md) for macOS Apple Silicon/Intel, Windows x64 and Linux
instructions, Debug/UBSan builds, testing and installation.

## Find the code

| Area | Location |
|---|---|
| Exact word arithmetic, actors, RNG and shared rules | `include/nuclear`, `src/game/game_rules.cpp` |
| Turn coordinator and resolution | `src/game/turn_manager.cpp`, `src/game/combat` |
| Four-attack, airborne-first revenge | `src/game/revenge/revenge_system.hpp` |
| Production and AI decisions | `src/game/game_rules.cpp`, `src/game/ai/ai_controller.cpp` |
| Human commands and committed display state | `src/ui/player_controller.*` |
| Console, speech, selection and playback | `src/ui` |
| Original-art rendering and animation definitions | `src/rendering` |
| Original sound scheduling and mixing | `src/audio/audio_manager.hpp` |
| Portable PNG/WAV loading and launch options | `src/platform` |
| Exact text, layout and asset metadata | `assets/data` |
| Deterministic and rendering regressions | `tests` |

[ARCHITECTURE.md](ARCHITECTURE.md) explains the turn flow and compatibility
boundaries. [CONTRIBUTING.md](CONTRIBUTING.md) explains safe changes.

## Validation status

The modernization passed deterministic state, decoded PCM and SDL rendering
comparisons against the original source baseline. Native Apple Silicon rendering
was checked. Windows, Linux and Intel macOS have build configurations/CI jobs but
were not executed on this machine. Audio samples were compared as decoded PCM;
speaker listening is not claimed.

Full evidence, limitations and source mapping:
[docs/MODERNIZATION_REPORT.md](docs/MODERNIZATION_REPORT.md).
[docs/ASSET_MANIFEST.md](docs/ASSET_MANIFEST.md) lists every packaged image and WAV.
Original content has not been assigned a new license by this migration; asset
provenance is retained in the manifests. Nothing has been published remotely.
