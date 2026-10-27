#pragma once
#include "game/revenge/revenge_system.hpp"
namespace nuclear::player {
// Existing synchronous normal resolver plus USER-DEFINED four-attempt revenge.
// Original native follow-up guard is neither called nor reinterpreted.
class TurnResolverAdapter : public session::Boundaries {
  public:
    Word setup(session::Data &d, session::CallbackSchedule &) override {
        d.revenge = session::RevengeState{};
        initialize_state(d.state, 0, d.rng);
        return 0;
    }
    Word resolve(session::Data &d, const session::ResolverRequest &request,
                 session::CallbackSchedule &) override {
        if (request.actor_order != std::array<std::size_t, 5>{{0, 1, 2, 3, 4}} ||
            d.rng.seed() != request.microseconds_low16)
            throw Unsupported("prototype requires coordinator reseed and original actor order");
        d.revenge.events.clear();
        d.revenge.contains_death = false;
        // Committed action5 is an attack by an already-deployed aircraft, not stock.
        for (std::size_t i = 0; i < 5; ++i)
            if (d.state.actors[i].action == 5 && populated_cities(d.state.actors[i])) {
                auto &b = d.revenge.airborne[i];
                b.capacity = bomber_capacity_left(d.state, i);
                b.active = signed_word(b.capacity) >= 0;
                b.reserved_payload = 0;
                for (Word h = 0; h < 4; ++h)
                    if (d.state.actors[i].magnitude == class_effect(h) && b.active)
                        b.reserved_payload = d.state.actors[i].magnitude;
            }
        RevengeQueue revenge(d);
        for (auto actor : request.actor_order) {
            auto &a = d.state.actors[actor];
            if (!populated_cities(a) || a.action == 0 || a.action == 88)
                continue;
            try {
                session::ResolvedTurnEvent e;
                e.actor = actor;
                e.before = d.state;
                e.seed_before = d.rng.seed();
                auto start = start_resolver_action(d.state, actor, 0, d.rng);
                if (!start.initialized)
                    continue;
                e.action = a.action;
                e.variant = start.handler.message_variant;
                e.effect = start.handler.effect;
                e.target = a.target_actor;
                e.city = a.target_city;
                e.magnitude = a.magnitude;
                if (e.target < 5 && e.city < 5)
                    e.population_before = d.state.actors[e.target].population[e.city];
                resolver::Context context{a.target_actor, a.target_city, a.magnitude, 0};
                if (a.action == 1 || a.action == 5) {
                    resolve_attack(d, e, start.prelude.magnitude_was_25);
                    if (e.action == 5) {
                        auto &b = d.revenge.airborne[actor];
                        b.reserved_payload = 0;
                        b.capacity = bomber_capacity_left(d.state, actor);
                        b.active = false; /* This flight has delivered its payload. */
                    }
                } else if (start.handler.effect != 255) {
                    auto result = resolver::effect_marker(d.state, actor, start.handler.effect,
                                                          0x400, 0, context, d.rng);
                    e.result_recorded = true;
                    e.result_variant = result.message_variant.value_or(0);
                    e.result_amount = result.context.magnitude;
                    e.target = e.action == 15 ? Word(actor) : result.context.target_actor;
                    e.city = result.context.target_city;
                    if ((e.action == 3 && e.effect == 13) || (e.action == 8 && e.effect == 15))
                        e.loss = result.context.magnitude;
                    if (e.target < 5) {
                        e.population_known = true;
                        for (std::size_t c = 0; c < 5; ++c)
                            e.population_delta +=
                                signed_word(d.state.actors[e.target].population[c]) -
                                signed_word(e.before.actors[e.target].population[c]);
                        if (e.city < 5)
                            e.population_before = e.before.actors[e.target].population[e.city];
                    }
                }
                if (e.action == 4) {
                    auto &b = d.revenge.airborne[actor];
                    if (b.active)
                        d.revenge.additional_airborne[actor].push_back(b);
                    b.death_eligible = false;
                    b.active = true;
                    b.capacity = bomber_capacity_left(d.state, actor);
                    b.reserved_payload = 0;
                }
                e.after = d.state;
                e.seed_after = d.rng.seed();
                d.revenge.events.push_back(e);
                revenge.detect(e.before);
                revenge.drain();
            } catch (const Unsupported &e) {
                throw Unsupported("resolver actor " + std::to_string(actor) + " action " +
                                  std::to_string(a.action) + ": " + e.what());
            }
        }
        return 0;
    }
    Word ending(session::Data &, session::CallbackSchedule &) override { return 2; }
};
} // namespace nuclear::player
