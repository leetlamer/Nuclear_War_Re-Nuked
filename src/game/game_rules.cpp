#include "nuclear/engine.hpp"
#include <algorithm>
#include <initializer_list>

namespace nuclear {
namespace {
void actor_index(std::size_t i) {
    if (i >= actor_count)
        throw Unsupported("actor index outside 0..4");
}
void city_index(std::size_t i) {
    if (i >= city_count)
        throw Unsupported("city index outside 0..4");
}
template <std::size_t N> int highest(const std::array<Word, N> &a) noexcept {
    for (std::size_t i = N; i != 0; --i)
        if (a[i - 1] != 0)
            return int(i - 1);
    return -1;
}
void check_roll(Word r) {
    if (r < 1 || r > 100)
        throw Unsupported("expected original rand()%100+1");
}
void category_indices(Word p, Word r) {
    if (p >= 5 || r >= 5)
        throw Unsupported("AI table index outside verified 5x5 tables");
}
} // namespace
Actor decode_actor(const ActorImage &b) {
    Actor a;
    std::size_t o = 0;
    auto byte = [&]() { return b[o++]; };
    auto word = [&]() {
        Word h = byte();
        return Word((h << 8) | byte());
    };
    a.profile = word();
    a.action = word();
    a.target_actor = word();
    a.target_city = word();
    for (auto &x : a.unknown_longs) {
        std::uint32_t h = word();
        x = (h << 16) | word();
    }
    for (auto &x : a.classifications)
        x = byte();
    a.unknown_21 = byte();
    for (auto *v : {&a.scores, &a.city_x, &a.city_y, &a.population})
        for (auto &x : *v)
            x = word();
    for (auto &x : a.unknown_4a)
        x = byte();
    for (auto *v : {&a.missiles, &a.warheads})
        for (auto &x : *v)
            x = word();
    for (auto *v : {&a.bombers, &a.defenses})
        for (auto &x : *v)
            x = word();
    a.magnitude = word();
    a.tally = word();
    return a;
}
ActorImage encode_actor(const Actor &a) {
    ActorImage b{};
    std::size_t o = 0;
    auto byte = [&](std::uint8_t v) { b[o++] = v; };
    auto word = [&](Word v) {
        byte(std::uint8_t(v >> 8));
        byte(std::uint8_t(v));
    };
    word(a.profile);
    word(a.action);
    word(a.target_actor);
    word(a.target_city);
    for (auto x : a.unknown_longs) {
        word(Word(x >> 16));
        word(Word(x));
    }
    for (auto x : a.classifications)
        byte(x);
    byte(a.unknown_21);
    for (auto *v : {&a.scores, &a.city_x, &a.city_y, &a.population})
        for (auto x : *v)
            word(x);
    for (auto x : a.unknown_4a)
        byte(x);
    for (auto *v : {&a.missiles, &a.warheads})
        for (auto x : *v)
            word(x);
    for (auto *v : {&a.bombers, &a.defenses})
        for (auto x : *v)
            word(x);
    word(a.magnitude);
    word(a.tally);
    return b;
}
Word Rng::next() noexcept {
    seed_ = std::uint32_t(std::uint64_t(seed_) * 0x41c64e6du + 0x3039u);
    return Word((seed_ >> 16) & 0x7fffu);
}
Word action3_event_for_roll(Word roll) {
    check_roll(roll);
    if (roll <= 4)
        return 11;
    if (roll <= 6)
        return 12;
    if (roll <= 8)
        return 16;
    if (roll <= 13)
        return 9;
    if (roll <= 18)
        return 13;
    if (roll <= 20)
        return 0x56;
    if (roll <= 23)
        return 15;
    if (roll <= 25)
        return 14;
    return 8;
}
Action3Decision decide_action3_substitution(Word action, Rng &rng) {
    Action3Decision result;
    result.selected_action = action;
    if (action != 3)
        return result;
    result.gate_roll = Word(rng.next() % 100 + 1);
    result.draws = 1;
    if (result.gate_roll >= 30)
        return result;
    result.event_roll = Word(rng.next() % 100 + 1);
    result.draws = 2;
    result.selected_action = action3_event_for_roll(result.event_roll);
    result.substituted = true;
    return result;
}
std::size_t quake_event_prelude(State &s, std::size_t i, Rng &rng) {
    actor_index(i);
    auto &a = s.actors[i];
    if (std::all_of(a.population.begin(), a.population.end(), [](Word p) { return p == 0; }))
        throw Unsupported("quake nonzero-city scan would not terminate");
    std::size_t city = rng.next() % 5;
    while (a.population[city] == 0)
        city = (city + 1) % 5;
    a.action = 0x56;
    a.target_city = Word(city);
    a.target_actor = Word(i);
    a.magnitude = a.population[city];
    return city;
}
Word initial_city_population(Word profile, bool human, Word mode, Rng &rng) {
    if (human && mode != 0)
        return 0;
    if (human)
        profile = 0;
    if (profile >= 5)
        throw Unsupported("initializer population local undefined for unknown AI profile");
    constexpr Word modulus[]{26, 16, 16, 50, 36}, base[]{10, 15, 20, 1, 5};
    return Word(rng.next() % modulus[profile] + base[profile]);
}
void initialize_state(State &s, Word mode, Rng &rng) {
    for (std::size_t i = 0; i < 4; ++i)
        if (s.actors[i].profile >= 5)
            throw Unsupported("initializer profile outside verified 0..4");
    constexpr Word xs[5][5]{{26, 80, 125, 20, 82},
                            {4, 68, 4, 73, 106},
                            {209, 241, 249, 290, 302},
                            {244, 299, 225, 223, 297},
                            {130, 115, 130, 167, 164}};
    constexpr Word ys[5][5]{{36, 20, 22, 64, 77},
                            {136, 120, 172, 167, 170},
                            {54, 37, 80, 72, 98},
                            {117, 122, 137, 172, 167},
                            {82, 107, 125, 109, 130}};
    for (std::size_t i = 0; i < 5; ++i) {
        auto &a = s.actors[i];
        if (!s.participating[i]) {
            a = Actor{};
            a.action = 88;
            s.pass_processed[i] = s.remaining_total[i] = s.prepared_capacity[i] = 0;
            continue;
        }
        a.target_actor = 0;
        a.target_city = 0;
        a.magnitude = 0;
        a.tally = 0;
        a.unknown_longs = {};
        a.classifications = {};
        a.scores = {};
        a.missiles = {};
        a.warheads = {};
        a.bombers = {};
        a.defenses = {};
        s.pass_processed[i] = 0;
        s.remaining_total[i] = 0;
        for (std::size_t c = 0; c < 5; ++c) {
            a.city_x[c] = xs[i][c];
            a.city_y[c] = ys[i][c];
            a.population[c] = initial_city_population(a.profile, i == 4, mode, rng);
            s.remaining_total[i] = add_word(s.remaining_total[i], a.population[c]);
            a.tally = add_word(a.tally, a.population[c]);
        }
        if (i != 4)
            switch (a.profile) {
            case 0:
                a.warheads = {1, 3, 0, 0};
                a.missiles = {1, 3, 0, 0};
                a.defenses[0] = 1;
                break;
            case 1:
                a.warheads = {1, 2, 2, 0};
                a.missiles = {1, 1, 1, 0};
                a.bombers[0] = 1;
                break;
            case 2:
                if (s.identity[i] != 9) {
                    a.missiles[0] = 3;
                    a.warheads[0] = 3;
                }
                a.defenses = {1, 1};
                break;
            case 3:
                (void)produce(s, i, rng);
                (void)produce(s, i, rng);
                break;
            case 4:
                a.warheads = {2, 2, 0, 0};
                a.missiles = {2, 2, 0, 0};
                break;
            }
        if (i == 4 && mode == 0) {
            a.warheads[0] = 1;
            a.missiles[0] = 1;
            a.defenses[0] = 1;
        }
        a.action = 0;
    }
}
Word populated_cities(const Actor &a) noexcept {
    Word n = 0;
    for (Word p : a.population)
        if (signed_word(p) > 0)
            ++n;
    return n;
}
Word living_actors(const State &s) noexcept {
    Word n = 0;
    for (const auto &a : s.actors)
        if (populated_cities(a) != 0)
            ++n;
    return n;
}
int last_surviving_actor(const State &s) noexcept {
    int last = -1;
    for (std::size_t i = 0; i < actor_count; ++i)
        if (populated_cities(s.actors[i]) != 0)
            last = int(i);
    return last;
}
EndScore end_game_score(const Actor &a) noexcept {
    EndScore result;
    for (auto p : a.population) {
        result.population_total = add_word(result.population_total, p);
        if (signed_word(p) > 0)
            ++result.positive_cities;
    }
    auto bonus = Word(signed_word(sub_word(result.positive_cities, 1)) * 25);
    result.score = add_word(result.population_total, bonus);
    return result;
}
bool end_condition(const State &s) noexcept { return living_actors(s) <= 1; }
Word clamp_loss(Word loss, Word pop) noexcept {
    if (signed_word(loss) > signed_word(pop))
        loss = pop;
    if (signed_word(loss) < 0)
        loss = 0;
    return loss;
}
Word population_add(Word p, Word d) noexcept { return add_word(p, d); }
Word population_subtract(Word p, Word d) noexcept { return sub_word(p, d); }
Word class_capacity(Word i) noexcept {
    constexpr Word t[]{10, 20, 50, 100};
    return i < 4 ? t[i] : 0;
}
Word class_effect(Word i) noexcept {
    constexpr Word t[]{2, 5, 10, 25};
    return i < 4 ? t[i] : 0;
}
int highest_missile(const Actor &a) noexcept { return highest(a.missiles); }
int highest_warhead(const Actor &a) noexcept { return highest(a.warheads); }
int highest_bomber(const Actor &a) noexcept { return highest(a.bombers); }
int fitting_warhead(const Actor &a, Word cap) noexcept {
    for (int c = 3; c >= 0; --c)
        if (a.warheads[std::size_t(c)] != 0 && signed_word(cap) >= class_capacity(Word(c)))
            return c;
    return -1;
}
int choose_city(const Actor &a, const Actor &t, Rng &rng) {
    int chosen = -1;
    std::int32_t limit = 0;
    switch (a.profile) {
    case 0:
        for (std::size_t i = 0; i < 5; ++i)
            if (signed_word(t.population[i]) > 0)
                chosen = int(i);
        return chosen;
    case 1:
        for (std::size_t i = 0; i < 5; ++i)
            if (signed_word(t.population[i]) > limit) {
                limit = signed_word(t.population[i]);
                chosen = int(i);
            }
        return chosen;
    case 2:
        chosen = 0;
        limit = 255;
        for (std::size_t i = 0; i < 5; ++i)
            if (signed_word(t.population[i]) > 0 && signed_word(t.population[i]) <= limit) {
                limit = signed_word(t.population[i]);
                chosen = int(i);
            }
        return chosen;
    case 3:
    case 4: {
        if (!populated_cities(t))
            throw Unsupported("original random city scan would not terminate");
        Word steps = Word(rng.next() % 5 + 1);
        std::size_t i = 0;
        for (;;) {
            if (signed_word(t.population[i]) > 0 && --steps == 0)
                return int(i);
            i = (i + 1) % 5;
        }
    }
    default:
        return -1; // initialized local, no matching profile case
    }
}
const std::array<std::array<Word, 5>, 5> category_one_table{{{{5, 0, 100, 30, 60}},
                                                             {{15, 5, 94, 40, 50}},
                                                             {{80, 10, 90, 35, 50}},
                                                             {{50, 5, 80, 40, 60}},
                                                             {{30, 0, 70, 50, 33}}}};
const std::array<std::array<Word, 5>, 5> category_two_table{{{{95, 100, 0, 70, 40}},
                                                             {{85, 95, 6, 60, 50}},
                                                             {{20, 90, 10, 65, 50}},
                                                             {{50, 95, 20, 60, 40}},
                                                             {{70, 100, 30, 50, 67}}}};
Word ai_category_for_roll(Word p, Word r, Word roll) {
    category_indices(p, r);
    check_roll(roll);
    Word t1 = category_one_table[p][r], sum = add_word(t1, category_two_table[p][r]);
    if (signed_word(roll) <= signed_word(t1))
        return 1;
    if (signed_word(roll) <= signed_word(sum))
        return 2;
    throw Unsupported("original category local uninitialized on this path");
}
Word ai_category(Word p, Word r, Rng &rng) {
    category_indices(p, r);
    return ai_category_for_roll(p, r, Word(rng.next() % 100 + 1));
}
Word relationship_bucket(Word bits) noexcept {
    auto s = signed_word(bits);
    if (s < -18000)
        return Word(0xfffe);
    if (s < -9000)
        return Word(0xffff);
    if (s > 18000)
        return 2;
    if (s > 9000)
        return 1;
    return 0;
}
Word relationship_tier(Word bits) noexcept {
    auto score = signed_word(bits);
    return Word(score < -9000 ? 0 : score > 9000 ? 2 : 1);
}
namespace {
constexpr std::int16_t attitude_tables[3][5][5]{{{-5, 5, -5, 10, 2000},
                                                 {-20, 10, -20, 20, -10},
                                                 {10, 10, -5, 30, 20},
                                                 {20, -5, 10, -5, 5},
                                                 {30, 10, 20, -30, 2000}},
                                                {{5, -20, 5, -5, 2000},
                                                 {-10, 5, -10, -5, 5},
                                                 {-5, 20, -10, 20, -10},
                                                 {5, -5, 10, -5, 5},
                                                 {10, -10, 10, -20, 2000}},
                                                {{20, 30, -40, -20, 2000},
                                                 {10, 20, -10, -10, -20},
                                                 {-5, 5, -5, 10, -10},
                                                 {30, -10, 5, 20, -5},
                                                 {30, 30, 30, 30, 2000}}};
}
Word attitude_table_value(Word tier, Word previous, Word profile) {
    if (tier > 2)
        return 0; // default initialized return local
    Word row = add_word(previous, 2);
    if (row >= 5 || profile >= 5)
        throw Unsupported("attitude table index outside verified arrays");
    return Word(attitude_tables[tier][row][profile]);
}
void update_attitudes(State &s) {
    auto copy = s;
    for (std::size_t i = 0; i < 4; ++i)
        for (std::size_t j = 0; j < 5; ++j)
            if (j != i) {
                auto &a = copy.actors[i];
                Word tier = relationship_tier(a.scores[j]);
                Word previous = Word(signed_byte(copy.actors[j].classifications[i]));
                Word raw = attitude_table_value(tier, previous, a.profile);
                Word delta = Word(std::uint32_t(raw) * 100); // MULS.W -> low word stored at 10b13e
                auto score = signed_word(a.scores[j]) + signed_word(delta);
                a.scores[j] =
                    Word(std::max<std::int32_t>(-32767, std::min<std::int32_t>(32767, score)));
            }
    // The local tier is reloaded ONLY on the diagonal (10b1ea). Preserve its
    // carry between rows; do not recalculate a relationship for every cell.
    Word tier = 0; // overwritten at i=0,j=0 before any use
    for (std::size_t i = 0; i < 4; ++i)
        for (std::size_t j = 0; j < 5; ++j) {
            if (i == j)
                tier = relationship_tier(copy.actors[i].scores[j]);
            copy.actors[i].classifications[j] = std::uint8_t(sub_word(tier, 2));
        }
    s = copy;
}
Word event_score_delta(Word profile, Word tier, Word event, Word magnitude) {
    // DATA bases in event order: 130b08,130aea,130b44,130b26,130b62,130b80.
    constexpr std::int16_t tables[6][3][5]{
        {{2, 1, 3, 0, 0}, {0, -1, 1, -1, 1}, {-2, -3, -1, -1, -1}},
        {{4, 2, 6, 3, 3}, {4, 2, 6, 3, 3}, {4, 2, 6, 3, 3}},
        {{3, 4, 2, 1, 1}, {1, 2, -2, 0, 0}, {-1, 0, -2, -1, -1}},
        {{3, 6, 1, 4, 3}, {3, 6, 1, 4, 3}, {3, 6, 1, 4, 3}},
        {{-100, 100, -200, 100, 200}, {-50, 300, -100, 100, 100}, {300, 500, -500, 0, 100}},
        {{0, -40, 60, -20, 20}, {0, -40, 60, -20, 20}, {0, -40, 60, -20, 20}}};
    if (event >= 6)
        throw Unsupported("10ac8e return local uninitialized for unknown event type");
    if (profile >= 5 || tier >= 3)
        throw Unsupported("event score table index outside verified arrays");
    Word raw = Word(tables[event][tier][profile]);
    Word delta = Word(std::uint32_t(raw) * (event == 4 ? 10u : 100u));
    if (event < 4)
        delta = Word(std::uint32_t(delta) * magnitude);
    return delta;
}
void apply_relationship_event(State &s, std::size_t actor, Word event, Word affected,
                              Word magnitude) {
    actor_index(actor);
    if (event >= 6)
        throw Unsupported("event type outside verified 0..5");
    auto copy = s;
    for (std::size_t i = 0; i < 4; ++i)
        if (i != actor) {
            auto &observer = copy.actors[i];
            auto &score = observer.scores[actor];
            Word effective = add_word(event, Word(affected == i && (event == 0 || event == 2)));
            bool direct = effective == 1 || effective == 3;
            if (direct && signed_word(score) < -9000) {
                score = sub_word(0, score); // NEG.W, including -32768 -> -32768; no clamp
            } else {
                Word delta = event_score_delta(observer.profile, relationship_tier(score),
                                               effective, magnitude);
                auto total = signed_word(score) + signed_word(delta) + (direct ? 9000 : 0);
                score = Word(std::max<std::int32_t>(-32767, std::min<std::int32_t>(32767, total)));
            }
        }
    s = copy;
}
void perturb_profile3_scores(State &s, Rng &rng) {
    for (std::size_t i = 0; i < 4; ++i)
        if (s.actors[i].profile == 3 && populated_cities(s.actors[i]) != 0) {
            for (std::size_t j = 0; j < 5; ++j)
                if (j != i && populated_cities(s.actors[j]) != 0) {
                    auto factor = std::int32_t(rng.next() % 5) - 2;
                    Word delta = Word(factor * 9000); // original MULS.W then word local
                    auto total = signed_word(s.actors[i].scores[j]) + signed_word(delta);
                    s.actors[i].scores[j] =
                        Word(std::max<std::int32_t>(-32767, std::min<std::int32_t>(32767, total)));
                }
        }
}
void plan_propaganda(State &s, std::size_t i, std::size_t t, Rng &rng) {
    actor_index(i);
    actor_index(t);
    auto &a = s.actors[i];
    int c = choose_city(a, s.actors[t], rng);
    if (c != -1) {
        a.action = 3;
        a.target_actor = Word(t);
        a.target_city = Word(c);
        (void)rng.next();
        a.magnitude = Word(rng.next() % 10 + 1);
    }
}
bool plan_defense(State &s, std::size_t i) {
    actor_index(i);
    std::size_t t = s.selected_opponent[i];
    actor_index(t);
    auto &a = s.actors[i];
    auto score = signed_word(s.actors[t].scores[i]);
    if (t != 4)
        for (std::size_t c = 0; c < 4; ++c) {
            if (c != i && populated_cities(s.actors[c]) &&
                score < signed_word(s.actors[c].scores[i])) {
                score = signed_word(s.actors[c].scores[i]);
                t = c;
            }
        }
    Word action = s.actors[t].action;
    bool earlier = t < i || t == 4;
    int defense = -1;
    if (action == (earlier ? 5 : 4) && a.defenses[1] != 0)
        defense = 1;
    if (action == (earlier ? 1 : 2)) {
        if (a.defenses[0] != 0)
            defense = 0;
        else if (a.defenses[1] != 0)
            defense = 1;
    }
    if (defense < 0)
        return false;
    a.defenses[std::size_t(defense)] = sub_word(a.defenses[std::size_t(defense)], 1);
    a.action = 7;
    a.magnitude = Word(defense);
    return true;
}
Word select_opponent(State &s, std::size_t i, Rng &rng) {
    actor_index(i);
    auto &a = s.actors[i];
    std::size_t best = i;
    std::int32_t max = -32767;
    s.selected_opponent[i] = 4;
    for (std::size_t c = 0; c < 5; ++c)
        if (c != i && populated_cities(s.actors[c]) && signed_word(a.scores[c]) >= max) {
            max = signed_word(a.scores[c]);
            best = c;
            s.selected_opponent[i] = Word(c);
        }
    Word ties = 0;
    for (std::size_t c = 0; c < 5; ++c)
        if (c != i && populated_cities(s.actors[c]) && signed_word(a.scores[c]) == max)
            ++ties;
    if (ties > 1) {
        Word ordinal = Word(rng.next() % ties + 1);
        for (std::size_t c = 0; c < 5; ++c)
            if (c != i && populated_cities(s.actors[c])) {
                if (signed_word(a.scores[c]) == max)
                    --ordinal;
                if (!ordinal) {
                    s.selected_opponent[i] = Word(c);
                    break;
                }
            }
    } else {
        int second = -1;
        std::int32_t next = -32767;
        for (std::size_t c = 0; c < 5; ++c)
            if (c != i && populated_cities(s.actors[c]) && signed_word(a.scores[c]) != max &&
                signed_word(a.scores[c]) >= next) {
                next = signed_word(a.scores[c]);
                second = int(c);
            }
        if (second < 0)
            s.selected_opponent[i] = Word(best);
        else {
            // 10b494 computes /834 in D0 but never writes it back to -1a(A5).
            // 10b49a compares the ORIGINAL 32-bit difference in that local.
            auto difference = std::min<std::int32_t>(max - next, 50);
            Word roll = Word(rng.next() % 100 + 1);
            s.selected_opponent[i] = Word(roll > difference + 50 ? std::size_t(second) : best);
        }
    }
    return relationship_bucket(a.scores[best]);
}
namespace {
void weapons_impl(State &s, std::size_t i, std::size_t t, Rng &rng) {
    auto &a = s.actors[i];
    Word city;
    if (!populated_cities(a)) {
        do {
            t = rng.next() % 5;
        } while (t == i);
        city = Word(rng.next() % 5);
    } else {
        city = Word(choose_city(a, s.actors[t], rng));
        // -1 is stored as a word by the original; no city is dereferenced here.
    }
    auto fallback = [&]() { a.action = populated_cities(a) ? 6 : 0; };
    auto &cap = s.prepared_capacity[i];
    if (a.action == 5 || a.action == 2 || a.action == 4) {
        if (a.action == 2) {
            a.action = 1;
            a.target_actor = Word(t);
            a.target_city = city;
            int c = fitting_warhead(a, cap);
            if (c != -1)
                a.warheads[std::size_t(c)] = sub_word(a.warheads[std::size_t(c)], 1);
            a.magnitude = class_effect(Word(c));
        } else {
            int c = highest_warhead(a); // extra stack capacity argument is ignored by 10cb1c
            if (c == -1 || signed_word(cap) < 0)
                fallback();
            else {
                a.action = 5;
                a.target_actor = Word(t);
                a.target_city = city;
                a.magnitude = class_effect(Word(c));
                a.warheads[std::size_t(c)] = sub_word(a.warheads[std::size_t(c)], 1);
                cap = sub_word(cap, class_capacity(Word(c)));
            }
        }
        return;
    }
    int warhead = highest_warhead(a), missile = highest_missile(a), bomber = highest_bomber(a);
    Word warhead_capacity = class_capacity(Word(warhead));
    // Both 10bac6 and 10bae6 push -c(A5), the WARHEAD index.
    Word missile_capacity = warhead_capacity;
    Word warheads = 0, missiles = 0, bombers = 0;
    for (auto x : a.warheads)
        warheads = add_word(warheads, x);
    for (auto x : a.missiles)
        missiles = add_word(missiles, x);
    for (auto x : a.bombers)
        bombers = add_word(bombers, x);
    bool enough = signed_word(add_word(missiles, bombers)) >= 3 && signed_word(warheads) >= 3;
    // Host reconstruction: initialize the fallback from the existing bomber class.
    // The recovered routine leaves this local undefined with three/four warheads.
    int plan = 3;
    Word bomber_capacity = Word(bomber == 0 ? 50 : 100);
    if (enough && bomber >= 0 && signed_word(warheads) >= 5) {
        if (signed_word(bomber_capacity) >= signed_word(warhead_capacity))
            plan = 2;
    }
    if (enough && plan == 3 && missile >= 0) {
        if (signed_word(missile_capacity) >= signed_word(warhead_capacity))
            plan = 1;
        if (fitting_warhead(a, missile_capacity) > 0)
            plan = 1;
    }
    if (enough && plan == 3 && bomber >= 0) {
        if (fitting_warhead(a, bomber_capacity) > 0)
            plan = 2;
    }
    if (plan == 1) {
        a.action = 2;
        auto c = std::size_t(missile);
        a.missiles[c] = sub_word(a.missiles[c], 1);
        cap = missile_capacity;
    } else if (plan == 2) {
        a.action = 4;
        auto c = std::size_t(bomber);
        a.bombers[c] = sub_word(a.bombers[c], 1);
        cap = bomber_capacity;
    } else
        fallback();
}
} // namespace
void plan_weapons(State &s, std::size_t i, std::size_t t, Rng &rng) {
    actor_index(i);
    actor_index(t);
    auto copy = s;
    auto random = rng;
    weapons_impl(copy, i, t, random);
    s = copy;
    rng = random;
}
ProducedItem production_item(Word k, Word c) {
    check_roll(k);
    check_roll(c);
    if (k <= 55)
        return {InventoryKind::Warhead, std::size_t(c <= 25 ? 0 : c <= 65 ? 1 : c <= 90 ? 2 : 3)};
    if (k <= 80)
        return {InventoryKind::Missile, std::size_t(c <= 20 ? 0 : c <= 55 ? 1 : c <= 85 ? 2 : 3)};
    return {k <= 95 ? InventoryKind::Bomber : InventoryKind::Defense, std::size_t(c > 60)};
}
namespace {
Production produce_items(Actor &a, Word n, Rng &rng) {
    Production out;
    auto draw = [&]() {
        ++out.draws;
        return rng.next();
    };
    for (Word j = 0; j < n; ++j) {
        Word k = Word(draw() % 100 + 1);
        if (k >= 56 && k <= 80)
            (void)draw(); // 10cfb4 result overwritten at 10cfd6
        auto item = production_item(k, Word(draw() % 100 + 1));
        Word *inventory = nullptr;
        Word *report = nullptr;
        switch (item.kind) {
        case InventoryKind::Warhead:
            inventory = &a.warheads[item.index];
            report = &out.warheads[item.index];
            break;
        case InventoryKind::Missile:
            inventory = &a.missiles[item.index];
            report = &out.missiles[item.index];
            break;
        case InventoryKind::Bomber:
            inventory = &a.bombers[item.index];
            report = &out.bombers[item.index];
            break;
        case InventoryKind::Defense:
            inventory = &a.defenses[item.index];
            report = &out.defenses[item.index];
            break;
        }
        *inventory = add_word(*inventory, 1);
        *report = add_word(*report, 1);
    }
    return out;
}
} // namespace
Production produce(State &s, std::size_t i, Rng &rng) {
    actor_index(i);
    if (i == human_actor)
        throw Unsupported("human production requires explicit persistent message context");
    return produce_items(s.actors[i], Word(populated_cities(s.actors[i]) + 1), rng);
}
AiProductionPhase finish_ai_production(State &s, Rng &rng) {
    AiProductionPhase result;
    for (std::size_t i = 0; i < 4; ++i)
        if (populated_cities(s.actors[i]) != 0 && s.actors[i].action == 6) {
            result.production[i] = produce(s, i, rng);
            result.produced[i] = true;
            result.draws = add_word(result.draws, result.production[i].draws);
        }
    return result;
}
CombatRoll begin_attack_roll(Word action, Rng &rng) {
    if (action != 1 && action != 5)
        throw Unsupported("attack initialization requires action1 or5");
    CombatRoll result;
    do {
        Word first = Word(rng.next() % 6);
        result.effect = Word(first + rng.next() % 6 + 2);
        result.draws += 2;
    } while (result.effect == 7);
    result.message_variant = Word(rng.next() % (action == 1 ? 3 : 4) + 1);
    ++result.draws;
    return result;
}
CombatImpact prepare_combat_impact(State &s, std::size_t attacker, std::size_t target,
                                   std::size_t city, Word effect, Rng &rng) {
    actor_index(attacker);
    actor_index(target);
    city_index(city);
    if (effect == 0 || effect == 0xffff)
        throw Unsupported("inactive impact branch has no damage draw");
    auto &a = s.actors[attacker];
    auto &d = s.actors[target];
    a.magnitude = add_word(a.magnitude, Word(rng.next() % 6 + 1));
    Word loss = a.magnitude;
    if (signed_word(loss) > signed_word(d.population[city]))
        loss = d.population[city];
    if (d.action == 7 && (d.magnitude == 1 || (a.action != 5 && d.magnitude == 0)))
        effect = 13;
    return {effect, loss};
}
Word combat_population_commit(State &s, std::size_t attacker, std::size_t target,
                              std::size_t city) {
    actor_index(attacker);
    actor_index(target);
    city_index(city);
    State copy = s;
    auto &a = copy.actors[attacker];
    auto &population = copy.actors[target].population[city];
    Word loss = a.magnitude;
    if (signed_word(loss) > signed_word(population)) {
        loss = population;
        a.magnitude = loss;
    }
    population = sub_word(population, loss);
    apply_relationship_event(copy, attacker, 0, Word(target), loss);
    s = copy;
    return loss;
}
Damage damage_effect(Word e, Word loss, Word pop, Word action, Word cap, bool magnitude_was_25) {
    switch (e) {
    case 2:
        if (action == 5) {
            action = 0x58;
            cap = 0;
            loss = 0;
        } else if (action == 1) {
            cap = 0;
            loss = 0;
        }
        break;
    case 3:
        loss = 0;
        break;
    case 4:
        loss = sub_word(loss, 2);
        if (signed_word(loss) < 0)
            loss = 0;
        break;
    case 5:
        loss = add_word(loss, 1);
        break;
    case 6:
    case 7:
        break;
    case 8:
        loss = add_word(loss, 2);
        break;
    case 9:
        loss = add_word(loss, 5);
        break;
    case 10:
        loss = add_word(loss, 10);
        break;
    case 11:
        loss = Word(std::uint32_t(loss) * 2);
        break;
    case 12:
        loss = Word(std::uint32_t(magnitude_was_25 ? pop : loss) * 3);
        break;
    case 13:
        if (action == 5) {
            action = 0;
            cap = 0;
            loss = 0;
        } else if (action == 1) {
            cap = 0;
            loss = 0;
        }
        break;
    default:
        throw Unsupported("damage effect outside verified 2..13 branch");
    }
    return {clamp_loss(loss, pop), action, cap};
}
CombatEvaluation evaluate_combat_impact(State &s, std::size_t attacker, std::size_t target,
                                        std::size_t city, Word effect, bool magnitude_was_25,
                                        Rng &rng) {
    actor_index(attacker);
    actor_index(target);
    city_index(city);
    if (effect < 2 || effect > 13)
        throw Unsupported("combat effect outside reconstructed2..13 branch");
    State copy = s;
    Rng random = rng;
    auto impact = prepare_combat_impact(copy, attacker, target, city, effect, random);
    auto &a = copy.actors[attacker];
    auto damage = damage_effect(impact.effect, impact.loss, copy.actors[target].population[city],
                                a.action, copy.prepared_capacity[attacker], magnitude_was_25);
    a.action = damage.action;
    a.magnitude = damage.loss;
    copy.prepared_capacity[attacker] = damage.prepared_capacity;
    s = copy;
    rng = random;
    return {impact.effect, damage};
}
std::size_t double_largest_city(Actor &a) {
    std::size_t city = 0;
    Word largest = 0;
    for (std::size_t c = 0; c < 5; ++c)
        if (signed_word(a.population[c]) > signed_word(largest)) {
            largest = a.population[c];
            city = c;
        }
    a.action = 0x16;
    a.population[city] = add_word(a.population[city], largest);
    a.magnitude = largest;
    return city;
}
Word growth_handler_initialize(Actor &a, Rng &rng) {
    Word message = Word(rng.next() % 4 + 1);
    a.magnitude = Word((rng.next() % 5 + 1) * 5);
    return message;
}
Word quake_handler_initialize(Actor &a, Rng &rng) {
    city_index(a.target_city);
    Word message = Word(rng.next() % 4 + 1);
    a.population[a.target_city] = 0;
    return message;
}
Word city_loss_handler_initialize(Actor &a, Word action, Rng &rng) {
    if (action != 11 && action != 12)
        throw Unsupported("city loss initialization requires action11 or12");
    std::size_t city = 0;
    while (city < city_count && a.population[city] == 0)
        ++city;
    if (city == city_count)
        throw Unsupported("original city loss scan reads past five populations");
    Word message = Word(rng.next() % (action == 11 ? 4 : 3) + 1);
    a.target_city = Word(city);
    a.magnitude = a.population[city];
    return message;
}
void growth_commit(Actor &a, std::size_t c) {
    city_index(c);
    a.population[c] = add_word(a.population[c], a.magnitude);
}
void radiation_population_commit(Actor &a, Rng &rng) {
    a.magnitude = 0;
    Word loss = Word(rng.next() % 5 + 1);
    a.action = 0x15;
    for (auto &population : a.population)
        if (population != 0) {
            if (signed_word(loss) > signed_word(population)) {
                a.magnitude = add_word(a.magnitude, population);
                population = 0;
            } else {
                population = sub_word(population, loss);
                a.magnitude = add_word(a.magnitude, loss);
            }
        }
}
Word city_removal_commit(Actor &a, Rng &rng) {
    city_index(a.target_city); // reject host out-of-bounds before any mutation/draw
    a.action = 0x11;
    Word message = Word(rng.next() % 3 + 1);
    a.population[a.target_city] = 0;
    return message;
}
Word quake_effect_notice(Actor &a, Rng &rng) {
    a.action = 0x56;
    return Word(rng.next() % 3 + 1);
}
Word weight_drop_commit(Actor &a, Rng &rng) {
    city_index(a.target_city);
    a.action = 0x12;
    Word message = Word(rng.next() % 3 + 1);
    a.population[a.target_city] = 0;
    return message;
}
CowLoss cow_population_commit(State &s, std::size_t i, Rng &rng) {
    actor_index(i);
    auto &a = s.actors[i];
    actor_index(a.target_actor);
    city_index(a.target_city);
    Word loss = a.magnitude;
    a.action = 0x13;
    auto &population = s.actors[a.target_actor].population[a.target_city];
    if (signed_word(loss) > signed_word(population))
        loss = population;
    Word message = Word(rng.next() % 5 + 1);
    population = sub_word(population, loss);
    return {loss, message};
}
void transfer_arrival_commit(State &s, std::size_t i) {
    actor_index(i);
    auto &a = s.actors[i];
    actor_index(a.target_actor);
    city_index(a.target_city);
    Word magnitude = a.magnitude;
    a.action = 0x14;
    auto &population = s.actors[a.target_actor].population[a.target_city];
    population = add_word(population, magnitude);
}
TransferPrelude transfer_event_prelude(State &s, std::size_t i, Word roll, Rng &rng) {
    actor_index(i);
    if (roll != 24 && roll != 25)
        throw Unsupported("transfer prelude requires selected second roll24 or25");
    auto copy = s;
    auto random = rng;
    auto &a = copy.actors[i];
    TransferPrelude report;
    // 102df4 forces profile1 for a scan of DATA-tail words at index24/25.
    // That bounded read-only scan makes no RNG calls; its result at -12(A5)
    // is overwritten at 102e30 before use. Elide it rather than invent RAM.
    // 102e1a restores the ORIGINAL profile before this actual source choice.
    const Word profile = a.profile;
    int source = choose_city(a, a, random);
    if (profile == 3 || profile == 4)
        ++report.draws; // existing helper draws exactly once
    if (source < 0)
        throw Unsupported("transfer source city would address outside modeled population array");
    city_index(std::size_t(source));
    report.source_city = Word(source);
    auto divisor = signed_word(a.population[std::size_t(source)]);
    if (divisor == 0)
        throw Unsupported("transfer DIVS.W would divide by zero");
    auto draw = [&]() {
        ++report.draws;
        return random.next();
    };
    // DIVS.W remainder has the nonnegative dividend's sign, even for a
    // negative source word. ADDQ.W can produce magnitude0x8000.
    report.magnitude = Word(std::int32_t(draw()) % divisor + 1);
    a.magnitude = report.magnitude;
    a.population[std::size_t(source)] = sub_word(a.population[std::size_t(source)], a.magnitude);
    a.action = 14;
    if (!living_actors(copy))
        throw Unsupported("transfer recipient scan would never terminate after source deduction");
    Word destination = Word(i);
    while (destination == i) {
        destination = Word(draw() % 5);
        ++report.candidate_draws;
    }
    // The original only excludes self in the RNG loop. The subsequent
    // deterministic scan can wrap back to the acting actor (102ed0..102ef0).
    while (!populated_cities(copy.actors[destination]))
        destination = Word((destination + 1) % 5);
    a.target_actor = destination;
    a.profile = 1;
    int city = choose_city(a, copy.actors[destination], random); // fixed profile1: no draw
    a.profile = profile;
    if (city < 0)
        throw Unsupported("transfer destination city unavailable after living-actor scan");
    city_index(std::size_t(city));
    a.target_city = Word(city);
    report.destination_actor = destination;
    report.destination_city = Word(city);
    s = copy;
    rng = random;
    return report;
}
Action3Prelude apply_action3_prelude(State &s, std::size_t i, Rng &rng) {
    actor_index(i);
    State copy = s;
    Rng random = rng;
    Action3Prelude result;
    result.decision = decide_action3_substitution(copy.actors[i].action, random);
    result.draws = result.decision.draws;
    if (result.decision.substituted) {
        if (result.decision.selected_action == 14) {
            result.draws +=
                transfer_event_prelude(copy, i, result.decision.event_roll, random).draws;
        } else if (result.decision.selected_action == 86) {
            quake_event_prelude(copy, i, random);
            ++result.draws;
        } else
            copy.actors[i].action = result.decision.selected_action;
    }
    s = copy;
    rng = random;
    return result;
}
PropagandaTransfer propaganda_commit(State &s, std::size_t i, Rng &rng) {
    actor_index(i);
    const auto target = s.actors[i].target_actor, source = s.actors[i].target_city;
    actor_index(target);
    city_index(source);
    State copy = s;
    Rng random = rng;
    auto &a = copy.actors[i];
    auto &population = copy.actors[target].population[source];
    Word loss = a.magnitude;
    if (signed_word(loss) > signed_word(population)) {
        loss = population;
        a.magnitude = loss;
    }
    apply_relationship_event(copy, i, 2, target, loss);
    population = sub_word(population, loss);
    if (i == 4)
        a.profile = 2;
    int destination = choose_city(a, a, random);
    if (destination < 0)
        throw Unsupported("propaganda own destination city undefined");
    if (i == 4)
        a.profile = 0;
    city_index(std::size_t(destination));
    a.population[std::size_t(destination)] = add_word(a.population[std::size_t(destination)], loss);
    PropagandaTransfer report{loss, source, Word(destination),
                              Word(i != 4 && (a.profile == 3 || a.profile == 4))};
    s = copy;
    rng = random;
    return report;
}
std::size_t defection_commit(State &s, std::size_t i, Rng &rng) {
    actor_index(i);
    auto &a = s.actors[i];
    const std::size_t t = a.target_actor, c = a.target_city;
    actor_index(t);
    city_index(c);
    int source = choose_city(a, a, rng);
    if (source < 0)
        throw Unsupported("effect15 source city would be out of bounds");
    auto source_city = std::size_t(source);
    Word loss = a.magnitude;
    if (signed_word(loss) > signed_word(a.population[source_city])) {
        loss = a.population[source_city];
        a.magnitude = loss;
    }
    a.population[source_city] = sub_word(a.population[source_city], loss);
    s.actors[t].population[c] = add_word(s.actors[t].population[c], loss);
    return source_city;
}
void apply_special_event_effect(State &s, std::size_t i, Word effect, Rng &rng) {
    actor_index(i);
    State copy = s;
    Rng random = rng;
    auto &a = copy.actors[i];
    switch (effect) {
    case 0x0d:
        propaganda_commit(copy, i, random);
        break;
    case 0x0f:
        defection_commit(copy, i, random);
        break;
    case 0x10:
        growth_commit(a, a.target_city);
        break;
    case 0x11:
        city_removal_commit(a, random);
        break;
    case 0x12:
        weight_drop_commit(a, random);
        break;
    case 0x13:
        cow_population_commit(copy, i, random);
        break;
    case 0x14:
        transfer_arrival_commit(copy, i);
        break;
    case 0x15:
        radiation_population_commit(a, random);
        break;
    case 0x45:
        double_largest_city(a);
        break;
    case 0x56:
        quake_effect_notice(a, random);
        break;
    default:
        throw Unsupported("special event effect outside reconstructed dispatch domain");
    }
    s = copy;
    rng = random;
}
std::array<Word, 4> human_entry_rolls(Rng &rng) {
    std::array<Word, 4> rolls{};
    for (auto &roll : rolls)
        roll = Word(rng.next() % 107 + 1);
    return rolls;
}
AttitudeRefreshRng attitude_refresh_rng(const State &s, Rng &rng) {
    AttitudeRefreshRng result;
    for (std::size_t i = 0; i < 4; ++i) {
        const auto &a = s.actors[i];
        if (populated_cities(a) == 0)
            continue;
        auto bucket = signed_word(relationship_bucket(a.scores[4]));
        if (a.profile == 4)
            bucket = -bucket;
        if (bucket < 0)
            continue;
        Word first = Word(rng.next() % 150);
        result.random_periods[i] = Word(first + rng.next() % 150 + 2);
        result.randomized[i] = true;
        result.draws = add_word(result.draws, 2);
    }
    return result;
}
MessageRng advance_message_rng(std::string_view text, Word x, Word y, Rng &rng) {
    auto nul = text.find(char(0));
    if (nul != std::string_view::npos)
        text = text.substr(0, nul);
    if (text.size() > 32767)
        throw Unsupported("message length outside verified signed-word loop domain");
    MessageRng report;
    report.noise_calls = std::uint32_t(x != 78 || y != 182);
    for (unsigned char c : text)
        if (c != 0x20)
            ++report.noise_calls;
    report.draws = report.noise_calls * 128;
    for (std::uint32_t draw = 0; draw < report.draws; ++draw)
        rng.next();
    return report;
}
MessageRng advance_message_sequence(MessageSequenceRng &state, std::string_view text, Rng &rng) {
    // Only equality with(78,182) is observable to the RNG branch. For every
    // completed message, Move baseline182 then cp_y-8 proves inequality.
    auto result = advance_message_rng(text, 78, state.at_initial_origin ? 182 : 174, rng);
    state.at_initial_origin = false;
    return result;
}
std::vector<std::string> production_summary_messages(const Production &production) {
    unsigned total = 0;
    for (auto count : production.missiles)
        total += count;
    for (auto count : production.warheads)
        total += count;
    for (auto count : production.bombers)
        total += count;
    for (auto count : production.defenses)
        total += count;
    if (total > 6)
        throw Unsupported("production summary outside original at-most6-items domain");
    std::vector<std::string> lines;
    auto count_text = [](Word count) { return std::string("0") + std::to_string(count); };
    auto suffix = [](Word count) { return count > 1 ? "s" : " "; };
    for (std::size_t c = 0; c < 4; ++c) {
        std::string capacity = std::to_string(class_capacity(Word(c)));
        if (capacity.size() < 3)
            capacity = " " + capacity;
        if (production.missiles[c] != 0)
            lines.push_back(count_text(production.missiles[c]) + " " + capacity + "MT Missile" +
                            suffix(production.missiles[c]));
        if (production.warheads[c] != 0)
            lines.push_back(count_text(production.warheads[c]) + " " + capacity + "MT Warhead" +
                            suffix(production.warheads[c]));
        if (c < 2) {
            if (production.bombers[c] != 0)
                lines.push_back(count_text(production.bombers[c]) + "  " + (c == 0 ? "NP" : "GR") +
                                "-" + std::to_string(c + 1) + " Bomber" +
                                suffix(production.bombers[c]));
            if (production.defenses[c] != 0)
                lines.push_back(count_text(production.defenses[c]) + "  " +
                                (c == 0 ? "L.N.D.S." : "MegaCannon") +
                                (production.defenses[c] > 1 ? "'s" : "  "));
        }
    }
    return lines;
}
HumanProduction produce_human(State &s, MessageSequenceRng &context, Rng &rng) {
    State copy = s;
    MessageSequenceRng cursor = context;
    Rng random = rng;
    Word items = Word(populated_cities(copy.actors[4]) + 1);
    Word variant = Word(random.next() % 3 + 1);
    HumanProduction report;
    report.total_draws = 1;
    report.messages.push_back(variant == 3 ? "Your citizens made:" : "Your factories produced:");
    report.total_draws += advance_message_sequence(cursor, report.messages.back(), random).draws;
    report.inventory = produce_items(copy.actors[4], items, random);
    report.inventory.human_message_variant = variant;
    report.total_draws += report.inventory.draws;
    report.inventory.draws = add_word(report.inventory.draws, 1);
    auto summaries = production_summary_messages(report.inventory);
    for (const auto &line : summaries) {
        report.total_draws += advance_message_sequence(cursor, line, random).draws;
        report.messages.push_back(line);
    }
    s = copy;
    context = cursor;
    rng = random;
    return report;
}
HumanEntryPrefix prepare_human_entry_prefix(State &s, MessageSequenceRng &context, Rng &rng) {
    State copy = s;
    MessageSequenceRng cursor = context;
    Rng random = rng;
    HumanEntryPrefix report;
    report.entry_rolls = human_entry_rolls(random);
    report.total_draws = 4;
    report.first_refresh = attitude_refresh_rng(copy, random);
    report.total_draws += report.first_refresh.draws;
    report.empty_message = advance_message_sequence(cursor, "", random);
    report.total_draws += report.empty_message.draws;
    report.second_refresh = attitude_refresh_rng(copy, random);
    report.total_draws += report.second_refresh.draws;
    if (copy.actors[4].action == 6) {
        report.production = produce_human(copy, cursor, random);
        report.produced = true;
        report.total_draws += report.production.total_draws;
    }
    s = copy;
    context = cursor;
    rng = random;
    return report;
}
bool select_human_delivery_fields(State &s, HumanSelection &selection, Word action, Word choice) {
    auto &human = s.actors[4];
    Word selected = 0, capacity = selection.selected_capacity;
    if (action == 2) {
        if (selection.selected_class == 0xffff)
            return false;
        if (choice > 4)
            throw Unsupported("human missile choice outside0..4");
        if (choice == 0) {
            int highest = highest_missile(human);
            if (highest < 0)
                throw Unsupported("human auto missile path uses absent inventory index");
            selected = Word(highest);
        } else {
            selected = sub_word(choice, 1);
            if (human.missiles[selected] == 0)
                return false;
        }
        capacity = class_capacity(selected);
    } else if (action == 4 || action == 7) {
        const auto &inventory = action == 4 ? human.bombers : human.defenses;
        if (inventory[0] == 0 && inventory[1] == 0)
            return false;
        if (choice > 2)
            throw Unsupported("human binary delivery choice outside0..2");
        if (choice == 0)
            selected = Word(signed_word(inventory[1]) > 0);
        else {
            selected = sub_word(choice, 1);
            if (inventory[selected] == 0)
                return false;
        }
        if (action == 4)
            capacity = Word(selected * 50 + 50);
    } else
        throw Unsupported("human delivery fields require action2,4 or7");
    selection.selected_class = selected;
    selection.selected_capacity = capacity;
    selection.pending_action = action;
    if (action == 2)
        human.magnitude = 2;
    if (action == 7)
        human.magnitude = selected;
    return true;
}
HumanWarheadCheck select_human_warhead_fields(const State &s, HumanSelection &selection,
                                              Word choice, Word target_marker) {
    const auto &human = s.actors[4];
    if (choice > 4)
        throw Unsupported("human warhead choice outside0..4");
    if (choice == 0)
        selection.selected_class = Word(highest_warhead(human));
    else {
        Word index = sub_word(choice, 1);
        if (human.warheads[index] == 0)
            return HumanWarheadCheck::no_warhead;
        selection.selected_class = index;
    }
    if (selection.selected_class == 0xffff)
        return HumanWarheadCheck::no_warhead;
    selection.selected_capacity = class_capacity(selection.selected_class);
    if (human.action == 4 || human.action == 5) {
        if (s.human_prepared_capacity == 0)
            return HumanWarheadCheck::no_capacity;
        if (signed_word(s.human_prepared_capacity) < signed_word(selection.selected_capacity))
            return HumanWarheadCheck::too_large;
    }
    if (human.action == 2 &&
        signed_word(s.human_prepared_capacity) < signed_word(selection.selected_capacity))
        return HumanWarheadCheck::too_large;
    if (human.action != 4 && human.action != 5 && human.action != 2)
        return HumanWarheadCheck::no_prepared_delivery;
    if (target_marker == 0xffff)
        return HumanWarheadCheck::no_target;
    return HumanWarheadCheck::accepted;
}
void finish_human_attack_fields(State &s, HumanSelection &selection) {
    auto &human = s.actors[4];
    if (selection.selected_class >= 4)
        throw Unsupported("human attack class outside0..3");
    if (human.action != 2 && human.action != 4 && human.action != 5)
        throw Unsupported("human attack final fields require prepared delivery");
    selection.pending_action = human.action == 2 ? 1 : 5;
    human.magnitude = class_effect(selection.selected_class);
}
bool begin_human_propaganda_fields(HumanSelection &selection, Word target) noexcept {
    selection.pending_action = 3;
    return target != 0xffff;
}
Word roll_human_propaganda_fields(State &s, HumanSelection &selection, Rng &rng) {
    if (selection.pending_action != 3)
        throw Unsupported("human propaganda direct rolls require pending3");
    selection.selected_class = Word(rng.next() % 4);
    selection.selected_capacity = class_capacity(selection.selected_class);
    Word discarded = Word(rng.next() % 100 + 1);
    s.actors[4].magnitude = Word(rng.next() % 10 + 1);
    return discarded;
}
bool human_confirmation_ready(Word pending, Word target) noexcept {
    if (pending == 0)
        return false;
    if ((pending == 1 || pending == 3 || pending == 5) && target == 0xffff)
        return false;
    return true;
}
bool confirm_human_selection_fields(State &s, const HumanSelection &selection, Word target) {
    if (!human_confirmation_ready(selection.pending_action, target))
        return false;
    human_commit(s, selection.pending_action, selection.selected_class,
                 selection.selected_capacity);
    return true;
}
void select_human_production_fields(HumanSelection &selection) noexcept {
    selection.pending_action = 6;
}
std::uint8_t cycle_human_classification(State &s, std::size_t target) {
    if (target >= 4)
        throw Unsupported("human attitude key target outside0..3");
    auto &value = s.actors[4].classifications[target];
    value = std::uint8_t(unsigned(value) - 1u);
    if (signed_byte(value) < -2)
        value = 2;
    return value;
}
Word human_menu_control(Word code) noexcept {
    if (code == 0xffff || (code & 0x1fu) != 0)
        return 0;
    Word menu = Word((code >> 5) & 0x3fu);
    return menu < 2 ? add_word(menu, 1) : Word(0);
}
bool select_human_target_fields(State &s, Word viewed, Word city) {
    actor_index(viewed);
    if (viewed == 4)
        return false;
    city_index(city);
    s.actors[4].target_actor = viewed;
    s.actors[4].target_city = city;
    return true;
}
void human_commit(State &s, Word pending, Word selected, Word capacity) {
    if (pending > 7)
        throw Unsupported("human commit only verified ordinary action codes 0..7");
    if (((pending == 4 || pending == 7) && selected >= 2) ||
        ((pending == 1 || pending == 2 || pending == 5) && selected >= 4))
        throw Unsupported("human commit inventory index outside verified array");
    auto &a = s.actors[4];
    bool penalty = (pending == 6 && a.action == 6) || (a.action == 2 && pending != 1) ||
                   (a.action == 4 && pending != 5);
    if (penalty)
        for (std::size_t i = 0; i < 4; ++i) {
            auto &scores = s.actors[i].scores;
            scores[4] =
                signed_word(scores[4]) < 0x4df8 ? add_word(scores[4], 0x3332) : Word(0x7fff);
            for (std::size_t j = 0; j < 4; ++j)
                scores[j] =
                    signed_word(scores[j]) <= -0x4df8 ? Word(0x8001) : sub_word(scores[j], 0x3332);
        }
    a.action = pending;
    if (pending == 7)
        a.defenses[selected] = sub_word(a.defenses[selected], 1);
    if (pending == 4) {
        a.bombers[selected] = sub_word(a.bombers[selected], 1);
        s.human_prepared_capacity = capacity;
    }
    if (pending == 2) {
        a.missiles[selected] = sub_word(a.missiles[selected], 1);
        s.human_prepared_capacity = capacity;
    }
    if (pending == 1 || pending == 5) {
        a.warheads[selected] = sub_word(a.warheads[selected], 1);
        s.human_prepared_capacity = sub_word(s.human_prepared_capacity, capacity);
    }
}
AlienPrelude alien_prelude(Actor &a, Rng &rng) {
    AlienPrelude p;
    p.message_variant = Word(rng.next() % 2 + 1);
    p.loss = Word(rng.next() % 10 + 1);
    a.magnitude = p.loss;
    p.initial_x = Word(rng.next() % 200 + 0x33);
    return p;
}
EventInitialization initialize_event_handler(Actor &a, Rng &rng) {
    Actor copy = a;
    Rng random = rng;
    EventInitialization result;
    switch (copy.action) {
    case 3:
        result.effect = 13;
        result.message_variant = Word(random.next() % 4 + 1);
        result.draws = 1;
        break;
    case 8:
        result.effect = 15;
        break;
    case 9:
        result.effect = 16;
        result.message_variant = growth_handler_initialize(copy, random);
        result.draws = 2;
        break;
    case 11:
    case 12:
        result.effect = Word(copy.action + 6);
        result.message_variant = city_loss_handler_initialize(copy, copy.action, random);
        result.draws = 1;
        break;
    case 13:
        result.effect = 19;
        result.message_variant = growth_handler_initialize(copy, random);
        result.draws = 2;
        break;
    case 14:
        result.effect = 20;
        result.message_variant = Word(random.next() % 4 + 1);
        result.draws = 1;
        break;
    case 15:
        result.effect = 21;
        result.message_variant = Word(random.next() % 3 + 1);
        result.draws = 1;
        break;
    case 16: {
        auto alien = alien_prelude(copy, random);
        result.effect = 69;
        result.message_variant = alien.message_variant;
        result.alien_initial_x = alien.initial_x;
        result.draws = 3;
        break;
    }
    case 86:
        result.effect = 86;
        result.message_variant = quake_handler_initialize(copy, random);
        result.draws = 1;
        break;
    default:
        throw Unsupported(
            "special handler initial-frame gameplay not reconstructed for this action");
    }
    a = copy;
    rng = random;
    return result;
}
EventInitialization initialize_preparation_handler(State &s, std::size_t i, Rng &rng) {
    actor_index(i);
    State copy = s;
    Rng random = rng;
    Word action = copy.actors[i].action;
    if (action != 2 && action != 4 && action != 6 && action != 7)
        throw Unsupported("preparation initialization requires action2,4,6 or7");
    if (action == 6)
        apply_relationship_event(copy, i, 5, Word(i), 0);
    if (action == 7)
        apply_relationship_event(copy, i, 4, Word(i), 0);
    EventInitialization result;
    result.effect = action == 2 ? 16 : 255;
    result.message_variant = Word(random.next() % (action == 6 ? 5 : 4) + 1);
    result.draws = 1;
    s = copy;
    rng = random;
    return result;
}
Dispatch action_dispatch(Word a) noexcept {
    switch (a) {
    case 1:
        return {a, 0x117576};
    case 2:
        return {a, 0x1128e8};
    case 3:
        return {a, 0x116c84};
    case 4:
        return {a, 0x1169f6};
    case 5:
        return {a, 0x1164dc};
    case 6:
        return {a, 0x11729a};
    case 7:
        return {a, 0x112c42};
    case 8:
        return {a, 0x110cb2};
    case 9:
        return {a, 0x1119b8};
    case 11:
        return {a, 0x111f62};
    case 12:
        return {a, 0x117c22};
    case 13:
        return {a, 0x11145c};
    case 14:
        return {a, 0x110f64};
    case 15:
        return {a, 0x116f9a};
    case 16:
        return {a, 0x117f48};
    case 0x56:
        return {a, 0x11245e};
    default:
        return {a, 0};
    }
}
void resolve_action(State &, std::size_t, Rng &) {
    throw Unsupported("TODO complete two-pass animation/collision resolver 1028c8");
}
void human_turn(State &) { throw Unsupported("TODO complete human command transitions 106712"); }
} // namespace nuclear
