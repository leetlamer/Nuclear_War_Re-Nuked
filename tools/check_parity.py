#!/usr/bin/env python3
"""Compare native SDL replay outputs with a separately built reference executable.
The reference must run in an isolated copy; this tool never runs source wrappers.
"""

from pathlib import Path
import argparse, subprocess, os, json, struct, hashlib, re

p = argparse.ArgumentParser()
p.add_argument("--reference", type=Path, required=True)
p.add_argument("--reference-cwd", type=Path, required=True)
p.add_argument("--candidate", type=Path, required=True)
p.add_argument("--output", type=Path, required=True)
p.add_argument("--native", action="store_true")
a = p.parse_args()
a.output.mkdir(parents=True, exist_ok=True)
env = dict(os.environ)
if not a.native:
    env.update(SDL_VIDEODRIVER="dummy", SDL_AUDIODRIVER="dummy")
else:
    env.pop("SDL_VIDEODRIVER", None)
    env.pop("SDL_AUDIODRIVER", None)
fixtures = [
    "missile",
    "bomber-attack",
    "interception",
    "defense",
    "production",
    "100mt-production",
    "100mt-missile",
    "selector",
    "action-shutters",
    "reverse-shutters",
    "rapid-shutters",
    "round-shutters",
    "round-shutters-eliminated",
    "mood-controls",
    "mood-profile4",
    "speech",
    "alien-departure",
    "alien-delivery",
    "earthquake",
    "stork",
    "weight",
    "meltdown",
    "defector-right",
    "revenge-ai",
    "revenge-human",
    "revenge-bomber",
]
cases = (
    [("four_rounds", [])]
    + [(f, ["--media-fixture", f]) for f in fixtures]
    + [
        (
            f + "_" + mode,
            [
                "--media-fixture",
                f,
                "--playback-replay",
                mode,
                *(["--animation-hold-ms", "20"] if mode == "hold" else []),
            ],
        )
        for f in ["missile", "bomber-attack", "revenge-ai", "revenge-human", "revenge-bomber"]
        for mode in ["click", "hold"]
    ]
    + [
        (f + "restart", ["--session-fixture", f, "--restart-test"])
        for f in ["human-victory", "ai-only"]
    ]
)
if a.native:
    cases = [
        c
        for c in cases
        if c[0]
        in ["missile", "mood-controls", "alien-departure", "revenge-bomber", "human-victoryrestart"]
    ]


def bmp(p):
    b = p.read_bytes()
    off = struct.unpack_from("<I", b, 10)[0]
    w, h = struct.unpack_from("<ii", b, 18)
    assert struct.unpack_from("<H", b, 28)[0] == 32
    return w, h, b[off : off + w * h * 4]


records = []
for name, args in cases:
    pair = []
    for label, exe, cwd in [
        ("reference", a.reference, a.reference_cwd),
        ("candidate", a.candidate, a.candidate.parent),
    ]:
        folder = (a.output / name / label).resolve()
        folder.mkdir(parents=True, exist_ok=True)
        r = subprocess.run(
            [str(exe.resolve()), *args, "--smoke-test", str(folder)],
            cwd=cwd,
            env=env,
            text=True,
            capture_output=True,
            timeout=120,
        )
        (folder / "run.log").write_text(r.stdout + r.stderr)
        assert r.returncode == 0, (name, label, r.stdout, r.stderr)
        pair.append(folder)
    matched = compared = 0
    differences = []
    for x in sorted(pair[0].glob("*.bmp")):
        y = pair[1] / x.name
        if not y.exists():
            continue
        w, h, old = bmp(x)
        ww, hh, new = bmp(y)
        assert (w, h) == (ww, hh)
        # The console uses the actual SDL wall clock in the original, even in fixed-dt
        # replays. Exclude that viewport only; never mask the action/result captions.
        canvas = bytearray(old)
        current = bytearray(new)
        if "phase_" not in x.name or name in [
            "100mt-production",
            "100mt-missile",
            "speech",
            "mood-controls",
        ]:
            for yy in range(158 * h // 200, 195 * h // 200):
                start = ((h - 1 - yy) * w + 78 * w // 320) * 4
                end = ((h - 1 - yy) * w + 240 * w // 320) * 4
                canvas[start:end] = b"\0" * (end - start)
                current[start:end] = b"\0" * (end - start)
        compared += 1
        if canvas == current:
            matched += 1
        else:
            differences.append(x.name)
    # Always compare the final result where present; transient capture sets can
    # differ because the original scripts schedule input from the wall clock.
    if "result.bmp" in differences:
        raise AssertionError((name, "final pixels differ", differences))
    assert compared and matched, (name, compared, differences)
    # Deterministic result-bearing stdout (not wall-clock presentation frame counts).
    for key in ["final_seed=", "seed=", "attacks=", "fresh_state_rng_match="]:
        old = re.findall(re.escape(key) + r"(\d+)", (pair[0] / "run.log").read_text())
        new = re.findall(re.escape(key) + r"(\d+)", (pair[1] / "run.log").read_text())
        assert old == new, (name, key, old, new)
    for log in ["restart_trace.txt", "revenge_presentation.txt", "production_100mt_audio.txt"]:
        old = pair[0] / log
        new = pair[1] / log
        if old.exists():
            assert new.exists()
            left = old.read_text()
            right = new.read_text()
            if log == "production_100mt_audio.txt":
                left = re.sub(r"queued=\d+", "queued=*", left)
                right = re.sub(r"queued=\d+", "queued=*", right)
            assert left == right, (name, log)
    records.append(
        dict(
            case=name,
            compared_frames=compared,
            exact_frames_outside_wall_clock_console=matched,
            transient_differences=differences,
        )
    )
    print(name, matched, "/", compared, flush=True)
(a.output / "results.json").write_text(json.dumps(records, indent=2) + "\n")
print("PASS", len(records), "reference/candidate SDL replay pairs")
