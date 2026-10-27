#!/usr/bin/env python3
"""Save or verify the tracked source contents; does not alter the source reference."""

from pathlib import Path
import argparse, hashlib, json, subprocess, datetime

R = Path(__file__).resolve().parents[1]
record = R / "docs/validation/source_checkpoint.json"
p = argparse.ArgumentParser()
p.add_argument("--verify", action="store_true")
p.add_argument("--label", default="modernized-source-validated")
a = p.parse_args()


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


if a.verify:
    data = json.loads(record.read_text())
    bad = [
        name
        for name, value in data["sha256"].items()
        if not (R / name).is_file() or sha(R / name) != value
    ]
    assert not bad, bad
    print(
        f"PASS {len(data['sha256'])} source/data/test/document hashes; checkpoint {data['label']}"
    )
else:
    names = subprocess.check_output(["git", "ls-files", "-z"], cwd=R).decode().split("\0")
    paths = [name for name in names if name and name != "docs/validation/source_checkpoint.json"]
    commit = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=R, text=True).strip()
    data = {
        "label": a.label,
        "source_commit": commit,
        "saved_utc": datetime.datetime.now(datetime.timezone.utc).isoformat(),
        "validation": "docs/validation/results.json",
        "sha256": {name: sha(R / name) for name in paths},
    }
    record.write_text(json.dumps(data, indent=2) + "\n")
    print("Saved", a.label, len(paths), "source files")
