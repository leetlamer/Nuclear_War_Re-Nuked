#pragma once
#include "nuclear/session.hpp"

namespace nuclear::resolver {
// Resolver stack locals, separate from mutable Actor target fields.
struct Context {
    Word target_actor{}, target_city{}, magnitude{}, suppression{};
};
struct EffectResult {
    Context context{};
    Word coordinate{}, applied{};
    bool dispatched{};
    std::optional<Word> message_variant;
    std::optional<Word> chosen_city;
    std::uint32_t seed_before{}, seed_after{};
};
// 10383e..1043e6: reached saved-coordinate marker, no frame scheduling.
// Pure bounded composition: caller must establish valid nonaliasing storage and
// no unrepresented callbacks inside the synchronous helper slice. No defaults
// synthesize those native facts. Unsupported leaves state and RNG unchanged.
EffectResult effect_marker(State&, std::size_t actor, Word selector,
    Word saved_coordinate, Word effect_applied, Context, Rng&);

struct ImpactState {
    Context context{};
    Word selector{}, completion{2}, primary_processed{}, pending{};
    bool magnitude_was_25{};
};
struct ImpactResult {
    ImpactState local{};
    ResolverFrameRoute route{};
    std::optional<CombatEvaluation> combat;
    std::uint32_t seed_before{}, seed_after{};
};
// Reached primary-slot ordinary attack token only. No collision/arrival inference.
// Only active normal missile/bomber effect2..13. Other impact paths Unsupported.
ImpactResult primary_impact(State&, std::size_t actor, Word marker,
    const ImpactState&, Rng&);
struct CommitResult { ImpactState local{}; PendingCombatCommit commit{}; };
// Established early or final gate. This composes the existing synchronous
// population+relationship slice; renderer/ack scheduling must be supplied outside.
CommitResult pending_commit(State&, std::size_t actor, const ImpactState&,
    Word primary_status, bool final_gate);

// Session integration: a body provider must establish each reached slice and
// actual exit, not merely finish a list of effects. Default remains Unsupported.
// Do not reseed or perform AI production here: Coordinator owns both boundaries.
class Body {
public:
    virtual ~Body() = default;
    virtual Word execute(session::Data&, const session::ResolverRequest&,
                         session::CallbackSchedule&);
};
class Boundary : public session::Boundaries {
    Body& body_;
public:
    explicit Boundary(Body& body) : body_(body) {}
    Word resolve(session::Data&, const session::ResolverRequest&,
                 session::CallbackSchedule&) override;
};
} // namespace nuclear::resolver
