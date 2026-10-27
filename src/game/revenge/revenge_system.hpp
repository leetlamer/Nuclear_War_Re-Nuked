#pragma once
#include "nuclear/single_missile.hpp"
#include <deque>
namespace nuclear::player {
// Shared normal combat path. Caller has already committed the payload/roll.
inline void resolve_attack(session::Data &session, session::ResolvedTurnEvent &attack,
                           bool magnitude25) {
    resolver::ImpactState local;
    local.context = {attack.target, attack.city, attack.magnitude, 0};
    local.selector = attack.effect;
    local.magnitude_was_25 = magnitude25;
    auto impact = resolver::primary_impact(session.state, attack.actor, 0x5208, local, session.rng);
    attack.effect = impact.local.selector;
    resolver::DamageAcknowledgment ack(attack.actor, impact.local);
    auto write = ack.write_at_gate(session.state, 0, true);
    ack.observe_render_request(session.state, 0);
    attack.loss = write.loss;
}
inline Word bomber_capacity_left(const State &s, std::size_t attacker_index) {
    return attacker_index == 4 ? s.human_prepared_capacity : s.prepared_capacity[attacker_index];
}
struct RevengeWeapon {
    Word action{}, delivery{}, warhead_class{}, payload_strength{}, capacity_cost{};
    bool reserved{};
    std::size_t flight{};
};
inline std::optional<RevengeWeapon> revenge_weapon(const session::Data &session,
                                                   std::size_t attacker_index) {
    const auto &attacker = session.state.actors[attacker_index];
    // Only aircraft captured at death, still active, and not already delivered.
    for (std::size_t slot = 0; slot <= session.revenge.additional_airborne[attacker_index].size();
         ++slot) {
        const auto &flight = slot ? session.revenge.additional_airborne[attacker_index][slot - 1]
                                  : session.revenge.airborne[attacker_index];
        if (!flight.active || !flight.death_eligible)
            continue;
        if (flight.reserved_payload)
            return RevengeWeapon{5, 0, 0, flight.reserved_payload, 0, true, slot};
        for (Word warhead_class = 0; warhead_class < 4; ++warhead_class)
            if (attacker.warheads[warhead_class] &&
                signed_word(flight.capacity) >= int(class_capacity(warhead_class)))
                return RevengeWeapon{5,
                                     0,
                                     warhead_class,
                                     class_effect(warhead_class),
                                     class_capacity(warhead_class),
                                     false,
                                     slot};
    }
    // Deterministic ascending missile class, then ascending compatible warhead.
    for (Word missile_class = 0; missile_class < 4; ++missile_class)
        if (attacker.missiles[missile_class])
            for (Word warhead_class = 0; warhead_class <= missile_class; ++warhead_class)
                if (attacker.warheads[warhead_class])
                    return RevengeWeapon{
                        1, missile_class, warhead_class, class_effect(warhead_class), 0, false, 0};
    return std::nullopt;
}
// FIFO, bounded to5deaths/20attempts. No presentation callback executes this.
class RevengeQueue {
    session::Data &session_;
    std::deque<std::size_t> pending_;

  public:
    explicit RevengeQueue(session::Data &session) : session_(session) {}
    void detect(const State &before) {
        for (std::size_t attacker_index = 0; attacker_index < 5; ++attacker_index)
            if (populated_cities(before.actors[attacker_index]) &&
                !populated_cities(session_.state.actors[attacker_index]) &&
                !session_.revenge.announced[attacker_index]) {
                session_.revenge.airborne[attacker_index].death_eligible =
                    session_.revenge.airborne[attacker_index].active;
                for (auto &flight : session_.revenge.additional_airborne[attacker_index])
                    flight.death_eligible = flight.active;
                session_.revenge.announced[attacker_index] = true;
                session_.revenge.contains_death = true;
                pending_.push_back(attacker_index);
                session::ResolvedTurnEvent notice;
                notice.death = true;
                notice.actor = attacker_index;
                notice.before = notice.after = session_.state;
                notice.seed_before = notice.seed_after = session_.rng.seed();
                session_.revenge.events.push_back(std::move(notice));
            }
    }
    void drain() {
        while (!pending_.empty()) {
            auto attacker_index = pending_.front();
            pending_.pop_front();
            std::array<bool, 5> targeted{};
            while (session_.revenge.attacks[attacker_index] < 4) {
                auto weapon = revenge_weapon(session_, attacker_index);
                if (!weapon)
                    break;
                std::vector<Word> players;
                for (Word target_index = 0; target_index < 5; ++target_index)
                    if (target_index != attacker_index &&
                        populated_cities(session_.state.actors[target_index]) &&
                        !targeted[target_index])
                        players.push_back(target_index);
                if (players.empty()) {
                    targeted.fill(false);
                    for (Word target_index = 0; target_index < 5; ++target_index)
                        if (target_index != attacker_index &&
                            populated_cities(session_.state.actors[target_index]))
                            players.push_back(target_index);
                }
                if (players.empty())
                    break;
                session::ResolvedTurnEvent attack;
                attack.revenge = true;
                attack.actor = attacker_index;
                attack.action = weapon->action;
                attack.magnitude = weapon->payload_strength;
                attack.before = session_.state;
                attack.seed_before = session_.rng.seed();
                attack.target = players[session_.rng.next() % players.size()];
                std::vector<Word> cities;
                for (Word city_index = 0; city_index < 5; ++city_index)
                    if (signed_word(session_.state.actors[attack.target].population[city_index]) >
                        0)
                        cities.push_back(city_index);
                attack.city = cities[session_.rng.next() % cities.size()];
                targeted[attack.target] = true;
                attack.population_before =
                    session_.state.actors[attack.target].population[attack.city];
                auto &attacker = session_.state.actors[attacker_index];
                auto &flight =
                    weapon->flight
                        ? session_.revenge.additional_airborne[attacker_index][weapon->flight - 1]
                        : session_.revenge.airborne[attacker_index];
                if (attack.action == 1) {
                    --attacker.missiles[weapon->delivery];
                    --attacker.warheads[weapon->warhead_class];
                } else {
                    if (weapon->reserved)
                        flight.reserved_payload = 0;
                    else {
                        --attacker.warheads[weapon->warhead_class];
                        flight.capacity = sub_word(flight.capacity, weapon->capacity_cost);
                    }
                    session_.state.prepared_capacity[attacker_index] = flight.capacity;
                }
                attacker.action = attack.action;
                attacker.target_actor = attack.target;
                attacker.target_city = attack.city;
                attacker.magnitude = attack.magnitude;
                ++session_.revenge.attacks[attacker_index];
                auto roll = begin_attack_roll(attack.action, session_.rng);
                attack.variant = roll.message_variant;
                attack.effect = roll.effect;
                resolve_attack(session_, attack, attack.magnitude == 25);
                if (attack.action == 5) {
                    flight.active = false;
                    flight.death_eligible = false;
                    flight.capacity = session_.state.prepared_capacity[attacker_index];
                    if (attacker_index == 4)
                        session_.state.human_prepared_capacity = flight.capacity;
                }
                attack.after = session_.state;
                attack.seed_after = session_.rng.seed();
                session_.revenge.events.push_back(attack);
                detect(attack.before);
            }
        }
    }
};
} // namespace nuclear::player
