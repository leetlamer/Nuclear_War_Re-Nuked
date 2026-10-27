# Contributing

Use C++17 and the checked-in `.clang-format` (LLVM-based,4-space indentation).
Keep changes small and explain why compatibility-sensitive code exists.

Before changing a rule, read ARCHITECTURE.md and run the relevant deterministic
tests. Preserve `Word` widths, explicit wrap/sign semantics, RNG draw count/order,
actor ordering and production boundaries. Do not consume gameplay RNG for UI.
Unknown reverse-engineered fields must not receive speculative names or meanings.

Do not rebuild speech/turn snapshots to refresh a live UI selection. Do not apply
damage, inventory changes or production in animation callbacks. Existing committed
display snapshots and result records separate presentation from simulation.

After editing:

```sh
cmake --build build --parallel 4
ctest --test-dir build --output-on-failure
python3 tools/validate_assets.py
```

Also run Debug/UBSan for simulation changes. A passing unit suite alone does not
establish visual/audio parity: use SDL frame captures before RenderPresent and
compare decoded PCM. The original renderer invalidates some native buffers after
presentation, so do not infer a rendering regression from those captures alone.

Edit JSON under `assets/data`, then run `python3 tools/generate_data.py`. The
committed generated headers and CMake fingerprints must agree. PNG conversion
retains source pixels and alpha; WAV conversion is reversible. Avoid artistic
filtering, rescaling, alternate fonts or reordered text. Existing asset provenance
and third-party rights are not replaced by this project.

`tools/check_parity.py` accepts explicit reference/candidate executable paths and an
isolated reference working directory. Never use it with test writers rooted in a
protected original workspace. `tools/deterministic_trace.cpp` provides a compact
phase/state oracle. Historical prototype/missile-demo helpers are test-only
compatibility oracles; they are not alternate game implementations shipped at runtime.
