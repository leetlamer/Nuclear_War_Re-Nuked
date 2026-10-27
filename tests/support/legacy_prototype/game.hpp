#pragma once
#include "nuclear/engine.hpp"
#include <string>
#include <vector>

namespace prototype {
using nuclear::Word;
enum class CommandKind { missile, bomber, propaganda, production, defense };
struct Command {
    CommandKind kind{CommandKind::production};
    std::size_t target{}, city{}, delivery{}, warhead{};
};
enum class Outcome { ongoing, victory, defeat, draw, round_limit };
struct Game {
    nuclear::State state{};
    nuclear::Rng rng;
    std::uint32_t initial_seed;
    unsigned round{}, max_rounds;
    Outcome outcome{Outcome::ongoing};
    std::vector<std::string> log;
    explicit Game(std::uint32_t seed, unsigned limit = 200);
    bool alive(std::size_t actor) const;
    std::string validate(const Command &) const;
    // Rejected commands leave state, RNG, round and log unchanged.
    bool play(const Command &, std::string &error);
    Command automatic_command() const;
    void finish_if_needed();
    // Prototype-only immediate resolver; also usable by headless simulations.
    // Does not plan actions, advance round, or infer an Amiga frame gate.
    void resolve_immediate(std::size_t actor);

  private:
    void human_command(const Command &);
    void plan_ai();
    void production(std::size_t);
};
const char *leader(std::size_t);
const char *outcome_name(Outcome);
std::string action_name(Word);
std::string state_digest(const Game &);
} // namespace prototype
