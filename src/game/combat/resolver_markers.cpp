#include "nuclear/resolver_markers.hpp"

namespace nuclear {
ResolverEntryState begin_resolver_entry(const State& state, Word time_word, Rng& rng) {
    ResolverEntryState entry;
    entry.human_cities_before=populated_cities(state.actors[4]);
    rng.reseed_word(time_word);
    return entry;
}
EffectMarkerResult consume_effect_marker(Word coordinate, Word applied) noexcept {
    if ((coordinate & 0x0400u) == 0) return {coordinate, applied, false};
    coordinate = sub_word(coordinate, 0x0400u);
    if (applied != 0) return {coordinate, applied, false};
    return {coordinate, 1, true};
}
Word resolver_frame_slots(Word selector) noexcept {
    switch (selector) {
    case 0x19: case 0x56: case 0x10: case 0x13: case 0x11: case 0x14:
        return 2;
    default: return 1;
    }
}
ResolverFrameDecision classify_resolver_frame(Word marker, Word slot,
    Word processed, Word counter) {
    if (marker == 0x55f0) return {ResolverFrameRoute::end_frame, counter, 0};
    if (marker != 0x5208) return {ResolverFrameRoute::ordinary, counter, 0};
    counter = sub_word(counter, 1);
    if (signed_word(slot) >= 5)
        return {ResolverFrameRoute::secondary_slot_cleanup, counter, 0};
    if (signed_word(slot) < 0)
        throw Unsupported("resolver marker negative slot indexes outside the array");
    if (processed != 0) return {ResolverFrameRoute::processed_slot, counter, 0};
    return {ResolverFrameRoute::prepare_impact, counter, 15};
}
ResolverActorDecision classify_resolver_actor(const State& state,std::size_t actor,Word pass) {
    if (actor>=actor_count) throw Unsupported("resolver actor outside0..4");
    Word total=0;
    for (const auto& a:state.actors) total=add_word(total,populated_cities(a));
    if (total==0) return {ResolverActorRoute::no_cities,1,0,0};
    bool alive=populated_cities(state.actors[actor])!=0;
    if (pass==0) {
        if (!alive) return {ResolverActorRoute::skip_actor,pass,total,0};
        return {ResolverActorRoute::ordinary_action,pass,total,4};
    }
    if (state.pass_processed[actor]!=0 || alive)
        return {ResolverActorRoute::skip_actor,pass,total,0};
    return {ResolverActorRoute::followup_requires_evidence,pass,total,0};
}
bool resolver_pass_has_finished(Word pass) noexcept { return signed_word(pass)>1; }
ResolverPassDecision advance_resolver_pass(Word pass,Word repeat,Word abort_control) {
    if (resolver_pass_has_finished(pass)) throw Unsupported("resolver pass already exited before actor phase");
    if (abort_control!=0) return {ResolverPassRoute::production_phase,pass,abort_control};
    if (repeat!=0) return {ResolverPassRoute::repeat_pass,pass,0};
    pass=add_word(pass,1);
    return {resolver_pass_has_finished(pass)?ResolverPassRoute::production_phase:ResolverPassRoute::begin_next_pass,pass,0};
}
ResolverActionPrelude prepare_resolver_action_prelude(State& state,
    std::size_t actor, Word pass, Rng& rng) {
    ResolverActionPrelude report;
    report.eligibility=classify_resolver_actor(state,actor,pass);
    if(report.eligibility.route==ResolverActorRoute::followup_requires_evidence)
        throw Unsupported("10c0ce follow-up reads uninitialized local; no speculative action");
    if(report.eligibility.route!=ResolverActorRoute::ordinary_action) return report;
    auto copy=state; auto random=rng;
    report.magnitude_was_25=copy.actors[actor].magnitude==25;
    report.substitution=apply_action3_prelude(copy,actor,random);
    report.draws=report.substitution.draws;
    report.magnitude_before_handler=copy.actors[actor].magnitude;
    state=copy; rng=random; return report;
}
ResolverActionStart start_resolver_action(State& state, std::size_t actor, Word pass, Rng& rng) {
    auto copy=state; auto random=rng; ResolverActionStart report;
    report.prelude=prepare_resolver_action_prelude(copy,actor,pass,random);
    if(report.prelude.eligibility.route!=ResolverActorRoute::ordinary_action) return report;
    const Word action=copy.actors[actor].action;
    report.dispatch=action_dispatch(action);
    switch(action) {
    case 1: case 5: {
        const auto roll=begin_attack_roll(action,random);
        report.handler.effect=roll.effect; report.handler.message_variant=roll.message_variant;
        report.handler.draws=roll.draws; break;
    }
    case 2: case 4: case 6: case 7:
        report.handler=initialize_preparation_handler(copy,actor,random); break;
    case 3: case 8: case 9: case 11: case 12: case 13: case 14: case 15: case 16: case 86:
        report.handler=initialize_event_handler(copy.actors[actor],random); break;
    default: throw Unsupported("no verified framezero handler for ordinary action");
    }
    report.initialized=true; report.draws=report.prelude.draws+report.handler.draws;
    state=copy; rng=random; return report;
}
ResolverFrameTail resolver_frame_tail_step(Word frame, Word reverse,
    Word completion, Word frameflag_f4) noexcept {
    ResolverFrameTail result;
    result.set_render_request=frameflag_f4==0;
    result.next_frame=reverse!=0?sub_word(frame,1):add_word(frame,1);
    result.repeat_handler=completion!=0 && signed_word(result.next_frame)<800 && result.next_frame!=0xffff;
    return result;
}
ResolverOrdinaryTokenStep apply_resolver_ordinary_token(Word token,
    const ResolverOrdinaryTokenState& state) noexcept {
    ResolverOrdinaryTokenStep result; result.state=state;
    auto& s=result.state;
    if(token==0x5dc0) { s.primary_132418=0; s.primary_13241a=0; }
    if(token!=0x59d8) return result;
    if(s.seen_59d8==0) {
        s.saved_frame=s.frame; s.seen_59d8=1;
        s.primary_13241a=0x114; s.render_phase_13234e=2; s.frameflag_f4=0;
        result.first_59d8_setup=true;
    } else if(s.reverse==0 && s.frame==799) {
        s.reverse=1; s.frame=s.saved_frame; s.render_phase_13234e=1;
        result.reverse_setup=true;
    }
    return result;
}
PendingCombatCommit commit_pending_combat(State& state, std::size_t attacker,
    std::size_t target, std::size_t city, Word& pending, Word primary, bool final_gate) {
    if(pending==0 || (!final_gate && primary!=196)) return {};
    auto copy=state;
    PendingCombatCommit result;
    result.loss=combat_population_commit(copy,attacker,target,city);
    result.committed=true;
    state=copy; pending=0; return result;
}
ResolverSlotWords snapshot_resolver_slot(Word slot, ResolverSlotWords primary,
    ResolverSlotWords secondary) noexcept {
    return slot==0?primary:secondary;
}
ResolverFrameContext initialize_resolver_frame_context(const State& state, std::size_t actor,
    Word frame, Word selector, const ResolverFrameContext& previous) {
    auto result=previous;
    if(frame==0) {
        if(actor>=actor_count) throw Unsupported("framezero actor outside0..4");
        const auto& a=state.actors[actor];
        result.render_phase_13234e=resolver_frame_slots(selector);
        result.target_actor=a.target_actor; result.target_city=a.target_city; result.magnitude=a.magnitude;
        if(selector!=0 && selector!=1 && selector!=2 && selector!=0xffff) result.reverse=0;
    }
    result.completion=2; return result;
}
ResolverInputStep process_resolver_input(bool has_event, std::uint32_t event_class,
    Word code, Word frame, Word mode, const ResolverInputControl& previous) noexcept {
    ResolverInputStep result;
    result.state=previous;
    result.state.slot_index=0;
    if(!has_event) return result;
    if(event_class==0x2000) {
        result.rewrite_reply_code=true;
        result.reply_code=2;
    }
    if(event_class==8 && code==0x68 && frame!=0) {
        if(mode!=0) result.state.control=1;
        result.state.frameflag_f4=1;
    }
    return result;
}
} // namespace nuclear
