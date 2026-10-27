#!/usr/bin/env python3
"""Verify every modern asset and structured reference, with no original input."""

from pathlib import Path
import json, hashlib, struct, zlib, wave

R = Path(__file__).resolve().parents[1]
m = json.loads((R / "assets/data/manifest.json").read_text())
for e in m["images"]:
    b = (R / "assets" / e["file"]).read_bytes()
    assert b[:8] == b"\x89PNG\r\n\x1a\n"
    pos = 8
    compressed = bytearray()
    while pos < len(b):
        n = struct.unpack_from(">I", b, pos)[0]
        kind = b[pos + 4 : pos + 8]
        data = b[pos + 8 : pos + 8 + n]
        pos += n + 12
        if kind == b"IHDR":
            w, h, depth, color, *_ = struct.unpack(">IIBBBBB", data)
            assert (w, h, depth, color) == (e["width"], e["height"], 8, 6)
        elif kind == b"IDAT":
            compressed += data
    raw = zlib.decompress(compressed)
    stride = w * 4
    rgba = bytearray()
    for y in range(h):
        assert (
            raw[y * (stride + 1)] == 0
        ), "Re-export with convert_assets.py (canonical PNG filter0)"
        rgba += raw[y * (stride + 1) + 1 : (y + 1) * (stride + 1)]
    assert hashlib.sha256(rgba).hexdigest() == e["rgba_sha256"], e["file"]
for e in m["audio"]:
    with wave.open(str(R / "assets" / e["file"]), "rb") as f:
        assert (f.getnchannels(), f.getsampwidth(), f.getframerate(), f.getnframes()) == (
            1,
            1,
            e["sample_rate"],
            e["samples"],
        )
        signed = bytes((v + 128) & 255 for v in f.readframes(f.getnframes()))
        assert hashlib.sha256(signed).hexdigest() == e["signed_source_sha256"]
d = json.loads((R / "assets/data/dialogue.json").read_text())
ids = {int(x["id"], 16) for x in d["entries"]}
assert len(ids) == 743
assert all(int(x["id"], 16) == 0 or int(x["id"], 16) in ids for x in d["tables"])
print(
    f"PASS {len(m['images'])} exact RGBA images, {len(m['audio'])} reversible PCM WAVs,743 texts /315 references"
)
