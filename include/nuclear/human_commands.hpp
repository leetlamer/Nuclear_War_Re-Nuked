#pragma once
#include "nuclear/engine.hpp"
namespace nuclear {
// Original106712 locals; textcontext remains explicit and persists across turns.
struct HumanCommandState {
    HumanSelection selection{};
    Word logical_view{4}, display_actor{4}, target_marker{0xffff};
    Word control_return{};
    bool active{};
};
struct HumanCommandStart { HumanCommandState command_state{}; HumanEntry entry{}; };
// Original caller invokes106712 only for a living human. Previous globals are
// explicit; this does not initialize a game, acquire a seed or resolve a round.
HumanCommandStart start_human_commands(State&, Word previous_selected_class,
    Word previous_selected_capacity, const std::array<std::string,5>& names,
    Word& welcome_flag, MessageSequenceRng&, Rng&);
enum class HumanCommandKind {
    confirm, propaganda, production, warhead, missile, bomber, defense,
    actor_view, city_target, classification, menu
};
struct HumanCommand { HumanCommandKind kind; Word argument{}; };
struct HumanCommandResult {
    bool turn_finished{};
    Word control_return{};
    std::uint32_t draws{};
    std::vector<std::string> messages;
};
// Normalized original commands; argument is original one-based inventory choice
// (0 auto), actor/city index, classification actor0..3, or raw menu code.
// No pixel mapping. Hidden propaganda target with no explicit proposed actor
// is coordinate-dependent and Unsupported. Every transition is transactional.
HumanCommandResult apply_human_command(State&, HumanCommandState&, HumanCommand,
    const std::array<std::string,5>& names, MessageSequenceRng&, Rng&);
} // namespace nuclear
