#pragma once
#include "nuclear/resolver_core.hpp"
namespace nuclear::resolver {
struct MissileWordWrite {
    std::uint32_t old_address{};
    Word value{};
};
struct MissilePrefix {
    ResolverActionStart start{};
    Word launch_x{}, launch_y{}, target_x{}, target_y{}, right{}, long_path{};
    // Only fields actually written BEFORE11780a; later transient fields absent.
    std::vector<MissileWordWrite> writes;
    std::uint32_t seed_before{}, seed_after{};
    static constexpr std::uint32_t stop_old_pc = 0x11780a;
    static constexpr std::uint32_t stop_new_pc = 0x23680a;
};
// Normal mode0/frame0, living committed action1 only. Uses original launch tables.
// Does not consume inventory again, reseed, latch post-handler frame context,
// execute mathffp/trajectory, produce arrival, commit damage or return resolver D0.
// Invalid host indices reject transactionally, without an invented game rule.
MissilePrefix begin_committed_missile(State &, std::size_t actor, Rng &);
enum class MissileEvent {
    trajectory_step,
    arrival,
    render_acknowledged,
    clear_slots,
    finish_action
};
// No event can bypass the missing trajectory implementation. Caller-supplied
// arrival/ack is NOT proof control reached it. This prefix deliberately stops.
[[noreturn]] void advance_missile(const MissilePrefix &, MissileEvent);

enum class DamagePhase { awaiting_gate, awaiting_render_ack, relationship_complete };
struct PopulationWrite {
    bool written{};
    Word loss{}, render_request{}, city_redraw{};
};
// Independent REACHED damage-gate continuation, not a missile-arrival provider.
// Splits105112..1051d0 at the original render request/ack wait. Early equivalent
// gate103774 likewise. No callback/Signal scheduling or generated ack events.
class DamageAcknowledgment {
    std::size_t actor_;
    ImpactState local_;
    DamagePhase phase_{DamagePhase::awaiting_gate};

  public:
    DamageAcknowledgment(std::size_t actor, ImpactState local) : actor_(actor), local_(local) {}
    DamagePhase phase() const noexcept { return phase_; }
    const ImpactState &local() const noexcept { return local_; }
    PopulationWrite write_at_gate(State &, Word secondary_status, bool final_gate);
    // Input is an actual observed/supplied request word. Nonzero keeps waiting.
    // Zero permits original relationship0 continuation. Tests label it synthetic.
    bool observe_render_request(State &, Word observed_request);
};
} // namespace nuclear::resolver
