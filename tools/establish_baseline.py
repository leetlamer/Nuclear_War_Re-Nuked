from pathlib import Path
import hashlib,json,shutil
source=Path('/Users/zellcorp/GameReverse');target=Path(__file__).resolve().parents[1];baseline=target/'.migration/baseline'
def digest(p):
 h=hashlib.sha256()
 with p.open('rb') as f:
  for block in iter(lambda:f.read(1024*1024),b''):h.update(block)
 return h.hexdigest()
records={str(p.relative_to(source)):digest(p) for p in sorted(source.rglob('*')) if p.is_file()}
(target/'docs/source_workspace_sha256.json').write_text(json.dumps(records,indent=2)+'\n')
for name in ['player','engine','session','resolver','prototype','missile_prototype','assets','evidence']:
 shutil.copytree(source/'port'/name,baseline/'port'/name,dirs_exist_ok=True,ignore=shutil.ignore_patterns('__pycache__','*.dSYM'))
for name in ['analysis/unpacked/segment_1_original.bin','analysis/amiga_loader_comparison/evidence/gadget_chain.json','original/Nuclear War Amiga Sourcecode/HighScores','analysis/emoji_speech/run_regressions.py','analysis/emoji_speech/build/player_assets.hpp']:
 dest=baseline/name;dest.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(source/name,dest)
for name in ['analysis/release_candidate/evidence','analysis/final_seven/evidence','analysis/emoji_speech/evidence']:(baseline/name).mkdir(parents=True,exist_ok=True)
print('Recorded',len(records),'original files; created isolated baseline test workspace',flush=True)
