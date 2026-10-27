#!/usr/bin/env python3
"""Read-only, complete content/inventory comparison against a saved source manifest."""

from pathlib import Path
import argparse, hashlib, json

p = argparse.ArgumentParser()
p.add_argument("source", type=Path)
p.add_argument("manifest", type=Path)
p.add_argument("--report", type=Path)
a = p.parse_args()
expected = json.loads(a.manifest.read_text())
current = {str(p.relative_to(a.source)): p for p in a.source.rglob("*") if p.is_file()}
changed = []
for name, path in current.items():
    if name not in expected:
        changed.append("added:" + name)
        continue
    h = hashlib.sha256()
    with path.open("rb") as f:
        for b in iter(lambda: f.read(1024 * 1024), b""):
            h.update(b)
    if h.hexdigest() != expected[name]:
        changed.append("changed:" + name)
changed += ["removed:" + name for name in expected if name not in current]
result = {"files": len(expected), "unchanged": not changed, "differences": changed}
if a.report:
    a.report.write_text(json.dumps(result, indent=2) + "\n")
print(json.dumps(result))
raise SystemExit(bool(changed))
