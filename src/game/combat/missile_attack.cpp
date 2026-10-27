#include "nuclear/single_missile.hpp"
namespace nuclear::resolver {
MissilePrefix begin_committed_missile(State &state, std::size_t actor, Rng &rng) {
    if (actor >= actor_count)
        throw Unsupported("missile actor outside0..4");
    const auto &a = state.actors[actor];
    if (a.action != 1 || populated_cities(a) == 0)
        throw Unsupported("prefix requires living actor with already-committed missile action1");
    if (a.target_actor >= actor_count || a.target_city >= city_count)
        throw Unsupported("missile target index outside original modeled arrays");
    auto copy = state;
    auto random = rng;
    MissilePrefix out;
    out.seed_before = rng.seed();
    out.start = start_resolver_action(copy, actor, 0, random);
    constexpr std::array<Word, 5> launch_x{{57, 44, 260, 270, 143}}; // 131214
    constexpr std::array<Word, 5> launch_y{{48, 152, 55, 150, 101}}; // 13121e
    out.launch_x = launch_x[actor];
    out.launch_y = launch_y[actor];
    out.target_x = add_word(copy.actors[a.target_actor].city_x[a.target_city], 8);
    out.target_y = sub_word(copy.actors[a.target_actor].city_y[a.target_city], 16);
    out.right = Word(signed_word(out.launch_x) < signed_word(out.target_x));
    Word distance =
        out.right ? sub_word(out.target_x, out.launch_x) : sub_word(out.launch_x, out.target_x);
    out.long_path = Word(signed_word(distance) > 50);
    // Original ASR.W3, including negative coordinate behavior; no signed shift.
    const auto x = signed_word(out.launch_x);
    const Word shifted = Word(x >= 0 ? x / 8 : -((-x + 7) / 8));
    const Word offset = add_word(Word(std::uint32_t(out.launch_y) * 40), shifted);
    out.writes = {{0x131a00, 0}, // 11758a done BEFORE dice, not an arrival marker
                  {0x131a06, out.launch_x},
                  {0x131a08, out.launch_y},
                  {0x1319f8, out.target_x},
                  {0x1319fa, out.target_y},
                  {0x131a0a, out.right},
                  {0x131a14, out.long_path},
                  {0x1319f6, out.launch_y},
                  {0x1319f4, out.launch_x},
                  {0x131a18, out.right},
                  {0x132404, out.launch_y},
                  {0x132454, 240},
                  {0x132418, 240},
                  {0x13247c, 240},
                  {0x1323f0, out.launch_x},
                  {0x1324f4, out.launch_x},
                  {0x1323dc, offset},
                  {0x1324e0, offset},
                  {0x13242c, offset},
                  {0x132440, 7},
                  {0x132468, 7}};
    // No current-x131a0c, FFP-x131a10, control131a16, widths/heights or
    // later frame-latched values are initialized here: their writes follow FFP.
    out.seed_after = random.seed();
    state = copy;
    rng = random;
    return out;
}
[[noreturn]] void advance_missile(const MissilePrefix &, MissileEvent) {
    throw Unsupported("11780a/23680a trajectory setup1059e0 reaches unimplemented FFP105a26; "
                      "arrival/ack/exit not proven");
}
PopulationWrite DamageAcknowledgment::write_at_gate(State &state, Word status, bool final_gate) {
    if (phase_ != DamagePhase::awaiting_gate)
        throw Unsupported("damage population write already reached; cannot repeat pending commit");
    if (local_.pending == 0 || (!final_gate && status != 196))
        return {};
    if (actor_ >= actor_count || local_.context.target_actor >= actor_count ||
        local_.context.target_city >= city_count)
        throw Unsupported("reached pending damage indexes outside original modeled arrays");
    auto staged = state;
    auto next = local_;
    auto &attacker = staged.actors[actor_];
    auto &population =
        staged.actors[next.context.target_actor].population[next.context.target_city];
    Word loss = attacker.magnitude; // CURRENT actor, not earlier context magnitude
    if (signed_word(loss) > signed_word(population)) {
        loss = population;
        attacker.magnitude = loss;
    }
    population = sub_word(population, loss);
    next.context.magnitude = loss;
    next.pending = 0;
    // Stop BEFORE native render ack and relationship0. Callback state not guessed.
    state = staged;
    local_ = next;
    phase_ = DamagePhase::awaiting_render_ack;
    return {true, loss, 1, 1};
}
bool DamageAcknowledgment::observe_render_request(State &state, Word observed) {
    if (phase_ != DamagePhase::awaiting_render_ack)
        throw Unsupported("render acknowledgment is not pending for this population write");
    if (observed != 0)
        return false;
    auto staged = state;
    apply_relationship_event(staged, actor_, 0, local_.context.target_actor,
                             local_.context.magnitude);
    state = staged;
    phase_ = DamagePhase::relationship_complete;
    return true;
}
} // namespace nuclear::resolver
