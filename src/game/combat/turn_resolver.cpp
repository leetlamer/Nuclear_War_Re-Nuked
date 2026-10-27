#include "nuclear/resolver_core.hpp"
namespace nuclear::resolver {
namespace {
void actor_index(std::size_t actor) {
    if (actor >= actor_count)
        throw Unsupported("resolver actor outside0..4");
}
} // namespace
EffectResult effect_marker(State &state, std::size_t actor, Word selector, Word coordinate,
                           Word applied, Context context, Rng &rng) {
    const auto marker = consume_effect_marker(coordinate, applied);
    EffectResult out;
    out.context = context;
    out.coordinate = marker.frame_coordinate;
    out.applied = marker.effect_applied;
    out.dispatched = marker.dispatch_effect;
    out.seed_before = rng.seed();
    out.seed_after = rng.seed();
    if (!marker.dispatch_effect)
        return out; // unused targets/actor not accessed
    actor_index(actor);
    auto copy = state;
    auto random = rng;
    auto &a = copy.actors[actor];
    out.context.magnitude = a.magnitude; // 103858..103868, before selector dispatch
    switch (selector) {
    case 0x0d:
    case 0x0f: {
        // Helpers use Actor targets; this staged, private substitution supplies
        // original stack latches. Neither helper/choose_city/relationship reads
        // those fields for any other purpose or writes them. Restore originals.
        const auto target = a.target_actor, city = a.target_city;
        a.target_actor = context.target_actor;
        a.target_city = context.target_city;
        if (selector == 0x0d) {
            const auto report = propaganda_commit(copy, actor, random);
            out.context.magnitude = report.loss;
            out.chosen_city = report.destination_city;
        } else {
            out.chosen_city = Word(defection_commit(copy, actor, random));
            out.context.magnitude = a.magnitude;
        }
        a.target_actor = target;
        a.target_city = city;
        break;
    }
    case 0x10:
        growth_commit(a, a.target_city);
        out.context.target_actor = Word(actor);
        out.context.target_city = a.target_city;
        break;
    case 0x11:
        out.message_variant = city_removal_commit(a, random);
        out.context.target_actor = Word(actor);
        out.context.target_city = a.target_city;
        break;
    case 0x12:
        out.message_variant = weight_drop_commit(a, random);
        out.context.target_actor = Word(actor);
        out.context.target_city = a.target_city;
        break;
    case 0x13: {
        const auto report = cow_population_commit(copy, actor, random);
        out.context.target_actor = a.target_actor;
        out.context.target_city = a.target_city;
        out.context.magnitude = report.loss;
        out.message_variant = report.message_variant;
        break;
    }
    case 0x14:
        transfer_arrival_commit(copy, actor);
        out.context.target_actor = a.target_actor;
        out.context.target_city = a.target_city;
        break;
    case 0x15:
        radiation_population_commit(a, random);
        out.context.target_actor = a.target_actor;
        out.context.target_city = a.target_city;
        out.context.magnitude = a.magnitude;
        break;
    case 0x45:
        out.chosen_city = Word(double_largest_city(a));
        out.context.target_actor = Word(actor);
        out.context.target_city = *out.chosen_city;
        out.context.magnitude = a.magnitude;
        break;
    case 0x56:
        out.message_variant = quake_effect_notice(a, random);
        break;
    default:
        throw Unsupported("effect selector outside verified context dispatcher");
    }
    out.context.suppression = 0; // explicit CLR.W f4 in all ten branches
    out.seed_after = random.seed();
    state = copy;
    rng = random;
    return out;
}
ImpactResult primary_impact(State &state, std::size_t actor, Word marker, const ImpactState &local,
                            Rng &rng) {
    ImpactResult out;
    out.local = local;
    out.seed_before = rng.seed();
    out.seed_after = rng.seed();
    const auto route =
        classify_resolver_frame(marker, 0, local.primary_processed, local.completion);
    out.route = route.route;
    out.local.completion = route.completion_counter;
    if (route.route != ResolverFrameRoute::prepare_impact)
        return out;
    actor_index(actor);
    if (state.actors[actor].action != 1 && state.actors[actor].action != 5)
        throw Unsupported("primary impact requires established ordinary missile/bomber path");
    if (local.selector < 2 || local.selector > 13)
        throw Unsupported("inactive/special impact selector requires unimplemented interpreter");
    auto copy = state;
    auto random = rng;
    const auto combat =
        evaluate_combat_impact(copy, actor, local.context.target_actor, local.context.target_city,
                               local.selector, local.magnitude_was_25, random);
    out.local.primary_processed = 1;
    out.local.pending = route.pending_value;
    out.local.selector = combat.effect;
    out.local.context.magnitude = combat.damage.loss;
    out.local.context.suppression = 0; // 104d2a CLR.W f4 after magnitude store
    out.combat = combat;
    out.seed_after = random.seed();
    state = copy;
    rng = random;
    return out;
}
CommitResult pending_commit(State &state, std::size_t actor, const ImpactState &local,
                            Word primary_status, bool final_gate) {
    auto copy = state;
    CommitResult out;
    out.local = local;
    out.commit =
        commit_pending_combat(copy, actor, local.context.target_actor, local.context.target_city,
                              out.local.pending, primary_status, final_gate);
    // 10512a/105172 (and early equivalents) reload/cap stack f6.
    if (out.commit.committed)
        out.local.context.magnitude = out.commit.loss;
    state = copy;
    return out;
}
Word Body::execute(session::Data &, const session::ResolverRequest &, session::CallbackSchedule &) {
    throw Unsupported(
        "complete resolver body: native schedule/FFP and undefined follow-up guard unresolved");
}
Word Boundary::resolve(session::Data &data, const session::ResolverRequest &request,
                       session::CallbackSchedule &callbacks) {
    if (request.actor_order != std::array<std::size_t, 5>{{0, 1, 2, 3, 4}})
        throw Unsupported("resolver actor order must be0,1,2,3,4");
    if (request.entry.pass != 0 || request.entry.control_return != 0 ||
        request.entry.human_cities_before != populated_cities(data.state.actors[4]) ||
        data.rng.seed() != request.microseconds_low16)
        throw Unsupported("resolver boundary requires original already-reseeded entry");
    auto staged = data;
    const auto result = body_.execute(staged, request, callbacks);
    data = std::move(staged);
    return result;
}
} // namespace nuclear::resolver
