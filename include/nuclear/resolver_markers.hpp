#pragma once
#include "nuclear/engine.hpp"

namespace nuclear {
struct ResolverEntryState {
    Word human_cities_before{}; //local-a captured102934
    Word pass{}; //local-14c initialized102a50
    Word control_return{}; //local-32 initialized1028cc
};
//1028c8..102a54 startup only. Caller supplies exactword read fromlocal-154
// after OS service11c02e. No clock substitute; original RNG seed is replaced.
// Excludes rendering/OS calls, actorpass/frameprogression and processed resets.
ResolverEntryState begin_resolver_entry(const State&, Word supplied_time_word, Rng&);
// 10383e–103856. BTST.B addresses the high byte of the big-endian word.
struct EffectMarkerResult {
    Word frame_coordinate{};
    Word effect_applied{};
    bool dispatch_effect{};
};
EffectMarkerResult consume_effect_marker(Word frame_coordinate,
                                         Word effect_applied) noexcept;
// 105096–1050cc. Number of slots inspected per handler frame.
Word resolver_frame_slots(Word selector) noexcept;
// Read-only control-flow classification at 1043ea–104458. This does not
// assert that a handler will emit a marker or perform any population change.
enum class ResolverFrameRoute {
    ordinary, end_frame, processed_slot, secondary_slot_cleanup, prepare_impact
};
struct ResolverFrameDecision {
    ResolverFrameRoute route{};
    Word completion_counter{};
    Word pending_value{}; // 15 only at the proven 104458 route, otherwise 0.
};
ResolverFrameDecision classify_resolver_frame(Word marker, Word slot,
    Word slot_processed, Word completion_counter);
//102ad4..102b62: pass entry classification only; no action/flag mutation.
// Nonzero pass routes dead, unprocessed actors to unresolved10c0ce follow-up.
enum class ResolverActorRoute { no_cities, skip_actor, ordinary_action, followup_requires_evidence };
struct ResolverActorDecision {
    ResolverActorRoute route{};
    Word pass{}, total_cities{}, first_followup_index{};
};
ResolverActorDecision classify_resolver_actor(const State&, std::size_t actor, Word pass);
struct ResolverActionPrelude {
    ResolverActorDecision eligibility{};
    bool magnitude_was_25{};
    Word magnitude_before_handler{};
    Action3Prelude substitution{};
    std::uint32_t draws{};
};
//102a98..103010 ordinary action prelude only; framezero handlers separate.
// Snapshot magnitude==25 BEFORE substitution; initialmagnitude AFTER setup.
// Skip/no-cities returns eligibility without writes/draws. Follow-up Unsupported.
ResolverActionPrelude prepare_resolver_action_prelude(State&, std::size_t actor,
    Word pass, Rng&);
struct ResolverActionStart {
    ResolverActionPrelude prelude{};
    Dispatch dispatch{};
    EventInitialization handler{};
    bool initialized{};
    std::uint32_t draws{};
};
// Ordinary prelude + known dispatch framezero only. No subsequentframe/effect
// marker is assumed. Actions with no verified handler explicitly Unsupported.
ResolverActionStart start_resolver_action(State&, std::size_t actor, Word pass, Rng&);
struct ResolverFrameTail {
    Word next_frame{};
    bool set_render_request{}; //emit write1, false means leave existingflag alone
    bool repeat_handler{};
};
//1050d6..10510a only; excludes followingbusywait/callback andeffectcommit.
// frameflag_f4 suppresses rendering whennonzero; reverseword nonzero decrements.
ResolverFrameTail resolver_frame_tail_step(Word frame, Word reverse_word,
    Word completion_word, Word frameflag_f4) noexcept;
struct ResolverOrdinaryTokenState {
    Word frame{}, reverse{}, saved_frame{}, seen_59d8{}, frameflag_f4{};
    Word primary_132418{}, primary_13241a{}, render_phase_13234e{};
};
struct ResolverOrdinaryTokenStep {
    ResolverOrdinaryTokenState state{};
    bool first_59d8_setup{}, reverse_setup{};
};
//104e18..104fb2 synchronous token writes only. Token already selected from
// primary/secondaryslot. Setupevents require presentation adapter; no ack or
// Actor/effect/pendingcommit is simulated.5dc0 clearsPRIMARY words inanyslot.
ResolverOrdinaryTokenStep apply_resolver_ordinary_token(Word token,
    const ResolverOrdinaryTokenState&) noexcept;
struct PendingCombatCommit { bool committed{}; Word loss{}; };
//103774..10383e earlygate or105112..1051d0 finalgate. Pending13254e is a
// distinct resolverword. Targetindices are originalframezero LATCHES.
// Earlyrequiresprimary13241a196, finalignoresit; bothrequirependingnonzero.
// Falsegate skipsunusedindices. Transactional onUnsupported; no renderer/ack.
PendingCombatCommit commit_pending_combat(State&, std::size_t attacker,
    std::size_t latched_target, std::size_t latched_city, Word& pending_13254e,
    Word primary_13241a, bool final_gate);
struct ResolverSlotWords { Word coordinate{}, marker{}; };
//103754..103774 snapshots BEFORE early pending commit/its external render ack.
// Original tests only zero vs nonzero slot; caller owns loop bounds.
ResolverSlotWords snapshot_resolver_slot(Word slot, ResolverSlotWords primary,
    ResolverSlotWords secondary) noexcept;
struct ResolverFrameContext {
    Word target_actor{}, target_city{}, magnitude{}; //locals-c,-e,-f6
    Word render_phase_13234e{}, reverse{}, completion{};
};
//103612..1036b6 AFTER handler invocation. Frame0 refreshes latches/phase and
// conditional reverse reset; everyframe resetscompletion2. No input/ack calls.
// Nonzeroframe doesnotread/validate actor. Rawtargetwords are copied unvalidated.
ResolverFrameContext initialize_resolver_frame_context(const State&, std::size_t actor,
    Word frame, Word selector, const ResolverFrameContext&);
struct ResolverInputControl {
    Word control{}, frameflag_f4{}, slot_index{};
};
struct ResolverInputStep {
    ResolverInputControl state{};
    bool rewrite_reply_code{};
    Word reply_code{}; // meaningful only when rewrite_reply_code is true
};
//1036ca..103754 after external render wait. Always enters105096 with slot0.
// Native polling/replying stays external; event class is a LONG, code a WORD.
ResolverInputStep process_resolver_input(bool has_event, std::uint32_t event_class,
    Word code, Word frame, Word mode, const ResolverInputControl&) noexcept;
//102a54 signed pass>1 exits to production;10530e..105330 end-pass flow.
bool resolver_pass_has_finished(Word pass) noexcept;
enum class ResolverPassRoute { repeat_pass, begin_next_pass, production_phase };
struct ResolverPassDecision { ResolverPassRoute route{}; Word pass{}, control{}; };
// Entry assumes signedpass<=1. Nonzero abort goes to production before repeat;
// otherwise nonzero repeat keeps pass, or ADDQ.W1 advances to signed exit gate.
ResolverPassDecision advance_resolver_pass(Word pass, Word repeat, Word abort_control);
} // namespace nuclear
