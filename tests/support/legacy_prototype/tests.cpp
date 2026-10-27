#include "tests/support/legacy_prototype/game.hpp"
#include <iostream>
#include <stdexcept>
#include <string>
using namespace prototype;
using namespace nuclear;
namespace {
unsigned checks = 0, groups = 0;
void check(bool value) {
    ++checks;
    if (!value)
        throw std::runtime_error("prototype assertion " + std::to_string(checks));
}
void equal_state(const State &a, const State &b) {
    for (unsigned i = 0; i < 5; ++i)
        check(encode_actor(a.actors[i]) == encode_actor(b.actors[i]));
    check(a.prepared_capacity == b.prepared_capacity);
    check(a.human_prepared_capacity == b.human_prepared_capacity);
}
void fixture(Game &g) {
    for (auto &a : g.state.actors) {
        a.profile = 0;
        a.population.fill(100);
        a.action = 0;
        a.magnitude = 0;
    }
    g.rng = Rng(1);
}
} // namespace
int main() {
    try {
        {
            Game g(1);
            const auto before = state_digest(g);
            std::string error;
            for (Command c : {Command{CommandKind::missile, 9, 0, 0, 0},
                              Command{CommandKind::missile, 0, 9, 0, 0},
                              Command{CommandKind::missile, 0, 0, 9, 0},
                              Command{CommandKind::missile, 0, 0, 0, 9},
                              Command{CommandKind::defense, 0, 0, 9, 0},
                              Command{static_cast<CommandKind>(99)}}) {
                check(!g.play(c, error));
                check(!error.empty());
                check(state_digest(g) == before);
                check(g.log.empty());
            }
            g.state.actors[0].population.fill(0);
            check(!g.validate({CommandKind::propaganda, 0, 0}).empty());
            g.state.actors[4].warheads[3] = 1;
            check(!g.validate({CommandKind::missile, 1, 0, 0, 3}).empty());
            ++groups;
        }
        {
            for (Word action : {Word(1), Word(5)})
                for (Word defense : {Word(0), Word(1), Word(2)}) {
                    Game g(5);
                    fixture(g);
                    auto &a = g.state.actors[4];
                    a.action = action;
                    a.magnitude = 10;
                    a.target_actor = 0;
                    a.target_city = 2;
                    if (defense < 2) {
                        g.state.actors[0].action = 7;
                        g.state.actors[0].magnitude = defense;
                    }
                    State expected = g.state;
                    Rng oracle(1);
                    auto roll = begin_attack_roll(action, oracle);
                    auto impact =
                        evaluate_combat_impact(expected, 4, 0, 2, roll.effect, false, oracle);
                    (void)combat_population_commit(expected, 4, 0, 2);
                    g.resolve_immediate(4);
                    equal_state(g.state, expected);
                    check(g.rng.seed() == oracle.seed());
                    if (defense == 1 || (defense == 0 && action == 1)) {
                        check(impact.effect == 13);
                        check(g.state.actors[0].population[2] == 100);
                        check(g.log.back().find("intercepted") != std::string::npos);
                    }
                }
            ++groups;
        }
        {
            // Every action3 substitution route, compared to verified immediate slices.
            for (Word wanted : {Word(3), Word(8), Word(9), Word(11), Word(12), Word(13), Word(14),
                                Word(15), Word(16), Word(86)}) {
                unsigned seed = 0;
                for (unsigned candidate = 1; candidate < 100000; ++candidate) {
                    Rng r(candidate);
                    if (decide_action3_substitution(3, r).selected_action == wanted) {
                        seed = candidate;
                        break;
                    }
                }
                check(seed != 0);
                Game g(seed);
                fixture(g);
                g.rng = Rng(seed);
                auto &a = g.state.actors[4];
                a.action = 3;
                a.target_actor = 0;
                a.target_city = 1;
                a.magnitude = 7;
                State expected = g.state;
                Rng oracle(seed);
                (void)apply_action3_prelude(expected, 4, oracle);
                check(expected.actors[4].action == wanted);
                const auto event = initialize_event_handler(expected.actors[4], oracle);
                apply_special_event_effect(expected, 4, event.effect, oracle);
                g.resolve_immediate(4);
                equal_state(g.state, expected);
                check(g.rng.seed() == oracle.seed());
                check(g.log.size() == 1 && g.log[0].find("cancelled") == std::string::npos);
            }
            ++groups;
        }
        {
            Game g(1);
            fixture(g);
            g.state.actors[4].action = 6;
            State scratch;
            scratch.actors[0] = g.state.actors[4];
            Rng oracle(g.rng.seed());
            State init = g.state;
            (void)initialize_preparation_handler(init, 4, oracle);
            (void)produce(scratch, 0, oracle);
            g.resolve_immediate(4);
            check(g.state.actors[4].missiles == scratch.actors[0].missiles);
            check(g.state.actors[4].warheads == scratch.actors[0].warheads);
            check(g.rng.seed() == oracle.seed());
            Game h(1);
            std::string error;
            h.state.actors[4].warheads = {2, 0, 0, 0};
            h.state.actors[4].missiles = {2, 0, 0, 0};
            check(h.play({CommandKind::missile, 0, 0, 0, 0}, error));
            check(h.state.actors[4].missiles[0] == 1 && h.state.actors[4].warheads[0] == 1);
            Game b(1);
            b.state.actors[4].bombers[0] = 1;
            b.state.actors[4].warheads[0] = 1;
            check(b.play({CommandKind::bomber, 0, 0, 0, 0}, error));
            check(b.state.actors[4].bombers[0] == 0);
            Game d(1);
            check(d.play({CommandKind::defense, 0, 0, 0, 0}, error));
            check(d.state.actors[4].defenses[0] == 0);
            ++groups;
        }
        {
            Game g(1);
            fixture(g);
            g.state.actors[4].action = 3;
            g.state.actors[4].target_actor = 0xffff;
            g.state.actors[4].target_city = 0xffff;
            g.state.actors[0].population.fill(0);
            g.resolve_immediate(4);
            check(g.log.size() == 1 && g.log[0].find("cancelled") == std::string::npos);
            Game failed(1);
            fixture(failed);
            failed.state.actors[4].action = 99;
            const auto seed = failed.rng.seed();
            failed.resolve_immediate(4);
            check(failed.rng.seed() == seed && failed.state.actors[4].action == 0);
            check(failed.log[0].find("PROTOTYPE_APPROXIMATION") != std::string::npos);
            bool caught = false;
            try {
                failed.resolve_immediate(5);
            } catch (const std::invalid_argument &) {
                caught = true;
            }
            check(caught);
            ++groups;
        }
        {
            Game g(1);
            for (unsigned i = 0; i < 4; ++i)
                g.state.actors[i].population.fill(0);
            g.finish_if_needed();
            check(g.outcome == Outcome::victory);
            auto score = end_game_score(g.state.actors[4]);
            check(score.positive_cities == 5);
            std::string error;
            check(!g.play({}, error));
            Game d(1);
            d.state.actors[4].population.fill(0);
            d.finish_if_needed();
            check(d.outcome == Outcome::defeat);
            for (auto &a : d.state.actors)
                a.population.fill(0);
            d.finish_if_needed();
            check(d.outcome == Outcome::draw);
            Game limit(1, 1);
            check(limit.play({}, error));
            check(limit.outcome != Outcome::ongoing);
            check(Game(1).round == 0 && Game(1).outcome == Outcome::ongoing);
            bool caught = false;
            try {
                Game bad(1, 0);
            } catch (const std::invalid_argument &) {
                caught = true;
            }
            check(caught);
            ++groups;
        }
        {
            unsigned wins = 0, losses = 0, limits = 0, cancelled = 0, fallbacks = 0;
            for (unsigned seed = 0; seed < 128; ++seed) {
                Game g(seed), replay(seed);
                std::string error;
                while (g.outcome == Outcome::ongoing) {
                    const auto command = g.automatic_command();
                    check(g.validate(command).empty());
                    check(g.play(command, error));
                    check(replay.play(replay.automatic_command(), error));
                    check(state_digest(g) == state_digest(replay));
                    check(g.log == replay.log);
                    check(g.round <= g.max_rounds);
                    for (const auto &a : g.state.actors)
                        for (auto p : a.population)
                            check(signed_word(p) >= 0);
                    for (const auto &line : g.log) {
                        if (line.find("cancelled") != std::string::npos)
                            ++cancelled;
                        if (line.find("produces instead") != std::string::npos)
                            ++fallbacks;
                    }
                }
                check(g.outcome != Outcome::ongoing);
                wins += g.outcome == Outcome::victory;
                losses += g.outcome == Outcome::defeat;
                limits += g.outcome == Outcome::round_limit;
            }
            check(wins + losses + limits == 128);
            check(wins > 0 && losses > 0);
            check(limits == 0 && cancelled == 0);
            check(fallbacks ==
                  0); // bomber capacity is now initialized; no synthetic planner fallback
            std::cout << "128 deterministic seeds, each replayed: wins=" << wins
                      << " defeats=" << losses << " limits=" << limits << " cancelled=" << cancelled
                      << " planner_fallbacks=" << fallbacks << "\n";
            ++groups;
        }
        std::cout << groups << " groups, " << checks << " checks passed\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << "\n";
        return 1;
    }
}
