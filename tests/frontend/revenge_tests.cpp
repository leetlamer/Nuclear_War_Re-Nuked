#include "tests/support/session_fixtures.hpp"
#include "ui/animation_playback.hpp"
#include "ui/turn_presentation.hpp"
#include <iostream>
#include <set>
using namespace nuclear;
using namespace nuclear::player;
unsigned checks = 0, groups = 0;
void check(bool x, int l) {
    ++checks;
    if (!x)
        throw std::runtime_error("revenge failure line " + std::to_string(l));
}
#define C(x) check(bool(x), __LINE__)
struct Schedule : session::CallbackSchedule {
    void apply(session::CallbackPoint, session::Data &) override {}
};
session::Data base(Word victim = 1, unsigned survivors = 3, unsigned pairs = 10, Word seed = 1) {
    session::Data d(seed, true);
    for (std::size_t i = 0; i < 5; ++i) {
        auto &a = d.state.actors[i];
        a.profile = 1;
        a.action = 88;
        a.city_x = {20, 70, 120, 170, 220};
        a.city_y = {30, 60, 90, 120, 150};
        d.names[i] = "Country" + std::to_string(i);
    }
    d.state.actors[0].population[0] = 1000;
    d.state.actors[victim].population[0] = 1;
    d.state.actors[victim].missiles[0] = Word(pairs);
    d.state.actors[victim].warheads[0] = Word(pairs);
    d.state.actors[0].action = 1;
    d.state.actors[0].target_actor = victim;
    d.state.actors[0].target_city = 0;
    d.state.actors[0].magnitude = 25;
    for (std::size_t i = 0; i < 5 && survivors > 1; ++i)
        if (i != victim && i != 0) {
            d.state.actors[i].population.fill(1000);
            --survivors;
        }
    return d;
}
void resolve(session::Data &d) {
    TurnResolverAdapter a;
    Schedule s;
    session::ResolverRequest r;
    r.microseconds_low16 = Word(d.rng.seed());
    a.resolve(d, r, s);
}
std::vector<session::ResolvedTurnEvent> attacks(const session::Data &d, std::size_t i) {
    std::vector<session::ResolvedTurnEvent> v;
    for (const auto &e : d.revenge.events)
        if (e.revenge && e.actor == i)
            v.push_back(e);
    return v;
}
void group(const char *n, void (*f)()) {
    f();
    ++groups;
    std::cout << "PASS " << n << '\n';
}
int main() {
    group("actual normal combat automatically triggers human and AI death", [] {
        for (Word v : {Word(1), Word(4)}) {
            auto d = base(v);
            C(populated_cities(d.state.actors[v]));
            resolve(d);
            C(!populated_cities(d.state.actors[v]));
            C(d.revenge.announced[v]);
            auto a = attacks(d, v);
            C(a.size() == 4);
            C(d.state.actors[v].missiles[0] == 6 && d.state.actors[v].warheads[0] == 6);
            C(d.revenge.events[0].actor == 0 && !d.revenge.events[0].revenge);
            C(d.revenge.events[1].death && d.revenge.events[1].actor == v);
            C(d.revenge.events[2].revenge);
        }
    });
    group("zero one three ten pairs and no fifth attack or inventory underflow", [] {
        for (unsigned stock : {0u, 1u, 3u, 10u}) {
            auto d = base(1, 3, stock);
            resolve(d);
            auto n = std::min(stock, 4u);
            C(attacks(d, 1).size() == n);
            C(d.revenge.attacks[1] == n);
            C(d.state.actors[1].missiles[0] == stock - n &&
              d.state.actors[1].warheads[0] == stock - n);
            auto seed = d.rng.seed();
            RevengeQueue q(d);
            q.detect(d.state);
            q.drain();
            C(d.rng.seed() == seed && d.revenge.attacks[1] == n);
        }
    });
    group("missile and warhead shortages and incompatible pairs", [] {
        for (int mode = 0; mode < 5; ++mode) {
            auto d = base();
            auto &v = d.state.actors[1];
            v.missiles.fill(0);
            v.warheads.fill(0);
            if (mode == 0)
                v.missiles[0] = 5;
            if (mode == 1)
                v.warheads[0] = 5;
            if (mode == 2) {
                v.missiles[0] = 2;
                v.warheads[0] = 10;
            }
            if (mode == 3) {
                v.missiles[0] = 10;
                v.warheads[3] = 10;
            }
            if (mode == 4) {
                v.missiles[0] = 10;
                v.warheads[0] = 2;
            }
            resolve(d);
            C(attacks(d, 1).size() == (mode == 2 || mode == 4 ? 2 : 0));
            C(v.warheads[3] == (mode == 3 ? 10 : 0));
        }
    });
    group("multiple warhead classes use documented deterministic compatibility order", [] {
        auto d = base(1, 4, 0);
        auto &a = d.state.actors[1];
        a.missiles = {1, 1, 1, 1};
        a.warheads = {1, 1, 1, 1};
        resolve(d);
        auto list = attacks(d, 1);
        C(list.size() == 4);
        for (Word i = 0; i < 4; ++i) {
            C(list[i].magnitude == class_effect(i));
            C(a.missiles[i] == 0 && a.warheads[i] == 0);
        }
        auto e = base(1, 4, 0);
        e.state.actors[1].missiles[3] = 10;
        e.state.actors[1].warheads = {2, 2, 2, 2};
        resolve(e);
        auto mixed = attacks(e, 1);
        C(mixed.size() == 4);
        for (unsigned i = 0; i < 4; ++i)
            C(mixed[i].magnitude == class_effect(Word(i / 2)));
        C(e.state.actors[1].missiles[3] == 6 && e.state.actors[1].warheads[2] == 2 &&
          e.state.actors[1].warheads[3] == 2);
    });
    group("rotation across four three two one survivors; random positive cities; deterministic",
          [] {
              for (unsigned survivors = 1; survivors <= 4; ++survivors) {
                  auto d = base(1, survivors);
                  auto twin = d;
                  resolve(d);
                  resolve(twin);
                  auto a = attacks(d, 1), b = attacks(twin, 1);
                  C(a.size() == 4 && b.size() == 4 && same_state(d.state, twin.state) &&
                    d.rng.seed() == twin.rng.seed());
                  std::set<Word> cycle;
                  for (unsigned k = 0; k < 4; ++k) {
                      if (cycle.size() == survivors)
                          cycle.clear();
                      C(!cycle.count(a[k].target));
                      cycle.insert(a[k].target);
                      C(a[k].target != 1 && populated_cities(a[k].before.actors[a[k].target]));
                      C(signed_word(a[k].before.actors[a[k].target].population[a[k].city]) > 0);
                      C(a[k].target == b[k].target && a[k].city == b[k].city);
                  }
              }
              std::set<Word> cities;
              for (Word seed = 1; seed < 50; ++seed) {
                  auto d = base(1, 4, 1, seed);
                  resolve(d);
                  auto a = attacks(d, 1);
                  if (!a.empty() && a[0].target != 0)
                      cities.insert(a[0].city);
              }
              C(cities.size() > 1);
          });
    group("grounded bomber stock is never airborne or consumed", [] {
        auto d = base(1, 3, 0);
        d.state.actors[1].bombers.fill(10);
        d.state.actors[1].warheads.fill(10);
        resolve(d);
        C(attacks(d, 1).empty());
        C(d.state.actors[1].bombers[0] == 10 && d.state.actors[1].warheads[0] == 10);
    });
    group("airborne first then missile with no new aircraft", [] {
        auto d = base(1, 3, 1);
        d.revenge.airborne[1] = {true, 50, 0};
        d.state.actors[1].warheads[0] = 10;
        d.state.actors[1].bombers.fill(0);
        resolve(d);
        auto a = attacks(d, 1);
        C(a.size() >= 2);
        C(a[0].action == 5 && a[1].action == 1);
        C(d.state.actors[1].missiles[0] == 0);
        C(d.state.actors[1].bombers[0] == 0);
        for (const auto &e : a)
            C(e.action == 1 || e.action == 5);
        C(d.state.actors[1].warheads[0] == 10 - a.size());
    });
    group("two or four recorded flights precede missiles and each payload delivers once", [] {
        for (unsigned flights : {2u, 4u}) {
            auto d = base(1, 4, 10);
            d.revenge.airborne[1] = {true, 50, 0};
            for (unsigned i = 1; i < flights; ++i)
                d.revenge.additional_airborne[1].push_back({true, 50, 0});
            resolve(d);
            auto a = attacks(d, 1);
            C(a.size() == 4);
            for (unsigned i = 0; i < 4; ++i)
                C(a[i].action == (i < flights ? 5 : 1));
            C(d.state.actors[1].missiles[0] == 10 - (4 - flights));
            C(d.state.actors[1].warheads[0] == 6);
            C(!d.revenge.airborne[1].active);
            for (auto b : d.revenge.additional_airborne[1])
                C(!b.active);
        }
    });
    group("destroyed and delivered aircraft cannot produce revenge payloads", [] {
        auto d = base(1, 3, 1);
        d.revenge.airborne[1] = {false, 50, 5};
        d.revenge.additional_airborne[1].push_back({false, 50, 0});
        resolve(d);
        auto a = attacks(d, 1);
        C(a.size() == 1 && a[0].action == 1);
    });
    group("completed deployment is tracked but unexecuted deployment is not airborne", [] {
        auto d = base(4, 3, 0);
        d.state.actors[4].action = 4;
        d.state.human_prepared_capacity = 50;
        d.state.actors[4].warheads[0] = 3;
        resolve(d);
        C(attacks(d, 4).empty());
        auto e = base(1, 3, 0);
        e.state.actors[0].population[0] = 1;
        e.state.actors[0].action = 4;
        e.state.prepared_capacity[0] = 50;
        e.state.actors[0].warheads[0] = 3;
        e.state.actors[1].population[0] = 1000;
        e.state.actors[1].action = 1;
        e.state.actors[1].target_actor = 0;
        e.state.actors[1].target_city = 0;
        e.state.actors[1].magnitude = 25;
        resolve(e);
        C(!populated_cities(e.state.actors[0]));
        C(!attacks(e, 0).empty());
        for (const auto &a : attacks(e, 0))
            C(a.action == 5);
        C(e.state.actors[0].bombers[0] == 0);
    });
    group("committed airborne payload is delivered once without charging another warhead", [] {
        auto d = base(1, 3, 0);
        d.state.actors[1].action = 5;
        d.state.actors[1].magnitude = 5;
        d.state.prepared_capacity[1] = 0;
        resolve(d);
        auto a = attacks(d, 1);
        C(a.size() == 1 && a[0].action == 5 && a[0].magnitude == 5);
        C(d.state.actors[1].warheads[0] == 0);
        C(!d.revenge.airborne[1].reserved_payload);
    });
    group("a normal delivered bomber is excluded if its owner dies later in the same pass", [] {
        auto d = base(1, 3, 0);
        auto &a = d.state.actors[0];
        a.action = 5;
        a.target_actor = 2;
        a.target_city = 0;
        a.magnitude = 2;
        a.population.fill(0);
        a.population[0] = 1;
        a.warheads[0] = 5;
        d.state.prepared_capacity[0] = 50;
        auto &killer = d.state.actors[1];
        killer.population[0] = 1000;
        killer.action = 1;
        killer.target_actor = 0;
        killer.target_city = 0;
        killer.magnitude = 25;
        resolve(d);
        C(!populated_cities(d.state.actors[0]));
        C(!d.revenge.airborne[0].active);
        C(attacks(d, 0).empty());
    });
    group("FIFO chain death separate allowance eligibility and termination", [] {
        bool found = false;
        for (Word seed = 1; seed < 100 && !found; ++seed) {
            auto d = base(1, 3, 10, seed);
            d.state.actors[2].population.fill(0);
            d.state.actors[2].population[0] = 1;
            d.state.actors[2].missiles[0] = 10;
            d.state.actors[2].warheads[0] = 10;
            resolve(d);
            if (!d.revenge.announced[2])
                continue;
            found = true;
            auto a = attacks(d, 1), b = attacks(d, 2);
            C(a.size() == 4 && b.size() == 4);
            bool seenB = false;
            unsigned deaths = 0;
            for (const auto &e : d.revenge.events) {
                if (e.death)
                    ++deaths;
                if (e.revenge) {
                    C(e.before.actors[e.target].population[e.city] > 0);
                    if (e.actor == 2)
                        seenB = true;
                    if (e.actor == 1)
                        C(!seenB);
                }
            }
            C(deaths == 2 && d.revenge.attacks[1] == 4 && d.revenge.attacks[2] == 4);
            C(d.revenge.events.size() <= 26);
        }
        C(found);
    });
    group("normal defence interception and no surviving city terminate safely", [] {
        auto d = base(1, 1);
        d.state.actors[0].action = 7;
        d.state.actors[0].magnitude = 1;
        auto before = d.state;
        RevengeQueue q(d);
        d.state.actors[1].population.fill(0);
        q.detect(before);
        q.drain();
        C(attacks(d, 1).size() == 4);
        for (const auto &e : attacks(d, 1))
            C(e.effect == 13 && e.loss == 0);
        C(d.state.actors[0].population[0] == 1000);
        auto z = base(1, 1);
        auto b = z.state;
        z.state.actors[0].population.fill(0);
        z.state.actors[1].population.fill(0);
        RevengeQueue none(z);
        none.detect(b);
        auto seed = z.rng.seed();
        none.drain();
        C(z.rng.seed() == seed && z.revenge.attacks[1] == 0);
    });
    group("death types fully before automatic next; three playback modes preserve state", [] {
        auto d = base();
        resolve(d);
        auto state = d.state;
        auto seed = d.rng.seed();
        std::vector<std::size_t> expected;
        for (std::size_t i = 0; i < d.revenge.events.size(); ++i)
            expected.push_back(i);
        for (int mode = 0; mode < 3; ++mode) {
            auto s = ActionPresentation::resolved_fixture(d);
            std::set<std::size_t> seen;
            bool noticeFinished = false;
            unsigned resultFrames = 0;
            for (unsigned tick = 0; tick < 10000 && s.active(); ++tick) {
                seen.insert(s.index());
                if (s.event().system_notice) {
                    if (s.text_complete())
                        noticeFinished = true;
                    C(!s.input_eligible());
                }
                if (s.event().auto_continue && s.event().media.supported && s.index() > 1)
                    C(noticeFinished);
                if (mode == 2 && s.playback_eligible())
                    s.instant_complete(tick * 500 + 1000);
                s.playback_tick(mode == 1 ? .15 : .1, mode == 1);
                if (s.active() && s.instant_result())
                    ++resultFrames;
                s.presented();
            }
            C(!s.active() && seen.size() == expected.size());
            if (mode == 2)
                C(resultFrames > 4);
            C(same_state(state, d.state) && seed == d.rng.seed());
        }
    });
    group("real Player coordinator resolution publishes human and AI death timelines", [] {
        for (bool human : {false, true}) {
            auto initial = endgame_fixture(human, 3);
            if (human) {
                initial.state.actors[4].missiles.fill(10);
                initial.state.actors[4].warheads.fill(10);
            }
            auto p = Player::developer_session(initial);
            ActionPresentation scene;
            scene.observe(p);
            bool actualDeath = false, shownDeath = false, shownRevenge = false;
            for (unsigned tick = 0; tick < 20000 && (!p.ended() || scene.active()); ++tick) {
                if (!scene.active()) {
                    if (p.coordinator()->phase() == session::Phase::human_selection) {
                        C(p.command({HumanCommandKind::production, 0}));
                        C(p.confirm_banner());
                    }
                    if (!p.ended())
                        C(p.advance_turn(Word(p.round() * 37 + 1)));
                    scene.observe(p);
                }
                if (scene.active()) {
                    if (scene.event().system_notice)
                        shownDeath = true;
                    if (scene.event().media.supported && scene.event().auto_continue &&
                        (scene.event().action == 1 || scene.event().action == 5) && shownDeath)
                        shownRevenge = true;
                    scene.tick(.1);
                    scene.presented();
                    if (scene.active() && !scene.event().auto_continue &&
                        scene.animation_complete())
                        scene.next();
                }
                for (const auto &e : p.coordinator()->events())
                    for (const auto &r : e.resolution_events)
                        if (r.death && (human ? r.actor == 4 : r.actor < 4))
                            actualDeath = true;
            }
            C(p.ended() && !scene.active());
            C(actualDeath && shownDeath && shownRevenge);
            if (human)
                C(p.coordinator()->data().revenge.attacks[4] > 0);
        }
    });
    group("target draw order and shared normal combat reproduce each committed missile", [] {
        auto d = base(1, 4);
        resolve(d);
        std::array<bool, 5> used{};
        for (const auto &e : attacks(d, 1)) {
            std::vector<Word> players;
            for (Word i = 0; i < 5; ++i)
                if (i != 1 && populated_cities(e.before.actors[i]) && !used[i])
                    players.push_back(i);
            if (players.empty()) {
                used.fill(false);
                for (Word i = 0; i < 5; ++i)
                    if (i != 1 && populated_cities(e.before.actors[i]))
                        players.push_back(i);
            }
            session::Data replay(e.seed_before, true);
            replay.state = e.before;
            C(e.target == players[replay.rng.next() % players.size()]);
            std::vector<Word> cities;
            for (Word i = 0; i < 5; ++i)
                if (signed_word(e.before.actors[e.target].population[i]) > 0)
                    cities.push_back(i);
            C(e.city == cities[replay.rng.next() % cities.size()]);
            used[e.target] = true;
            auto &a = replay.state.actors[1];
            --a.missiles[0];
            --a.warheads[0];
            a.action = 1;
            a.target_actor = e.target;
            a.target_city = e.city;
            a.magnitude = e.magnitude;
            auto roll = begin_attack_roll(1, replay.rng);
            auto r = e;
            r.effect = roll.effect;
            resolve_attack(replay, r, e.magnitude == 25);
            C(same_state(replay.state, e.after) && replay.rng.seed() == e.seed_after &&
              r.loss == e.loss);
        }
    });
    group("coordinator rollback also rolls back revenge ledger and consumption", [] {
        struct Failing : TurnResolverAdapter {
            Word setup(session::Data &, session::CallbackSchedule &) override { return 0; }
            Word resolve(session::Data &d, const session::ResolverRequest &req,
                         session::CallbackSchedule &cb) override {
                auto ready = base(1, 3);
                d.state = ready.state;
                TurnResolverAdapter::resolve(d, req, cb);
                C(d.revenge.attacks[1] == 4);
                throw Unsupported("fixture after revenge");
            }
        } boundary;
        Schedule schedule;
        session::Adapters adapters{boundary, schedule};
        session::Coordinator c(endgame_fixture(false, 3));
        for (unsigned n = 0; n < 10 && c.phase() != session::Phase::resolver_handoff; ++n)
            C(c.advance(adapters));
        C(c.phase() == session::Phase::resolver_handoff);
        auto before = c.data();
        C(!c.advance(adapters, Word(1)));
        C(same_state(before.state, c.data().state) && before.rng.seed() == c.data().rng.seed());
        C(c.data().revenge.events.empty());
        for (unsigned i = 0; i < 5; ++i)
            C(!c.data().revenge.announced[i] && c.data().revenge.attacks[i] == 0);
        C(c.events().back().resolution_events.empty());
    });
    group("new session setup clears death counters aircraft and event history", [] {
        auto d = base();
        resolve(d);
        C(d.revenge.announced[1] && !d.revenge.events.empty());
        d.revenge.airborne[1] = {true, 50, 5};
        TurnResolverAdapter a;
        Schedule schedule;
        a.setup(d, schedule);
        C(d.revenge.events.empty() && !d.revenge.contains_death);
        for (unsigned i = 0; i < 5; ++i)
            C(!d.revenge.announced[i] && d.revenge.attacks[i] == 0 &&
              !d.revenge.airborne[i].active && !d.revenge.airborne[i].reserved_payload);
    });
    group("recorded ordinary propaganda retains its original result and interaction policy", [] {
        session::Data d(1, true);
        d.names[0] = "A";
        d.names[1] = "B";
        session::ResolvedTurnEvent r;
        r.actor = 0;
        r.action = 3;
        r.variant = 1;
        r.effect = 13;
        r.loss = 3;
        r.target = 1;
        d.revenge.events.push_back(r);
        r.action = 1;
        d.revenge.events.push_back(r);
        r.death = true;
        r.actor = 1;
        d.revenge.events.push_back(r);
        auto s = ActionPresentation::resolved_fixture(d);
        C(s.events()[0].outcome ==
          nuclear::text::format(0x10538e, {std::int32_t(3), std::string("B"), std::string("A")}));
        C(!s.events()[0].auto_continue && s.events()[1].auto_continue);
    });
    std::cout << groups << " revenge groups, " << checks << " checks passed\n";
}
