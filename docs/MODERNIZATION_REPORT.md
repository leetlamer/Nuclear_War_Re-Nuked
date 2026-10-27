# Modernization report

## Result and scope

The separate NuclearWar-ReNuked project contains the completed game, organized as
C++17 source with PNG/WAV resources and a standard CMake build. GameReverse was
not moved, edited or used as a runtime resource directory. A complete inventory/
SHA256 comparison verified **133940 original files unchanged**, including the
pre-existing .DS_Store state. The original handoff was not updated because this
would have violated the stronger unchanged-source requirement.

This is a behavioral-preservation migration, not a claim that every undocumented
Amiga implementation detail is known. Existing user-defined revenge, native-width
rules and completed fixes are preserved rather than reinterpreted.

## Source inventory

The authoritative original launcher was `port/player/run.sh`. It compiled all
`port/engine/src`, `port/session/src`, `port/resolver/src`,
`port/prototype/original_text.cpp`, `port/player/model.cpp` and
`port/player/main.cpp`, with their headers and generated player/text tables.
`port/player/assets.py` supplied original static layout data. Its underlying
palettes, glyph metrics, sprite indices/masks, audio samples and exact text tables
were the asset/data baseline. Detailed original→new mappings are in
[source_inventory.json](source_inventory.json).

Original build/evidence wrappers, Ghidra projects, HUNK executables, disk images,
old UI experiments and historical capture trees are not source-release/runtime
inputs. The legacy prototype and missile-demo sources retained under tests/support
are test-only compatibility oracles, not alternative playable game implementations.
Original `port/tools` exports/checkpoint/evidence programs are development tools;
they are replaced here by conversion, validation and parity tools that do not
need Ghidra or an executable at build/runtime.

## Organization and refactoring

- Public native-rule interfaces remain in `include/nuclear` with their established
  integer widths and uncertain fields/address notes intact.
- `src/game` contains shared rules, turn management, AI entry, scoring, combat,
  events and the final revenge queue. The AI entry transaction was moved intact
  into `game/ai/ai_controller.cpp` and given descriptive local role names.
- `src/ui` contains the player controller, staged display, commands, console,
  speech, shutters, targeting, playback and endgame. The resolver boundary now
  has the established descriptive name `TurnResolverAdapter`.
- `src/rendering` owns the original-art renderer and named animation functions.
  Packed atlas images use readable asset keys/filenames; original source keys
  and mask mappings remain metadata for traceability.
- `src/audio` retains cue timing, gain, stream resampling, looping and mixing.
- `src/platform` resolves installed/build-tree resources, loads PNG/WAV and runs
  the application loop. Replay-only setup/capture/drivers were separated into
  `tests/support/*.inc` and compile only into `nuclearwar_replay`.
- The previous compressed source is consistently formatted. SDL window, renderer,
  textures, loaded WAV buffers and PNG lifetime use explicit RAII ownership.
  Speech-time emoji refresh, committed display quantities, source launch pads,
  city exhaust, shutters, input, scoring/restart and all original text remain.

No new gameplay abstraction, city model, inventory authority, AI probability,
combat algorithm or RNG was introduced. Some compatibility-sensitive rule and
frame coordinator functions remain large intentionally; further decomposition
should use the same parity gates rather than speculative rewrites.

## Modern assets and data

**13 PNG textures** preserve every original runtime RGBA pixel and source dimension,
including the bitmap font, opaque and masked atlas variants, and distinct ending
palettes. No upscaling, redrawing, filtering, alternate art or gamma transform was
introduced. Sprite source rectangles, logical320×200 display and nearest-neighbor
integer scaling are unchanged.

**13 WAV files** contain the11 actively used original payloads and the2 identical
UI-generated waveforms. An unreferenced siren, with no established active cue/rate,
was deliberately not shipped. WAV PCM8 bias is exactly reversible; playback uses
the unchanged signed8 resampler input, effective clock/period, cue lengths, gain
and repetition logic. Twenty decoded cue outputs were byte-identical to the
reference, including the1.08× production ooo cue with its existing5ms edge taper.
Typewriter/bleep WAV payloads were also byte-compared against the original C++
generators. Original samples were not modified or artistically processed.

Readable JSON preserves **743 exact strings /315 ordered references**, names,
profiles, palettes, coordinates, glyph metrics, default scores, asset metadata
and named animation mappings. Generated C++ tables are checked in; normal builds
need no Python. Configure-time fingerprints cover declared assets only (not OS .DS_Store metadata); regeneration validation prevents
stale/missing asset/data references. Native text IDs remain stable identifiers.
The animation C++ functions remain timing authorities; JSON does not introduce
loosely configured RNG/gameplay logic. Packed-atlas independent frame counts that
are not established are explicitly not fabricated.

All imported PNGs, WAVs and structured data were re-exported from source inputs
and compared for reproducibility. See [ASSET_MANIFEST.md](ASSET_MANIFEST.md).

## Incremental validation

