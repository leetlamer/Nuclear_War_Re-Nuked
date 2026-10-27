#include "nuclear/round_plan.hpp"
namespace nuclear {
RoundPlanResult prepare_round_plan(State &state, Word human_control, Rng &rng) {
    auto staged = state;
    auto staged_rng = rng;
    ai_turn(staged, staged_rng);
    update_attitudes(staged);
    state = staged;
    rng = staged_rng;
    return {human_control, human_control == 0};
}
} // namespace nuclear
