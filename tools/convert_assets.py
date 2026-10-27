#!/usr/bin/env python3
"""Lossless import from an explicitly supplied GameReverse source tree.
Writes only the output project. Python standard library; no Amiga tools required.
"""
from pathlib import Path
import argparse,hashlib,json,struct,zlib,binascii,wave,re
p=argparse.ArgumentParser();p.add_argument('source',type=Path);p.add_argument('--output',type=Path,default=Path(__file__).resolve().parents[1]);args=p.parse_args();S=args.source;O=args.output
raw=S/'port/assets/extracted/raw';manifest=json.loads((S/'port/assets/extracted/asset_manifest.json').read_text());assets={e.get('id'):e for e in manifest['assets']}
pal=assets['palette_main']['words'];end=assets['palette_ending_survivor']['words'];dead=assets['palette_ending_all_dead']['words']
def sha(b):return hashlib.sha256(b).hexdigest()
def chunk(tag,b):return struct.pack('>I',len(b))+tag+b+struct.pack('>I',binascii.crc32(tag+b)&0xffffffff)
def png(path,w,h,rgba):
 path.parent.mkdir(parents=True,exist_ok=True);scan=b''.join(b'\0'+rgba[y*w*4:(y+1)*w*4] for y in range(h));path.write_bytes(b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>IIBBBBB',w,h,8,6,0,0,0))+chunk(b'IDAT',zlib.compress(scan,9))+chunk(b'IEND',b''))
images=[]
specs=[('start',200,'','images/backgrounds/character_selection.png',pal),('dip',200,'','images/interface/main_window.png',pal),('leaders',160,'','images/characters/leader_portraits.png',pal),('world_map',200,'','images/backgrounds/world_map.png',pal),('smalltiles',272,'','images/interface/control_tiles.png',pal),('smalltiles',272,'mask_smallmask_z_m','images/effects/small_sprites.png',pal),('big512',288,'mask_big512mask','images/missiles/flight_and_effects.png',pal),('bigtiles',432,'mask_bigmask','images/effects/large_sprites.png',pal),('bigtiles',432,'','images/interface/selection_backdrops.png',pal),('endchar',112,'mask_endchar_mask','images/characters/victory_body.png',end),('spacemap',200,'','images/backgrounds/space_ending.png',dead),('desolate_map',200,'','images/backgrounds/desolate_ending.png',end)]
for key,h,mask,name,palette in specs:
 data=(raw/f'graphics/{key}.indices').read_bytes();bits=(raw/f'graphics/{mask}.1bit').read_bytes() if mask else None
 rgba=bytearray()
 for i,c in enumerate(data):
  color=palette[c];rgba.extend([(color>>8&15)*17,(color>>4&15)*17,(color&15)*17,255 if bits is None or bits[i//8]>>(7-i%8)&1 else 0])
 png(O/'assets'/name,320,h,bytes(rgba));images.append(dict(key=key,mask=mask,file=name,width=320,height=h,rgba_sha256=sha(rgba),transparency='original 1-bit mask' if bits else 'opaque',source_mapping=f'{key}: indexed pixels + original palette'+(' + '+mask if mask else ''),frame_count='atlas; see animations.json'))
b=(S/'port/assets/extracted/fonts/ld_8_strike.raw').read_bytes();rgba=bytearray()
for y in range(8):
 for x in range(576):rgba.extend([255,255,255,255 if b[y*72+x//8]>>(7-x%8)&1 else 0])
name='fonts/original_bitmap.png';png(O/'assets'/name,576,8,rgba);images.append(dict(key='font',file=name,width=576,height=8,rgba_sha256=sha(rgba),transparency='glyph bits',source_mapping='ld_8_strike.raw',frame_count=97))
audio=[]
rates={'exp':17734,'missile':4961,'prop':3550,'engine':5911,'build':27284,'feet':5067,'moo':4961,'door.snd':8547,'ooo':5358,'16ton':4961,'iwon':9586,'siren':4961,'ticker':27928,'selection_bleep':5067}
readable={'exp':'explosion','door.snd':'shutter','16ton':'falling_weight','ooo':'warhead_exclamation','iwon':'ai_victory','prop':'propaganda','engine':'bomber_engine','feet':'defector_steps','missile':'missile_launch','build':'factory','moo':'cow','siren':'siren','ticker':'typewriter','selection_bleep':'selection_bleep'}
waves={p.stem:p.read_bytes() for p in (raw/'audio').glob('*.s8')}
noise=0x4e575449;ticker=[]
for divisor in range(255,0,-2):noise=(noise*1664525+1013904223)&0xffffffff;n=((noise>>16)&32767)%divisor+1-divisor//2;ticker.append(n&255)
waves['ticker']=bytes(ticker);waves['selection_bleep']=bytes(v&255 for v in sum(([i,-i] for i in list(range(1,127))+list(range(127,0,-1))),[]))
for key,signed in sorted(waves.items()):
 name='audio/effects/'+readable[key]+'.wav';dest=O/'assets'/name;dest.parent.mkdir(parents=True,exist_ok=True)
 # RIFF PCM8 is unsigned. Biasing is exactly reversible, with no filtering.
 pcm=bytes((v+128)&255 for v in signed)
 with wave.open(str(dest),'wb') as w:w.setnchannels(1);w.setsampwidth(1);w.setframerate(rates[key]);w.writeframes(pcm)
 audio.append(dict(key=key,file=name,channels=1,bits=8,sample_rate=rates[key],samples=len(pcm),signed_source_sha256=sha(signed),source_mapping=key+'.s8' if key not in ['ticker','selection_bleep'] else 'unchanged deterministic UI waveform',rate_policy='cue period and 3546895 clock still determine effective source rate; production ooo 1.08x'))
data=(S/'analysis/unpacked/segment_1_original.bin').read_bytes();font=json.loads((S/'port/assets/extracted/fonts/ld_8_metrics.json').read_text());gadgets=json.loads((S/'analysis/amiga_loader_comparison/evidence/gadget_chain.json').read_text());catalog=json.loads((S/'port/assets/original_text_catalog.json').read_text());names=[e['exact_text'] for e in catalog['entries'] if e['category']=='leader_names'];high=(S/'original/Nuclear War Amiga Sourcecode/HighScores').read_bytes();scores=[]
for i in range(10):
 row=high[i*17:(i+1)*17];name=row[:11].split(b'\0')[0].decode('ascii');assert name.encode().ljust(11,b'\0')==row[:11];scores.append(dict(name=name,population=int.from_bytes(row[11:13],'big'),cities=int.from_bytes(row[13:15],'big'),score=int.from_bytes(row[15:17],'big')))
layout=dict(palette=pal,ending_palette=end,all_dead_palette=dead,portraits=[struct.unpack_from('>4h',data,0x1454+44*i) for i in range(10)],profiles=struct.unpack_from('>10H',data,0x13ba),names=names,origin_x=struct.unpack_from('>5H',data,0xd24),origin_y=struct.unpack_from('>5H',data,0xd2e),glyphs=[dict(offset=g['bit_offset'],width=g['width']) for g in font['glyphs']],gadgets=[{k:g[k] for k in ['id','left','top','width','height']} for g in gadgets],initial_high_scores=scores)
text=(S/'port/assets/original_text_data.hpp').read_text();rows=re.findall(r'\{(0x[0-9a-f]+)u, ("(?:\\.|[^"\\])*")\}',text);pointers=re.findall(r'\{"([^"]+)", (\d+)u, (0x[0-9a-f]+)u\}',text)
dialogue=dict(entries=[dict(id=a,text=json.loads(t)) for a,t in rows],tables=[dict(name=t,index=int(i),id=a) for t,i,a in pointers]);assert len(rows)==743 and len(pointers)==315
for name,value in [('manifest',dict(schema=1,images=images,audio=audio)),('interface_layout',layout),('dialogue',dialogue)]:
 dest=O/f'assets/data/{name}.json';dest.parent.mkdir(parents=True,exist_ok=True);dest.write_text(json.dumps(value,ensure_ascii=True,indent=2)+'\n')
print(f'Converted {len(images)} exact RGBA PNG textures, {len(audio)} PCM WAVs,743 text entries and315 table references')
