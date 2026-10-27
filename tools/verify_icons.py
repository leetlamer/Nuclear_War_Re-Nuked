#!/usr/bin/env python3
"""Validate generated icon sizes, provenance and native bundle when supplied."""

from pathlib import Path
import argparse, hashlib, json, plistlib
from PIL import Image

root = Path(__file__).resolve().parents[1]
p = argparse.ArgumentParser()
p.add_argument("--bundle", type=Path)
a = p.parse_args()
b = root / "assets/branding"
manifest = json.loads((b / "manifest.json").read_text())
assert (
    hashlib.sha256((b / "nuclear-war-renuked-icon.png").read_bytes()).hexdigest()
    == manifest["master_sha256"]
)
assert Image.open(b / "nuclear-war-renuked.ico").ico.sizes() == {
    (s, s) for s in (16, 24, 32, 48, 64, 128, 256)
}
icns = Image.open(b / "nuclear-war-renuked.icns")
assert {s[0] * s[2] for s in icns.info["sizes"]} >= {16, 32, 64, 128, 256, 512, 1024}
for s in (16, 32, 48, 64, 128, 256, 512):
    assert Image.open(b / f"linux/{s}x{s}/nuclear-war-renuked.png").size == (s, s)
text = (root / "packaging/linux/nuclear-war-renuked.desktop").read_text()
assert "Name=Nuclear War: Re-Nuked\n" in text and "Icon=nuclear-war-renuked\n" in text
if a.bundle:
    info = plistlib.loads((a.bundle / "Contents/Info.plist").read_bytes())
    assert info["CFBundleName"] == "Nuclear War: Re-Nuked"
    assert info["CFBundleIconFile"] == "nuclear-war-renuked.icns"
    assert (
        a.bundle / "Contents/Resources" / info["CFBundleIconFile"]
    ).read_bytes() == (b / info["CFBundleIconFile"]).read_bytes()
    assert (
        a.bundle
        / "Contents/Resources/assets/branding/linux/256x256/nuclear-war-renuked.png"
    ).is_file()
print(
    "PASS: master SHA256, ICO seven sizes, ICNS seven physical sizes, seven Linux PNGs, desktop mapping"
    + (", native app bundle/resources" if a.bundle else "")
)