Local Git checkpoints record the baseline-compatible source build, modern
PNG/WAV loading, source/ownership/replay separation, portable build and final
validated distribution. Each substantive stage compiled and ran its test gate;
`.migration` contains isolated, ignored full baseline/evidence material rather
than introducing those dependencies into the release tree.

The original tests were built/run in that isolated copy before asset/behavioral
migration. Baseline and modern results retain the same rule counts:

| Suite | Groups / checks or result |
|---|---|
| Engine + ending + markers + round-plan |205 /2874595|
| Session |24 /1797|
| Resolver |23 /407|
| Single missile |8 /657|
| Player/UI |173 /177996|
| Revenge |20 /790|
| Original text |1217 checks /138 independent format fixtures|
| Legacy compatibility prototype |7 /108475|
| Missile demo compatibility |9 /11194|
| SDL mask/mirror rendering |160703 checks|
| Emoji/speech actual renderer |397424 checks,104 clicks/bleeps|

The modern **16 CTest tests pass in Release, Debug/UBSan and a clean independently
extracted Debug source tree**. That clean tree has no .migration, original source,
raw audio, bitplanes or original executables. Assets/data validation is one of
those tests when Python is available; normal builds also check fingerprints
without Python.

Deterministic comparison matched **2173 phase records byte-for-byte**, covering
eight seeded games and actual human/AI elimination fixtures. Actor bytes include
population, weapons, defence, profiles/scores; IDs, capacities, pass/remaining
fields, phase and RNG state were compared. Existing revenge tests cover airborne-
first selection, missile fallback, four-attempt cap, compatible stock, rotation,
FIFO chains, normal combat/interception and no premature ending.

**39 reference/candidate SDL replay pairs** cover four complete rounds, missiles,
bombers, defence, production/100MT cue, selectors, forward/reverse/rapid shutters,
post-round/elimination re-entry, moods/speech, special events, revenge normal/
click/hold and two-session restart paths. Final-result frames match outside the
original real-wall-clock console viewport. Shared capture frames and transient
capture differences are enumerated in `validation/replay_results.json`, not
silently treated as full-frame equality. Ordered production text, recorded revenge
identity/target sequences, result-bearing RNG output, persisted score bytes and
restart traces were compared.

### Native capture limitation and resolution

The old automated native replay captures read pixels **after** SDL_RenderPresent.
A native restart comparison failed at that capture point. This was investigated,
not hidden: the reference probe itself changed buffer contents across presentation
in27 of29 frames. SDL invalidates that backbuffer; those images are not reliable
visual oracles.

Independent probes captured **before presentation** using each actual renderer.
All **29 paired native frames matched** outside the live console where applicable;
the high-score frame matched fully. They cover selection, UI, missile/bomber/
defence, city launch, alien/earthquake, high scores and reset-to-selection.
The native emoji renderer test also passed397424 checks /104 accepted clicks and
verified continued speech and12 subsequent rounds. This demonstrates actual SDL
pixel behavior rather than claiming unit tests alone establish visual parity.
Native speaker listening and a complete manual playthrough were not performed.

## Build, installation and platform status

Tested host: **macOS27 arm64, Apple Clang17, SDL2 2.32.10**. Release and Debug/UBSan
builds passed. The installed native executable started from a foreign working
directory without an asset override and remained running; the test terminated its
own process after startup. Resources resolve beside the executable or in the
installation share directory. SDL2/libpng remain explicit runtime dependencies;
Python and Amiga tools do not.

CMake and CI configurations cover Apple Silicon, Intel macOS, Linux x86_64 and
Windows x64. Other operating systems/architectures were **not executed locally**.
The host libpng bottle was built for macOS27 while the compiler's default target
was26, producing a linker deployment warning; do not claim that this host-built
binary supports macOS26 without matching dependency builds. Source builds are
not hardwired to that bottle or machine path.

Installation and source ZIP preparation are local only. No GitHub repository,
remote publish, signing/notarization or all-platform release certification is
claimed. Shared-library redistribution and platform packaging remain deployment
work beyond this source project's tested host.

## Preservation and remaining debt

- GameReverse content/inventory:133940 files unchanged. Its historical canonical
  `.DS_Store` evidence mismatch was neither altered nor called a passing check.
- No original files moved/deleted; no original handoff edits; no shutdown.
- No changed game rules, RNG ordering, production boundary, revenge policy,
  inventory wrapping, input decisions, population arithmetic, scoring or text.
- No source-tree/working-directory fallback for runtime assets, or hardcoded
  developer paths in runtime code/build definitions.
- Some native fields/quirks remain uncertain by design; this migration did not
  investigate or reinterpret them. Larger rule/frame coordinator functions are
  documented compatibility debt, not substituted algorithms.
- Cross-platform CI is prepared but unexecuted remotely. Native audio listening
  and broader manual acceptance remain limitations. The legacy post-present
  capture limitation remains in the reference tooling, not the playable game.

Machine-readable summaries and test logs are under `docs/validation`. Source maps,
architecture, build/contribution instructions and asset manifests are part of the
source distribution. The final local Git checkpoint and source archive provide
a reviewable, reproducible handoff without modifying the original project.
