> Historical 2026-10-29 report. The later selective UFO rollback is documented in SELECTIVE_SPECIAL_EVENT_ROLLBACK.md; roster and extinction remain intact.

# Roster, extinction and UFO restoration — 2026-10-29

## Status

**Implemented:** explicit 1–4 AI roster; complete recovered total-extinction Earth animation and space Hall.

**Partly implemented / fidelity blocker:** recovered UFO yellow-frame animation restored to the existing natural alien event. A separate **hostile city-damaging** UFO event could not be established from the recovered dispatcher, strings and resolver. No invented damage, dialogue or probability was added. It is not correct to claim all three requested features are complete.

## Roster

Original `complete_selection` filled unselected portraits at random until four, then assigned selected identities using random start + cyclic scan. Header/banner confirms, not the fourth portrait. Modern policy deliberately removes random filling: select 1–4 portraits then click the banner; zero selections cannot begin. Existing random assignment among explicitly selected portraits is preserved. Four-selected RNG/assignment remains unchanged.

`State::participating` defaults to all five for existing fixtures. The new-game roster marks only chosen AI slots occupied; initialization leaves absent actors empty/action88, without initialization RNG/stock. AI planning skips absent slots. Existing population guards then exclude them from production, attacks, special-event targets, revenge, survivor/endgame checks. Renderer omits absent portraits/names/emojis and full-map cities; selection cannot inspect an absent slot. This is occupancy metadata, not another gameplay inventory. Fresh `Player` initialization on restart permits a different roster.

Tests cover all four sizes, zero-choice rejection, absent stock, no planning, three consecutive resolved human/AI rounds per roster (or a legitimate ending), survival and pending-presentation boundaries. Existing restart replay passes.

## Extinction

Original entry **11437C**, last survivor -1. See [EXTINCTION_EVIDENCE.md](EXTINCTION_EVIDENCE.md) for exact instruction/array evidence. `expmap` is a **static320×200 intact globe**. Cracks come from three separate opaque overlays in **smallexp320×176**, followed by two small explosion stages. `bigexp320×432` has **five variable-size frames**, not one image:

|Order|Source rectangle x,y,w,h|Destination x,y|
|---|---|---|
|1|0,0,160,128|81,28|
|2|160,0,160,144|82,16|
|3|0,128,160,160|81,16|
|4|160,144,160,160|82,16|
|5|0,288,160,144|82,16|

The original opaque blits accumulate. Each render reconstructs that composition from the intact Earth and all completed overlays. No invented alpha, particles, artwork, frame rectangles or cosmetic RNG. Final stage clears to original space before Hall; all-dead creates no score candidate. Existing resolver and FIFO revenge complete before `EndgamePresentation::begin`; pending scenes cannot enter it.

PAL-derived timing is **INFERRED from VERIFIED WaitTOF counts**, not measured hardware: intact Earth0.18s; first three stages0.20s each; next six0.22s each; final frameapproximately0.04s; clean-space pause0.60s. Totalapproximately2.74s at regular frame rate. A slow host is allowed to lengthen the presentation so no recovered stage is dropped. Sound starts at stage3: original `exp.s8`/existing explosion.wav,4998bytes, period620, volume64, once; default3546895 clock gives5721Hz. No other audio rates changed.

`tools/import_extinction_assets.py <read-only GameReverse>` reproducibly imports only these three PNGs using original all-dead palette; `python3 tools/generate_data.py` regenerates checked tables. No existing artwork/audio changed. Native rectangles and destinations are also in assets/data/animations.json.

## UFO evidence and limitation

See [ALIEN_ATTACK_EVIDENCE.md](ALIEN_ATTACK_EVIDENCE.md). Eight UFO frames: **(0,210,32,10)** through **(224,210,32,10)** in32pixel steps. Four yellow16×16 frames: **(256,208)**, **(272,208)**, **(288,208)**, **(304,208)**. Native secondary placement is craftX+12,craftY+3. Restored those original frames instead of procedural beams. Craft descends, displays yellow frames and exits upward within the existing3second presentation; seconds-based movement/exit is explicitly **host reconstruction**, not verified original choreography. The separate city-liftoff exhaust stays unchanged.

Recovered announcement is **`Space Aliens land on %s.`**. Effect69 doubles the first strictly-largest positive own city with existing16bit wrapping; marker23000 announces **`%s gains %d Million people.`**. Natural action3 substitution remains unchanged: gate next()%100+1<30, second roll7–8 selects16. Handler draws remain variant%2+1, magnitude%10+1 and initialX%200+51. Action10 is skipped by the recovered normal dispatcher. These facts do not establish a hostile UFO event. Neither immigration nor city liftoff was changed into a damaging event to manufacture a test pass.

## Desktop validation actually executed

- Baseline optimized:16/16 CTest suites passed before edits.
- Final optimized:16/16 passed; player176groups/178644checks, revenge20groups/790checks.
- Final UBSan:16/16 passed (`-fsanitize=undefined -fno-sanitize-recover=all`).
- Asset validation:16 exact RGBA images,13 reversible original WAVs,743 texts/315 references.
- Native SDL replays: all-dead, human-death/AI-only, alien-delivery, revenge-human passed; revenge4attacks, no acknowledgement, seed3008955749.
- `tools/capture_restoration.cpp` compiled and ran against production renderer/library. Captures include exactly one opponent, intact/cracked Earth, every explosion stage, space Hall, UFO descent/yellow frames/exit. Selected captures were visually inspected. Original all-dead video and audible speaker playback were not verified; do not claim hardware/recording timing parity.

Evidence: [validation/restoration](validation/restoration). Browser synchronization and test results are recorded in the web project's corresponding report. No publishing, deployment or shutdown. GameReverse was read-only throughout. Existing packaged release ZIPs were not regenerated by this source milestone.

## Changed components

- Roster/state: include/nuclear/engine.hpp, src/ui/player_controller.{hpp,cpp}, src/game/game_rules.cpp, src/game/ai/ai_controller.cpp.
- Ending/render/audio: src/ui/endgame.hpp, src/rendering/renderer.hpp, src/rendering/animations/extinction.hpp, src/platform/application.cpp.
- UFO: src/rendering/animations/special_events.hpp.
- Data/assets: three new original PNGs, assets/data/{manifest,animations}.json, generated asset registry/CMake checksums.
- Tests/tools: tests/frontend/tests.cpp, explicit-roster replay_scene_setup.inc, capture_restoration.cpp, import_extinction_assets.py.

Remaining blocker: recover a distinct hostile UFO dispatch/damage/dialogue path or obtain explicit modern-design rules. It has not been silently implemented as a new random event.
