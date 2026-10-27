# Runtime asset manifest

All files below are self-contained PNG or RIFF PCM WAV. Runtime resolution is
executable-relative. Original indices, masks, bitplanes and .s8 payloads are not
runtime dependencies. Provenance and decoded-pixel/sample hashes are in
`assets/data/manifest.json`; frame schedules are in `assets/data/animations.json`
and their referenced C++ animation functions.

## Images

| File | Dimensions | Alpha | Source mapping |
|---|---|---|---|
| `images/backgrounds/character_selection.png` | 320×200 | opaque | start: indexed pixels + original palette |
| `images/interface/main_window.png` | 320×200 | opaque | dip: indexed pixels + original palette |
| `images/characters/leader_portraits.png` | 320×160 | opaque | leaders: indexed pixels + original palette |
| `images/backgrounds/world_map.png` | 320×200 | opaque | world_map: indexed pixels + original palette |
| `images/interface/control_tiles.png` | 320×272 | opaque | smalltiles: indexed pixels + original palette |
| `images/effects/small_sprites.png` | 320×272 | original 1-bit mask | smalltiles: indexed pixels + original palette + mask_smallmask_z_m |
| `images/missiles/flight_and_effects.png` | 320×288 | original 1-bit mask | big512: indexed pixels + original palette + mask_big512mask |
| `images/effects/large_sprites.png` | 320×432 | original 1-bit mask | bigtiles: indexed pixels + original palette + mask_bigmask |
| `images/interface/selection_backdrops.png` | 320×432 | opaque | bigtiles: indexed pixels + original palette |
| `images/characters/victory_body.png` | 320×112 | original 1-bit mask | endchar: indexed pixels + original palette + mask_endchar_mask |
| `images/backgrounds/space_ending.png` | 320×200 | opaque | spacemap: indexed pixels + original palette |
| `images/backgrounds/desolate_ending.png` | 320×200 | opaque | desolate_map: indexed pixels + original palette |
| `fonts/original_bitmap.png` | 576×8 | glyph bits | ld_8_strike.raw |

Packed atlases intentionally remain packed: city tiles, bomber frames,
exhaust, selection cursors and special events share original sprite sheets.
They have variable/overlapping sprite sizes, so a fabricated total independent
sprite count is not assigned. Verified used frame counts/sequences and rectangles
are named in animations.json; original glyph metrics describe97 font glyphs.
Leader art contains ten identities with five32×32 expression frames each.
Backgrounds/window art are static single textures. All PNGs retain exact
source dimensions, palette RGB and original1-bit mask alpha; no rescaling,
smoothing, alternate art or gamma correction was added.

## Audio

| File | Canonical header rate | Channels / PCM | Full source samples | Runtime key |
|---|---|---|---|---|
| `audio/effects/falling_weight.wav` | 4961 Hz | mono / unsigned8 | 5406 | `16ton` |
| `audio/effects/factory.wav` | 27284 Hz | mono / unsigned8 | 20032 | `build` |
| `audio/effects/shutter.wav` | 8547 Hz | mono / unsigned8 | 8820 | `door.snd` |
| `audio/effects/bomber_engine.wav` | 5911 Hz | mono / unsigned8 | 7818 | `engine` |
| `audio/effects/explosion.wav` | 17734 Hz | mono / unsigned8 | 4998 | `exp` |
| `audio/effects/defector_steps.wav` | 5067 Hz | mono / unsigned8 | 5060 | `feet` |
| `audio/effects/ai_victory.wav` | 9586 Hz | mono / unsigned8 | 5914 | `iwon` |
| `audio/effects/missile_launch.wav` | 4961 Hz | mono / unsigned8 | 6468 | `missile` |
| `audio/effects/cow.wav` | 4961 Hz | mono / unsigned8 | 11862 | `moo` |
| `audio/effects/warhead_exclamation.wav` | 5358 Hz | mono / unsigned8 | 12904 | `ooo` |
| `audio/effects/propaganda.wav` | 3550 Hz | mono / unsigned8 | 5400 | `prop` |
| `audio/effects/selection_bleep.wav` | 5067 Hz | mono / unsigned8 | 506 | `selection_bleep` |
| `audio/effects/typewriter.wav` | 27928 Hz | mono / unsigned8 | 128 | `ticker` |

The WAV stores the complete original sample or identical generated UI waveform.
Runtime cue lengths and the original3546895/period rate policy still select the
same playable slice and effective rate. A shared sound can have multiple cue
rates; the WAV header is its canonical use, not a global override. Conversion
only re-biases signed8 to standard WAV unsigned8, then the loader reverses that
bias before the unchanged SDL resampler. The100MT cue keeps1.08×, the5ms playback
edge taper and120ms production buffer. Twenty decoded cue vectors were compared
byte-for-byte with the reference. No filter/normalization was applied.

The source had an additional unreferenced siren sample. It was not imported
because no active cue/rate is established; this avoids inventing playback rules.

## Structured data

- `dialogue.json`:743 exact text entries and315 ordered/null-capable table references.
- `interface_layout.json`: palettes, portrait/glyph/gadget coordinates, profiles,
  country view origins, exact names and default high-score records.
- `animations.json`: readable frame/timing/source metadata; the C++ presentation
  functions remain timing authorities.
- `manifest.json`: texture/WAV dimensions, formats, mappings and payload hashes.

Regenerate from an explicitly supplied source with `tools/convert_assets.py`,
then run `tools/generate_data.py` and `tools/validate_assets.py`. Generated C++
tables are checked in; configure-time SHA256 checks prevent stale data.

## Application branding

The unchanged master PNG and derived Windows ICO, macOS ICNS and seven Linux PNG
sizes are listed in [APPLICATION_ICONS.md](APPLICATION_ICONS.md) and
`assets/branding/manifest.json`. These are platform/window branding, independent
of the game's original graphics atlases.
