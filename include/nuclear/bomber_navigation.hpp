#pragma once
#include "nuclear/resolver_markers.hpp"
namespace nuclear {
//11640c..1164da: signedword comparisons;0=exactcontact,1..8=clockwise fromup.
Word bomber_heading_to(Word x, Word y, Word target_x, Word target_y) noexcept;
//110c26..110cb0: wrappingwordmovement;0/outside1..8 leavescoordinates alone.
// Original pointer-store order retained, including aliasedx/y references.
void step_heading_word(Word& x, Word& y, Word heading) noexcept;
// Exact transient word globals used by116740..11692c, not actor records.
struct BomberControlState {
    Word turn_count{}, rotation_step{}, predecessor_count{}, successor_count{};
    Word desired_heading{}, heading{}, speed_count{};
    Word x{}, y{}, target_x{}, target_y{}, return_flag{}, saved_heading{};
    Word output_marker{}, output_x{}, output_y{}, output_width{}, output_height{};
};
struct BomberControlTick {
    bool marker_written{}, outputs_written{}, emitted_arrival{}, moved{};
};
// Starts at116740, AFTER any framezero initialization. Actual launch-table
// words supplied explicitly. No renderer, RNG, actor writes or outerframegates.
BomberControlTick bomber_control_tick(BomberControlState&, Word frame,
    Word launch_x, Word launch_y) noexcept;
//116666..116740 numeric fields only; preserves globals not written there.
// Caller provides actual launch-tablewords, not assumed display coordinates.
void initialize_bomber_control_fields(BomberControlState&, const State&,
    std::size_t actor, Word launch_x, Word launch_y);
struct BomberActionStart {
    ResolverActionStart resolver{};
    BomberControlTick first_tick{};
    bool clear_seen_59d8{}; //116736 emitswrite0 to generic1325e8; caller applies
};
// Ordinary action5 framezero: verifiedRNG/prelude, numericfields, then tick0.
// No renderer/genericframegate/effectcommit. Transactional oninvalidtarget.
BomberActionStart start_bomber_action(State&, BomberControlState&, std::size_t actor,
    Word pass, Word launch_x, Word launch_y, Rng&);
} // namespace nuclear
