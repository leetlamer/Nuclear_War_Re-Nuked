#include "nuclear/engine.hpp"
namespace nuclear {
// Plan in the established actor order and publish only after the full pass succeeds.
void ai_turn(State &state, Rng &random) {
    auto planned_state = state;
    auto planned_random = random;
    for (std::size_t actor_index = 0; actor_index < 4; ++actor_index) {
        if (!planned_state.participating[actor_index])
            continue;
        Word decision_row =
            add_word(select_opponent(planned_state, actor_index, planned_random), 2);
        auto &actor = planned_state.actors[actor_index];
        bool defence_planned = false;
        if (actor.defenses[0] != 0 || actor.defenses[1] != 0)
            defence_planned = plan_defense(planned_state, actor_index);
        if (!defence_planned) {
            Word action_category = (actor.action == 4 || actor.action == 5 || actor.action == 2)
                                       ? 2
                                       : ai_category(decision_row, actor.profile, planned_random);
            if (planned_state.identity[actor_index] == 9)
                action_category = 1;
            auto target_index = std::size_t(planned_state.selected_opponent[actor_index]);
            if (action_category == 1)
                plan_propaganda(planned_state, actor_index, target_index, planned_random);
            else if (action_category == 2)
                plan_weapons(planned_state, actor_index, target_index, planned_random);
            else if (action_category == 3)
                actor.action = 6;
        }
    }
    state = planned_state;
    random = planned_random;
}
} // namespace nuclear
