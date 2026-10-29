# Original total-extinction animation evidence

Investigation 2026-10-29. GameReverse was read-only. These findings come from existing Amiga-aware Ghidra exports and original extracted PNGs, not a new runtime observation. No new Ghidra project or reimport was made.

## Root cause and sources

The modern `EndgamePresentation` had an all-dead branch but no extinction animation; `hall()` became true after 0.8 seconds and `Renderer` drew the space background immediately. Original ending routine **0x11437c** (`ending_score_replay_interface`, aware address0x23337c) contains a separate last-survivor==-1 path that loads `spacemap.z`, `expmap.z`, `smallexp.z`, `bigexp.z` and the all-dead palette before an explicit ten-stage animation. Existing audit had left the rectangle map unresolved; the arrays and call instructions recover it completely.

Read-only reference paths beneath `/Users/zellcorp/GameReverse`:

- `analysis/second_pass/production/pseudocode/old_11437c.c`: full ending flow. Decompiler proxy signatures truncate native blitter arguments, so use the instructions below for geometry.
- `analysis/asset_extraction/audio/evidence/instructions.json`: original-address ending instructions114990..114cb0, including all seven blitter arguments, sound and wait counts.
- `analysis/second_pass/production/evidence/DATA.bin`: native big-endian arrays, based at0x23b09c.
- `analysis/second_pass/callbacks/evidence/instructions.json`: original10126a native rectangular blitter.
- `analysis/asset_extraction/graphics/instructions.json`: original1199c6..119a02 initialization of the source-tile-offset table.
- `port/assets/extracted/graphics/{spacemap,expmap,smallexp,bigexp}.png`: actual decoded original artwork, visually inspected in this investigation.

## VERIFIED assets and frames

`expmap.png` is **320×200**, a single intact Earth against space. It does not contain cracking frames. `smallexp.png` is **320×176**, with three crack overlays across its upper64 rows and two early explosions underneath. `bigexp.png` is **320×432**, an irregularly packed five-frame sheet. It must never be rendered as one explosion sprite.

The first three stages overlay separate portions of the existing globe; keep the preceding framebuffer contents. Do not replace the Earth with just a crack rectangle. The native blitter is opaque:10126a sets BLTCON0=0x07ca plus shift, BLTCON1 shift, disabled channelA with BLTADAT=-1 and channelsB/C/D enabled; mintermCA then copies sourceB with edge masking. Do not invent color-key transparency.

|Stage|Source|Source x,y,w,h|Destination x,y|Meaning|
|---|---|---|---|---|
|0|smallexp|0,0,48,38|151,43|First crack region|
|1|smallexp|48,0,112,64|113,74|Second crack region|
|2|smallexp|160,0,80,64|149,78|Third crack region|
|3|smallexp|0,64,112,112|111,42|First explosion|
|4|smallexp|112,64,112,112|103,39|Second explosion|
|5|bigexp|0,0,160,128|81,28|Large explosion1|
|6|bigexp|160,0,160,144|82,16|Large explosion2|
|7|bigexp|0,128,160,160|81,16|Large explosion3|
|8|bigexp|160,144,160,160|82,16|Large explosion4|
|9|bigexp|0,288,160,144|82,16|Large explosion5|

Exact decoded big-endian native arrays:

- 0x23c3ca source indices: `0,3,10,80,87,0,10,160,190,360`.
- 0x23c3de destinationX: `151,113,149,111,103,81,82,81,82,82`.
- 0x23c3f2 destinationY: `43,74,78,42,39,28,16,16,16,16`.
- 0x23c406 widths: `48,112,80,112,112,160,160,160,160,160`.
- 0x23c41a heights: `38,64,64,112,112,128,144,160,160,144`.

Source index table0x23cffe is initialized by1199c6..119a02 as `(index%20)*16/8 + (index/20)*16*40`, matching320-pixel/40-byte planar stride. Therefore source coordinates are `((index%20)*16,(index/20)*16)`, not arbitrary equal-sized frames.

Native sequence draws each stage twice, one for each doublebuffer, separated by one WaitTOF. That is **not two animation cycles**. Stages0..4 select small-explosion source planes; stages5..9 select big-explosion source planes. A low-memory branch0x23cae6!=0 stops after stage4 and omits loading bigexp, but a modern release with the complete asset should play all ten stages. After stage9, original copies the clean `spacemap` into both buffers, waits30frames, and draws Hall-O-Fame. No score candidate is created for actor−1.

## Timing and sound

**VERIFIED wait counts:** stages0..3 wait9WaitTOF before drawing, stages4..9 wait10; each stage then has one WaitTOF between doublebuffer updates. A final30WaitTOF follows clearing to space. Graphics and OS execution add overhead. Exact elapsed seconds were not measured on Amiga hardware.

**INFERRED portable timing:** with PAL50Hz, the first draw begins after9/50=0.18seconds. Successive stage onsets derived from the waits are approximately `[0.18,0.38,0.58,0.78,1.00,1.22,1.44,1.66,1.88,2.10]` seconds, excluding startup/copy overhead. This means stages0..2 last0.20seconds and stages3..8 last0.22seconds. Stage9 is followed by a doublebuffer update and another final buffer-swap wait before clearing, rather than a further10-frame stage wait; its exact visible duration requires display-list timing verification. The clear-space pause is30/50=0.60seconds. A longer final-frame hold would be an explicit host approximation. Clearly identify these as wait-count-derived timings, not measured footage timings.

**VERIFIED sound:** original1149a2 tests stage==3. At1149d8 it submits sample pointer0x23cc48 (original0x131bac, `exp.s8`), prefix0x1386=4998bytes, cycles1, period620, volume64. Reference `analysis/asset_extraction/audio/NOTES.md` records this mapping too. Configured clock3546895/620≈5720.798Hz (rounded5721Hz). Preserve that sample and timing, not missile-launch playback rate200.

## Limits

No alternate-ending playback from original footage was observed here. Original elapsed seconds, hardware filtering and audio-channel arbitration remain unmeasured. Frame coordinates/order, source identity, native wait counts and sound call are static verified facts. Requested endgame ordering must still wait for pending resolver/revenge work before beginning this presentation; this document does not assert that integration has been tested.
