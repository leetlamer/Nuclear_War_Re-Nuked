#!/usr/bin/env python3
"""Read-only import of three recovered original ending images. No existing asset changed."""
import argparse, hashlib, json, struct, zlib, binascii
from pathlib import Path
parser=argparse.ArgumentParser()
parser.add_argument('reference', type=Path)
args=parser.parse_args()
root=Path(__file__).resolve().parents[1]
raw=args.reference/'port/assets/extracted'
source=json.loads((raw/'asset_manifest.json').read_text())
palette=next(x['words'] for x in source['assets'] if x.get('id')=='palette_ending_all_dead')
manifest_path=root/'assets/data/manifest.json'
manifest=json.loads(manifest_path.read_text())
def chunk(tag,data):
    return struct.pack('>I',len(data))+tag+data+struct.pack('>I',binascii.crc32(tag+data)&0xffffffff)
for name,height in [('expmap',200),('smallexp',176),('bigexp',432)]:
    indices=(raw/f'raw/graphics/{name}.indices').read_bytes()
    assert len(indices)==320*height
    rgba=bytes(v for i in indices for v in (((palette[i]>>8)&15)*17,((palette[i]>>4)&15)*17,(palette[i]&15)*17,255))
    relative=f'images/effects/{name}.png'
    scan=b''.join(b'\0'+rgba[y*1280:(y+1)*1280] for y in range(height))
    (root/'assets'/relative).write_bytes(b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>IIBBBBB',320,height,8,6,0,0,0))+chunk(b'IDAT',zlib.compress(scan,9))+chunk(b'IEND',b''))
    entry=dict(key=name,mask='',file=relative,width=320,height=height,rgba_sha256=hashlib.sha256(rgba).hexdigest(),transparency='opaque native rectangle copy',source_key=name,source_mask='',source_mapping=f'{name}: indexed pixels + palette_ending_all_dead',frame_catalog='data/animations.json',frame_count=5 if name!='expmap' else 1)
    manifest['images']=[e for e in manifest['images'] if e['key']!=name]+[entry]
manifest_path.write_text(json.dumps(manifest,indent=2)+'\n')
