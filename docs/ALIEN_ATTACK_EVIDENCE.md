# UFO and four yellow-frame investigation

## Scope and conclusion

Read-only investigation of the recovered executable's existing instruction and
pseudocode exports, original text catalog, extracted graphics and current desktop
implementation. No original workspace files were changed. Static evidence is not
an observed Amiga runtime trace.

**VERIFIED:** the eight UFO frames and the four following yellow frames exist.
The original resolver connects those yellow frames to a *population gain*
announcement. The recovered normally dispatched alien event is immigration,
not a hostile population-loss attack. Restoring these original graphics to that
existing event is supported. Assigning hostile damage, an additional natural-event
probability, or invented dialogue is not supported by the recovered evidence.

**UNKNOWN:** an independently reachable hostile UFO city attack, its original
text, loss calculation, RNG consumption, and trigger. This is a remaining gap
against the requested hostile-event feature, not a claim that no other version
of Nuclear War contains such an event.

## Exact artwork

Modern equivalents of original `smalltiles.png`:

- `assets/images/interface/control_tiles.png`: original unmasked 320×272 atlas.
- `assets/images/effects/small_sprites.png`: same atlas with original one-bit mask.
- Source palette and mask provenance are in `assets/data/manifest.json`.

The grid is twenty 16-pixel cells per row. Native encoded 2000-series sprites use
this atlas; raw tile identity = encoded identity minus 2000.

| Native identity | Raw identity | Source x,y,width,height |
|---|---|---|
|2260|260|0,210,32,10|
|2262|262|32,210,32,10|
|2264|264|64,210,32,10|
|2266|266|96,210,32,10|
|2268|268|128,210,32,10|
|2270|270|160,210,32,10|
|2272|272|192,210,32,10|
|2274|274|224,210,32,10|
|276 (`0x114`)|276|256,208,16,16|
|277 (`0x115`)|277|272,208,16,16|
|278 (`0x116`)|278|288,208,16,16|
|279 (`0x117`)|279|304,208,16,16|

The eight UFO source rectangles have a two-pixel inset from row208. The yellow
frames use the full16×16 cells. Original masked transparency must be retained;
black is not a universal transparent colour. The atlas was actually inspected
visually during this investigation. No replacement artwork was generated.

## Verified native movement and frame cadence

Handler `0x117F48` (Amiga-aware relocated `0x236F48`), existing exports:

- `GameReverse/analysis/game_logic/00117f48.c`
- `GameReverse/analysis/second_pass/resolver/pseudocode/old_117f48.c`
- `GameReverse/analysis/game_logic/instructions.jsonl`

Frame zero consumes exactly three gameplay RNG draws:

1. `next()%2+1`, supplied as an extra unused formatting argument.
2. `next()%10+1`, provisional magnitude.
3. `next()%200+51`, initial UFO x.

Initial y is0. Destination x/y come from actor-base arrays at original
`0x131214` and `0x13121E`, not randomly selected enemy-city coordinates.
The craft raw frame begins260; a counter initialized4 advances it by2 every
four handler calls, wrapping at276 to260. A separate counter initialized2 moves
one native directional step every two handler calls. `0x11640C` computes the
8-way direction and `0x110C26` applies the one-pixel x/y step. At exact destination,
the craft x output is ORed with`0x400`, a presentation marker. Source size32×10.
Native per-call cadence is verified; its exact modern seconds conversion is not.

**UNKNOWN:** a native ascent out of the screen. This handler records downward
arrival and destination completion, not a separately recovered upward path.
A modern descend/hover/ascend choreography must be labelled inferred if used;
it must not be represented as instruction-verified original motion.

## Four yellow frames: original resolver connection

Native `0x104E36` compares the primary presentation marker with23000 (`0x59D8`).
On first entry, `0x104E82` loads text at`0x1059C3`:

> %s gains %d Million people.

This is not an alien attack/loss announcement. Instructions`0x104EA0–0x104EB2`
place the secondary sprite at craft x+12, craft y+3. The16×16 source graphic
contains its own visible shape; changing this to a bottom attachment is a host
presentation decision rather than the recovered native coordinate.

`0x104EE4`/`0x104EEA` initialize raw frame276;`0x104EF0` stores limit279.
`0x104EF6`/`0x104EFC` initialize cadence counters2. `0x104F02–0x104F20` set
16×16 dimensions. The source frames therefore run276,277,278,279 in order,
with a native counter2 cadence in the shared sprite animator. At frame799,
`0x104F6E–0x104FAC` removes the secondary image and restores saved frame state.
The exact wall-clock interval and repeat count were not established here.

This supplements older notes that described the four-frame visual connection as
unknown. It does **not** overturn their verified positive-population mechanic.

## Dialogue and authoritative population effect

`0x11817A`, directly referenced by alien handler `0x117F48`:

> Space Aliens land on %s.

Effect69 (`0x45`) in resolver `0x104296–0x10433C`:

1. Set result action22.
2. Scan the acting country's five cities using signed-positive population.
3. Keep the first strictly-largest city (ties retain first).
4. Store its old population P in magnitude.
5. Store `Word(P+P)` in that city using original16-bit wrapping arithmetic.

No effect-stage RNG draw. The provisional random1..10 is overwritten. The
primary effect result text at`0x105673` is:

> Population increases by %d million!

Current desktop `src/game/combat/turn_resolver.cpp`, effect`0x45`, already calls
`double_largest_city` and preserves this authoritative behaviour.

Human recap at`0x108F58` and following strings refers to alien immigrants.
The catalogue also contains AI reaction `Are aliens illegal?` at`0x10FD37`;
it is speech dialogue, not proof of an attack event.

## Natural trigger and sound

Normal action3 substitution at`0x102C18–0x102F88`:

- First draw `next()%100+1`; substitution only when less than30.
- Second draw `next()%100+1`; bins7–8 select action16.
- Other event bins remain unchanged.

The complete handler subtract/branch chain `0x1035AA–0x10360E` skips action10:
`0x1035E4` subtracts2 after the action9 branch before the action11 branch.
No recovered ordinary producer for hostile action10 was established.

Original action16 audio at`0x103414–0x10342E`: `prop`, period300, length5399,
volume20, repeat cycles0 (native continuous). At default audio clock3546895,
period300 corresponds to11822.9833samples/sec. No distinct original hostile
laser sound was verified. Preserve original payload and existing audio semantics;
do not invent an additional sample or change global audio clock.

## Search boundary and limitations

Reviewed normal dispatcher, effect69, handler117F48, marker23000 presentation,
full original text catalog's alien/space-related entries, current engine mapping,
and actual original extracted smalltiles artwork. Existing exhaustive catalogue:
`GameReverse/analysis/SPECIAL_EVENT_COMPLETE_CATALOGUE.md`, especially E7 and
unknowns. Its normal-domain conclusion is supported by the reviewed instructions.

No new Ghidra project, executable import, emulator runtime or original-video UFO
occurrence was used. No actual alien animation video/audio comparison is claimed.
Do not report hostile damage or a new event probability as recovered fact.
