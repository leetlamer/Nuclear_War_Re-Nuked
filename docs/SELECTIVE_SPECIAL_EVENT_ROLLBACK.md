# Selective special-event rollback — 2026-10-30

## Safety and history

Desktop started clean at7efb5d7; browser started clean atdfb3dc3. Inspected status, log, reflog, show and diffs against `roster-extinction-restored-ufo-partial`. Browser restoration commits had been rebased from327ca49/fa13536 to3f70f9f/dfb3dc3; the intervening README change was preserved.

Both repositories have safety tag `safety-before-special-event-rollback-20261030` and verified **complete-history** bundles at `.git/special-event-safety-20261030.bundle`. No whole-commit revert, reset, force push or deletion was used. GameReverse remained read-only.

Desktop inspected:667af1d (previous working special events),a1a8327 (protected roster),850da3b (protected extinction),405883f (UFO change),7efb5d7 (UFO departure guard). Browser inspected:3f70f9f/dfb3dc3 and milestone tag. Only UFO action16 changed last session; city-liftoff action11 and its resolver did not.

## Special-event correction

Restored **only the action16 function branch** verbatim from667af1d: original single32×10 UFO, original descent/landing interpolation and frame cycle. Removed yellow secondary sprite emission and the newly introduced departure motion. No hostile event, new damage, dialogue or RNG was added. Existing immigration population gain/audio remain unchanged. Removed only `alien yellow secondary` metadata; all original PNG sheets remain intact, including their unused yellow pixels.

### City refresh

The concrete reproduced problem was a second, stationary copy of the pre-effect city being redrawn beneath the rising city. The renderer redraws the world every frame from the presentation snapshot; this was not an SDL texture-cache invalidation failure. The old source still shows the ground city during ascent, and the final exhaust was emitted at exactly3seconds even after logical completion. Last-session history does not establish a separate city-resolver regression.

The action11 target ground tile now shows the existing empty/ruined tile150 while the city is airborne, like the existing earthquake background treatment. At completion, the renderer uses the normal resolved population snapshot (zero for the removed city). City/exhaust overlays now end at `seconds >= duration`, so they cannot redraw at the completion boundary. City artwork, trajectory, exhaust source, speed, population calculation and RNG are unchanged.

Actual production-renderer negative control: pre-fix header gave **16524 differing RGB channels** in the ground-city region between ascent1s and resolved3.2s. Corrected renderer gives **0 differences**. `city_refresh_tests` now asserts this using actual SDL pixels. Unit tests also check the before/after population snapshot and no overlay at3s. Native captures show the city rising with original exhaust, empty ground site, and resolved site after cleanup. Browser regression compares exact atlas/world-composited ground pixels during ascent and after completion, then confirms the action releases after normal input.

An indefinitely stale city after all subsequent turns was not independently reproduced; the reproducible redraw/completion defects above are fixed rather than assuming an unobserved cache failure.

## Protected features — completely unchanged

`validation/special-event-rollback/protected-features.json` records byte-identical SHA256 verification against the safety tag for:

- State occupancy, initialization/AI planning, player-controller roster and selection logic.
- Entire extinction frame header, endgame state machine and application loop (including sound trigger/reset).
- expmap,smallexp,bigexp and original explosion WAV.
- Renderer extinction block, including all ten stages, all five bigexp frames, original destinations, timing, accumulating overlays and space Hall.

The existing1–4 roster and extinction regression test groups are themselves unchanged. All original extinction captures (intact Earth, three cracks, both small explosions, five big explosions and space Hall) remain **byte-identical** to last-session PNG captures. Extinction metadata is unchanged; only the UFO-secondary metadata entry was removed.

Explosion sound trigger remains `exp`,4998bytes,period620,volume64,once atstage3; clock3546895. Sound was not edited. Native all-dead replay completed through space Hall. No claim of audible listening verification.

## Desktop validation

- Release build and **17/17** CTest suites pass, including new actual-renderer city refresh test.
- UBSan build and **17/17** suites pass.
- Existing explicit1–4 roster/repeated-round tests, extinction for different roster sizes, pending-presentation gating, every explosion frame, Hall and restart tests remain passing.
- Native SDL replays: `alien-delivery`, `alien-departure`, `all-dead` pass; no resolver hang.
- Native before/after city and alien captures visually inspected. Earth captures match previous ones exactly.
- Other event branches match previous working code; quake, stork, meltdown, catapult, weight and defence logic/timing untouched.

## Files

Runtime:src/rendering/animations/special_events.hpp (restore UFO and strict city cleanup),src/rendering/renderer.hpp (target ground redraw only).
Metadata:assets/data/animations.json,generated cmake/GeneratedData.cmake.
Tests:tests/frontend/tests.cpp,tests/frontend/city_refresh_tests.cpp,CMakeLists.txt,tools/capture_restoration.cpp.
Documentation/handoff/evidence updated. Separate browser changes mirror only those runtime/metadata hunks; its viewport, speech layer, loading, input, audio and Pages configuration remain unchanged. See browser docs/SELECTIVE_SPECIAL_EVENT_ROLLBACK.md for its final tests and commit.

No original assets deleted. No deployment, push or shutdown. No known extinction or roster regression remains in executed tests.
