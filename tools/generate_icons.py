#!/usr/bin/env python3
"""Generate launch icons from the unchanged master (requires Pillow)."""

from pathlib import Path
from PIL import Image
import hashlib
import json
import io
import struct

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "assets/branding"
MASTER = OUT / "nuclear-war-renuked-icon.png"
source = Image.open(MASTER).convert("RGBA")
# Fit rather than crop, preserving the complete title even for future non-square masters.
canvas = Image.new("RGBA", (max(source.size),) * 2)
canvas.alpha_composite(
    source, ((canvas.width - source.width) // 2, (canvas.height - source.height) // 2)
)
for size in (16, 32, 48, 64, 128, 256, 512):
    target = OUT / "linux" / f"{size}x{size}"
    target.mkdir(parents=True, exist_ok=True)
    canvas.resize((size, size), Image.Resampling.LANCZOS).save(
        target / "nuclear-war-renuked.png"
    )
canvas.save(
    OUT / "nuclear-war-renuked.ico",
    sizes=[(s, s) for s in (16, 24, 32, 48, 64, 128, 256)],
)
# Explicit PNG chunks include true 16px/32px/64px slots and Retina slots.
entries = []
for code, size in (
    (b"icp4", 16),
    (b"icp5", 32),
    (b"icp6", 64),
    (b"ic07", 128),
    (b"ic08", 256),
    (b"ic09", 512),
    (b"ic10", 1024),
    (b"ic11", 32),
    (b"ic12", 64),
    (b"ic13", 256),
    (b"ic14", 512),
):
    data = io.BytesIO()
    canvas.resize((size, size), Image.Resampling.LANCZOS).save(data, format="PNG")
    payload = data.getvalue()
    entries.append(code + struct.pack(">I", len(payload) + 8) + payload)
body = b"".join(entries)
(OUT / "nuclear-war-renuked.icns").write_bytes(
    b"icns" + struct.pack(">I", len(body) + 8) + body
)
manifest = {
    "master_sha256": hashlib.sha256(MASTER.read_bytes()).hexdigest(),
    "master_dimensions": list(source.size),
    "resampling": "Pillow LANCZOS; centered transparent square padding; no crop",
    "files": [
        str(p.relative_to(ROOT))
        for p in sorted(OUT.rglob("*"))
        if p.is_file() and p.suffix in (".png", ".ico", ".icns")
    ],
}
(OUT / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
print("Generated icons from", MASTER.name)
