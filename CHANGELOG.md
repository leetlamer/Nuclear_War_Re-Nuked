# Changelog

## Unreleased — source modernization

- Created an independent C++17 project with explicit CMake/SDL2/libpng dependencies.
- Reorganized and formatted the authoritative engine, coordinator, resolver and UI.
- Retained all completed gameplay/presentation fixes, including airborne-first
  four-attack revenge, committed HUD quantities, immediate speech-time emoji
  refresh, launch-pad origins and corrected100MT audio scheduling.
- Converted original atlases/font to13 pixel-exact PNG textures and original/UI
  sounds to13 reversible PCM WAVs.
- Moved exact text/layout/static asset data into readable JSON with generated
  C++ tables and configure-time consistency checks.
- Separated replay-only fixtures from the playable entry point, added RAII resource
  ownership, executable-relative resources, installation and cross-platform CI.
- Preserved deterministic state and decoded PCM parity; recorded native rendering
  comparisons and limitations rather than claiming untested platform readiness.
