# Three-feature restoration investigation

## Roster — VERIFIED source; deliberate modern behaviour change
`src/ui/player_controller.cpp::complete_selection` fills vacant selections with random identities until four, then assigns them cyclically after RNG draws. Banner/header confirms selection; fourth portrait does not confirm. `initialize_state` initializes five actors. Modern requirement removes auto-fill and retains exactly 1–4 explicitly selected opponents. Four-selected assignment and original gameplay remain unchanged.

## Extinction — VERIFIED missing presentation
`EndgamePresentation::begin` records winner -1 and returns; `hall()` becomes true at 0.8 seconds. Renderer immediately uses space background. Original ending 11437C loads spacemap, expmap, smallexp, bigexp. Existing audit explicitly leaves rectangles/timing unresolved. Detailed asset/control-flow recovery is in EXTINCTION_EVIDENCE.md. Endgame already waits for pending presentation and resolver/revenge completion.

## Alien attack — UNKNOWN pending recovery
Existing SPECIAL_EVENT_COMPLETE_CATALOGUE explicitly distinguishes action16 immigration (positive population) from unverified developer alien-laser preview. Action11 is city liftoff. Do not reassign either event or invent damage/probability. Detailed findings are in ALIEN_ATTACK_EVIDENCE.md.

GameReverse is read-only. Desktop changes precede browser synchronization.
