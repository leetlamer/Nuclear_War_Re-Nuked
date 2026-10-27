#include "nuclear/resolver_core.hpp"
#include <cstdlib>
#include <functional>
#include <iostream>
using namespace nuclear;
using namespace nuclear::resolver;
namespace {
unsigned checks = 0, groups = 0;
void check(bool v, int line) {
    ++checks;
    if (!v) {
        std::cerr << "failure " << line << '\n';
        std::exit(1);
    }
}
#define CHECK(x) check(bool(x), __LINE__)
void group(const char *n, const std::function<void()> &f) {
    f();
    ++groups;
    std::cout << "PASS " << n << '\n';
}
void unsupported(const std::function<void()> &f) {
    bool caught = false;
    try {
        f();
    } catch (const Unsupported &) {
        caught = true;
    }
    CHECK(caught);
}
State fixture() {
    State s;
    for (auto &a : s.actors) {
        a.population = {20, 10, 0, 0, 0};
        a.profile = 1;
    }
    return s;
}
void same(const State &a, const State &b) {
    for (std::size_t i = 0; i < 5; ++i)
        CHECK(encode_actor(a.actors[i]) == encode_actor(b.actors[i]));
    CHECK(a.prepared_capacity == b.prepared_capacity);
    CHECK(a.selected_opponent == b.selected_opponent);
    CHECK(a.identity == b.identity);
    CHECK(a.remaining_total == b.remaining_total);
    CHECK(a.pass_processed == b.pass_processed);
    CHECK(a.human_prepared_capacity == b.human_prepared_capacity);
}
// Explicit zero-callback TEST fixture, never a production/native default.
struct Schedule : session::CallbackSchedule {
    void apply(session::CallbackPoint, session::Data &) override {}
};
struct ScriptBody : Body {
    std::function<Word(session::Data &, const session::ResolverRequest &,
                       session::CallbackSchedule &)>
        fn;
    Word execute(session::Data &d, const session::ResolverRequest &r,
                 session::CallbackSchedule &cb) override {
        return fn(d, r, cb);
    }
};
struct SetupBoundary : Boundary {
    explicit SetupBoundary(Body &b) : Boundary(b) {}
    Word setup(session::Data &, session::CallbackSchedule &) override { return 0; }
};
session::Data data_fixture() {
    session::Data d(1, true);
    d.state = fixture();
    d.names = {"A", "B", "C", "D", "E"};
    return d;
}
void handoff(session::Coordinator &c, session::Adapters &a) {
    CHECK(c.advance(a));
    CHECK(c.advance(a));
    CHECK(c.phase() == session::Phase::human_selection);
    CHECK(c.select({HumanCommandKind::production, 0}, a));
    CHECK(c.request_banner_confirmation());
    CHECK(c.advance(a));
    CHECK(c.advance(a));
    CHECK(c.advance(a));
    CHECK(c.advance(a));
    CHECK(c.phase() == session::Phase::resolver_handoff);
}
} // namespace
int main() {
    group("marker skip does not inspect invalid unused actor/target or draw", [] {
        auto s = fixture();
        auto before = s;
        Rng rng(1);
        for (auto coordinate : {Word(0), Word(0x400)})
            for (auto applied : {Word(1), Word(0xffff)}) {
                auto r = effect_marker(s, 99, 0xffff, coordinate, applied, {99, 99, 8, 7}, rng);
                CHECK(!r.dispatched && r.applied == applied && r.context.magnitude == 8 &&
                      r.context.suppression == 7);
                CHECK(r.coordinate == 0 && rng.seed() == 1);
                same(s, before);
            }
    });
    group("propaganda uses source latches, preserves current target and local latches", [] {
        auto s = fixture();
        auto &a = s.actors[0];
        a.target_actor = 2;
        a.target_city = 1;
        a.magnitude = 12;
        Rng rng(1);
        auto r = effect_marker(s, 0, 0x0d, 0x400, 0, {1, 1, 999, 9}, rng);
        CHECK(s.actors[1].population[1] == 0 && s.actors[2].population[1] == 10);
        CHECK(a.population[0] == 30 && a.magnitude == 10 && a.target_actor == 2 &&
              a.target_city == 1);
        CHECK(r.context.target_actor == 1 && r.context.target_city == 1 &&
              r.context.magnitude == 10);
        CHECK(r.context.suppression == 0 && r.dispatched && r.applied == 1 && r.coordinate == 0 &&
              rng.seed() == 1);
    });
    group("human propaganda forces profile2 then clears profile, no extra draw", [] {
        auto s = fixture();
        s.actors[4].profile = 4;
        s.actors[4].magnitude = 3;
        s.actors[4].target_actor = 99;
        Rng rng(1);
        auto r = effect_marker(s, 4, 0x0d, 0x400, 0, {1, 0, 0, 1}, rng);
        CHECK(s.actors[4].profile == 0 && s.actors[4].population[1] == 13 &&
              s.actors[1].population[0] == 17);
        CHECK(s.actors[4].target_actor == 99 && r.chosen_city == 1 && rng.seed() == 1);
    });
    group("defection destination latches differ from actor targets; exact city draw", [] {
        auto s = fixture();
        auto &a = s.actors[0];
        a.profile = 3;
        a.target_actor = 2;
        a.target_city = 1;
        a.magnitude = 40;
        Rng rng(1);
        auto r = effect_marker(s, 0, 0x0f, 0x400, 0, {1, 0, 0, 1}, rng);
        // First LCG output16838, remainder3 -> fourth living-city visit -> city1.
        CHECK(r.chosen_city == 1 && a.population[1] == 0 && a.magnitude == 10);
        CHECK(s.actors[1].population[0] == 30 && s.actors[2].population[1] == 10);
        CHECK(a.target_actor == 2 && a.target_city == 1 && r.context.magnitude == 10 &&
              rng.seed() == 0x41c67ea6);
    });
    group("growth rewrites context to self, word overflow, effect once", [] {
        auto s = fixture();
        s.actors[0].target_city = 1;
        s.actors[0].magnitude = 0xfffe;
        Rng rng(1);
        auto r = effect_marker(s, 0, 0x10, 0x400, 0, {4, 4, 999, 1}, rng);
        CHECK(s.actors[0].population[1] == 8 && r.context.target_actor == 0 &&
              r.context.target_city == 1 && r.context.magnitude == 0xfffe);
        auto before = s;
        auto again = effect_marker(s, 0, 0x10, 0x400, r.applied, r.context, rng);
        CHECK(!again.dispatched && again.coordinate == 0 && rng.seed() == 1);
        same(s, before);
    });
    group("city removal and weight use current own city and one message draw", [] {
        for (auto e : {Word(0x11), Word(0x12)}) {
            auto s = fixture();
            s.actors[0].target_city = 1;
            s.actors[0].magnitude = 10;
            Rng rng(1);
            auto r = effect_marker(s, 0, e, 0x400, 0, {4, 4, 999, 1}, rng);
            CHECK(s.actors[0].population[1] == 0 && s.actors[0].action == e);
            CHECK(r.context.target_actor == 0 && r.context.target_city == 1 &&
                  r.context.magnitude == 10);
            CHECK(r.message_variant == 3 && rng.seed() == 0x41c67ea6);
        }
    });
    group("cow cap changes local loss but not actor magnitude; current targets", [] {
        auto s = fixture();
        auto &a = s.actors[0];
        a.target_actor = 2;
        a.target_city = 1;
        a.magnitude = 30;
        Rng rng(1);
        auto r = effect_marker(s, 0, 0x13, 0x400, 0, {1, 0, 999, 1}, rng);
        CHECK(a.magnitude == 30 && s.actors[2].population[1] == 0 &&
              s.actors[1].population[0] == 20);
        CHECK(r.context.target_actor == 2 && r.context.target_city == 1 &&
              r.context.magnitude == 10);
        CHECK(r.message_variant == 4 && rng.seed() == 0x41c67ea6);
    });
    group("arrival adds with ADD.W, radiation reloads total and targets", [] {
        auto s = fixture();
        auto &a = s.actors[0];
        a.target_actor = 2;
        a.target_city = 1;
        a.magnitude = 0xffff;
        Rng rng(1);
        auto arrival = effect_marker(s, 0, 0x14, 0x400, 0, {1, 0, 999, 1}, rng);
        CHECK(s.actors[2].population[1] == 9 && a.action == 0x14 &&
              arrival.context.target_actor == 2 && arrival.context.magnitude == 0xffff &&
              rng.seed() == 1);
        auto radiation = effect_marker(s, 0, 0x15, 0x400, 0, {1, 0, 999, 1}, rng);
        CHECK(a.population[0] == 16 && a.population[1] == 6 && a.magnitude == 8 &&
              a.action == 0x15);
        CHECK(radiation.context.target_actor == 2 && radiation.context.target_city == 1 &&
              radiation.context.magnitude == 8 && rng.seed() == 0x41c67ea6);
    });
    group("alien chooses local largest without writing actor target city", [] {
        auto s = fixture();
        auto &a = s.actors[0];
        a.population = {4, 30, 30, 1, 0};
        a.target_city = 4;
        Rng rng(1);
        auto r = effect_marker(s, 0, 0x45, 0x400, 0, {2, 2, 999, 1}, rng);
        CHECK(a.population[1] == 60 && a.magnitude == 30 && a.target_city == 4 &&
              r.chosen_city == 1);
        CHECK(r.context.target_actor == 0 && r.context.target_city == 1 &&
              r.context.magnitude == 30 && rng.seed() == 1);
    });
    group("quake notice preserves target latches and population", [] {
        auto s = fixture();
        s.actors[0].magnitude = 7;
        auto population = s.actors[0].population;
        Rng rng(1);
        auto r = effect_marker(s, 0, 0x56, 0x400, 0, {2, 3, 999, 1}, rng);
        CHECK(s.actors[0].population == population && s.actors[0].action == 0x56);
        CHECK(r.context.target_actor == 2 && r.context.target_city == 3 &&
              r.context.magnitude == 7 && r.message_variant == 3 && rng.seed() == 0x41c67ea6);
    });
    group("invalid reached effect targets and later relationship failures roll back", [] {
        for (auto e : {Word(0x0d), Word(0x0f), Word(0x13), Word(0x14), Word(0xffff)}) {
            auto s = fixture();
            s.actors[0].target_actor = 9;
            auto before = s;
            Rng rng(1);
            unsupported([&] { effect_marker(s, 0, e, 0x400, 0, {9, 9, 0, 1}, rng); });
            same(s, before);
            CHECK(rng.seed() == 1);
        }
        auto s = fixture();
        s.actors[0].magnitude = 1;
        s.actors[0].profile = 9;
        auto before = s;
        Rng rng(1);
        unsupported([&] { effect_marker(s, 0, 0x0d, 0x400, 0, {1, 0, 0, 1}, rng); });
        same(s, before);
        CHECK(rng.seed() == 1);
    });
    group("human vehicle/warhead consumed at verified commit, never twice in resolver", [] {
        for (auto attack : {Word(1), Word(5)}) {
            auto s = fixture();
            auto &a = s.actors[4];
            a.target_actor = 1;
            a.target_city = 0;
            a.missiles[1] = 2;
            a.bombers[0] = 2;
            a.warheads[1] = 2;
            human_commit(s, attack == 1 ? Word(2) : Word(4), attack == 1 ? Word(1) : Word(0), 20);
            CHECK(a.missiles[1] == Word(attack == 1 ? 1 : 2) &&
                  a.bombers[0] == Word(attack == 5 ? 1 : 2));
            human_commit(s, attack, 1, 20);
            CHECK(a.warheads[1] == 1);
            auto missiles = a.missiles, warheads = a.warheads;
            auto bombers = a.bombers;
            Rng rng(1);
            auto start = start_resolver_action(s, 4, 0, rng);
            CHECK(start.initialized);
            CHECK(a.missiles == missiles && a.warheads == warheads && a.bombers == bombers);
            ImpactState local;
            local.context = {1, 0, 0, 0};
            local.selector = start.handler.effect;
            auto impact = primary_impact(s, 4, 0x5208, local, rng);
            (void)pending_commit(s, 4, impact.local, 0, true);
            CHECK(a.missiles == missiles && a.warheads == warheads && a.bombers == bombers);
        }
    });
    group("commit signed cap reloads current magnitude into local f6", [] {
        for (auto pop : {Word(5), Word(0xffff)}) {
            auto s = fixture();
            s.actors[1].population[0] = pop;
            s.actors[0].magnitude = 10;
            ImpactState local;
            local.context = {1, 0, 99, 1};
            local.pending = 15;
            auto r = pending_commit(s, 0, local, 0, true);
            CHECK(r.commit.loss == pop && r.local.context.magnitude == pop &&
                  s.actors[0].magnitude == pop);
            CHECK(s.actors[1].population[0] == 0 && r.local.pending == 0);
        }
    });
    group("ordinary missile impact one draw; pending separate; repeat once gate", [] {
        auto s = fixture();
        auto &a = s.actors[0];
        a.action = 1;
        a.magnitude = 5;
        a.target_actor = 4;
        a.target_city = 4;
        a.missiles[0] = 1;
        a.warheads[0] = 1;
        Rng rng(1);
        ImpactState local;
        local.context = {1, 0, 999, 7};
        local.selector = 6;
        auto impact = primary_impact(s, 0, 0x5208, local, rng);
        CHECK(impact.combat && impact.local.completion == 1 &&
              impact.local.primary_processed == 1 && impact.local.pending == 15);
        CHECK(a.magnitude == 8 && s.actors[1].population[0] == 20 &&
              impact.local.context.magnitude == 8 && rng.seed() == 0x41c67ea6);
        CHECK(impact.local.context.suppression == 0);
        CHECK(a.missiles[0] == 1 &&
              a.warheads[0] == 1); // previously consumed by commitment, not again at impact
        auto again = primary_impact(s, 0, 0x5208, impact.local, rng);
        CHECK(!again.combat && again.local.completion == 0 && rng.seed() == 0x41c67ea6);
        auto blocked = pending_commit(s, 0, impact.local, 195, false);
        CHECK(!blocked.commit.committed && blocked.local.pending == 15);
        a.magnitude = 7; // explicit supplied change before established gate: current magnitude wins
        auto commit = pending_commit(s, 0, impact.local, 196, false);
        CHECK(commit.commit.committed && commit.commit.loss == 7 &&
              commit.local.context.magnitude == 7 && commit.local.pending == 0);
        CHECK(s.actors[1].population[0] == 13 && s.actors[4].population[4] == 0 &&
              rng.seed() == 0x41c67ea6);
        auto before = s;
        auto duplicate = pending_commit(s, 0, commit.local, 196, true);
        CHECK(!duplicate.commit.committed);
        same(s, before);
    });
    group("defense override consumes impact draw and no second defense inventory", [] {
        for (auto action : {Word(1), Word(5)})
            for (auto defense : {Word(0), Word(1)}) {
                auto s = fixture();
                s.actors[0].action = action;
                s.actors[0].magnitude = 5;
                s.actors[1].action = 7;
                s.actors[1].magnitude = defense;
                s.actors[1].defenses = {3, 4};
                Rng rng(1);
                ImpactState local;
                local.context = {1, 0, 0, 1};
                local.selector = 6;
                auto r = primary_impact(s, 0, 0x5208, local, rng);
                bool intercepted = defense == 1 || action == 1;
                CHECK(r.local.selector == Word(intercepted ? 13 : 6) &&
                      s.actors[0].magnitude == Word(intercepted ? 0 : 8));
                CHECK(s.actors[0].action == Word(intercepted && action == 5 ? 0 : action));
                CHECK((s.actors[1].defenses == std::array<Word, 2>{{3, 4}}) &&
                      rng.seed() == 0x41c67ea6);
            }
    });
    group("all combat effects preserve caps and exact one-draw boundary", [] {
        for (Word effect = 2; effect <= 13; ++effect) {
            auto s = fixture();
            s.actors[0].action = 5;
            s.actors[0].magnitude = 5;
            Rng rng(1);
            ImpactState local;
            local.context = {1, 0, 0, 1};
            local.selector = effect;
            auto r = primary_impact(s, 0, 0x5208, local, rng);
            constexpr Word losses[]{0, 0, 6, 9, 8, 8, 10, 13, 18, 16, 20, 0};
            CHECK(r.local.context.magnitude == losses[effect - 2] && rng.seed() == 0x41c67ea6);
            auto commit = pending_commit(s, 0, r.local, 0, true);
            CHECK(commit.commit.committed &&
                  s.actors[1].population[0] == Word(20 - losses[effect - 2]));
        }
    });
    group("pre-substitution magnitude25 snapshot is not recomputed at impact", [] {
        for (bool snapshot : {false, true}) {
            auto s = fixture();
            s.actors[0].action = 1;
            s.actors[0].magnitude = 1;
            Rng rng(1);
            ImpactState local;
            local.context = {1, 0, 999, 1};
            local.selector = 12;
            local.magnitude_was_25 = snapshot;
            auto r = primary_impact(s, 0, 0x5208, local, rng);
            CHECK(r.local.context.magnitude == Word(snapshot ? 20 : 12));
            CHECK(s.actors[1].population[0] == 20 && rng.seed() == 0x41c67ea6);
        }
    });
    group("invalid impact targets rollback, inactive selectors remain Unsupported", [] {
        for (auto selector : {Word(0), Word(0xffff), Word(0x45), Word(6)}) {
            auto s = fixture();
            s.actors[0].action = 1;
            auto before = s;
            Rng rng(1);
            ImpactState local;
            local.context = {9, 9, 0, 1};
            local.selector = selector;
            unsupported([&] { primary_impact(s, 0, 0x5208, local, rng); });
            same(s, before);
            CHECK(rng.seed() == 1);
        }
        auto s = fixture();
        Rng rng(1);
        ImpactState local;
        local.context = {99, 99, 0, 0};
        CHECK(!primary_impact(s, 99, 0x55f0, local, rng).combat && rng.seed() == 1);
        CHECK(!pending_commit(s, 99, local, 196, true).commit.committed);
    });
    group("default complete body and uninitialized followup explicitly unsupported", [] {
        Body body;
        Boundary boundary(body);
        Schedule cb;
        auto d = data_fixture();
        auto before = d.state;
        session::ResolverRequest req;
        req.microseconds_low16 = 7;
        req.entry = begin_resolver_entry(d.state, 7, d.rng);
        unsupported([&] { boundary.resolve(d, req, cb); });
        same(d.state, before);
        CHECK(d.rng.seed() == 7);
        d.state.actors[0].population = {};
        Rng rng(1);
        auto prior = d.state;
        unsupported([&] { start_resolver_action(d.state, 0, 1, rng); });
        same(d.state, prior);
        CHECK(rng.seed() == 1);
    });
    group("boundary rejects wrong order/reseed and stages failing body", [] {
        ScriptBody body;
        body.fn = [](session::Data &d, const session::ResolverRequest &,
                     session::CallbackSchedule &) -> Word {
            d.rng.next();
            d.state.actors[0].population[0] = 0;
            throw Unsupported("unknown callback interleaving");
        };
        Boundary boundary(body);
        Schedule cb;
        auto d = data_fixture();
        auto before = d.state;
        session::ResolverRequest req;
        req.microseconds_low16 = 7;
        req.entry = begin_resolver_entry(d.state, 7, d.rng);
        unsupported([&] { boundary.resolve(d, req, cb); });
        same(d.state, before);
        CHECK(d.rng.seed() == 7);
        req.actor_order[0] = 4;
        unsupported([&] { boundary.resolve(d, req, cb); });
        CHECK(d.rng.seed() == 7);
        req.actor_order[0] = 0;
        d.rng.next();
        auto seed = d.rng.seed();
        unsupported([&] { boundary.resolve(d, req, cb); });
        CHECK(d.rng.seed() == seed);
    });
    group("missing actual callback schedule rejects a provider continuation", [] {
        ScriptBody body;
        body.fn = [](session::Data &d, const session::ResolverRequest &,
                     session::CallbackSchedule &callbacks) -> Word {
            d.state.actors[0].magnitude = 5;
            effect_marker(d.state, 0, 0x10, 0x400, 0, {0, 0, 0, 0}, d.rng);
            callbacks.apply(session::CallbackPoint::resolver_owned, d);
            return 0;
        };
        Boundary boundary(body);
        session::CallbackSchedule missing;
        auto d = data_fixture();
        session::ResolverRequest req;
        req.microseconds_low16 = 7;
        req.entry = begin_resolver_entry(d.state, 7, d.rng);
        auto before = d.state;
        unsupported([&] { boundary.resolve(d, req, missing); });
        same(d.state, before);
        CHECK(d.rng.seed() == 7);
    });
    group("session abort fixture reaches AI production once, retains human control", [] {
        ScriptBody body;
        body.fn = [](session::Data &d, const session::ResolverRequest &,
                     session::CallbackSchedule &) -> Word {
            CHECK(d.rng.seed() == 123); // coordinator already reseeded; no duplicate seed
            for (auto &a : d.state.actors)
                a.action = 6;
            const auto input = process_resolver_input(true, 8, 0x68, 1, 1, {});
            CHECK(input.state.control == 1);
            return input.state.control; // fixture assumes reached exit after abort
        };
        SetupBoundary boundary(body);
        Schedule cb;
        session::Adapters adapters{boundary, cb};
        session::Coordinator c(data_fixture());
        handoff(c, adapters);
        CHECK(c.advance(adapters, 123) && c.phase() == session::Phase::ai_production &&
              c.retained_control() == 0 && c.resolver_result() == 1);
        auto reference = c.data().state;
        auto rng = c.data().rng;
        auto expected = finish_ai_production(reference, rng);
        CHECK(c.advance(adapters) && c.phase() == session::Phase::resolver_control);
        same(c.data().state, reference);
        CHECK(c.data().rng.seed() == rng.seed());
        for (bool v : expected.produced)
            CHECK(v);
        CHECK(c.advance(adapters) && c.phase() == session::Phase::return_routing);
        CHECK(c.advance(adapters) && c.phase() == session::Phase::restart &&
              c.retained_control() == 0);
    });
    group("unsupported integrated body rolls back reseed and all slice effects", [] {
        ScriptBody body;
        body.fn = [](session::Data &d, const session::ResolverRequest &,
                     session::CallbackSchedule &) -> Word {
            d.state.actors[0].magnitude = 5;
            effect_marker(d.state, 0, 0x10, 0x400, 0, {0, 0, 0, 0}, d.rng);
            throw Unsupported("no instruction-supported continuation supplied");
        };
        SetupBoundary boundary(body);
        Schedule cb;
        session::Adapters adapters{boundary, cb};
        session::Coordinator c(data_fixture());
        handoff(c, adapters);
        auto before = c.data().state;
        auto seed = c.data().rng.seed();
        CHECK(!c.advance(adapters, 123));
        same(c.data().state, before);
        CHECK(c.data().rng.seed() == seed && c.phase() == session::Phase::resolver_handoff &&
              c.diagnostic());
    });
    std::cout << groups << " groups, " << checks << " checks passed\n";
}
