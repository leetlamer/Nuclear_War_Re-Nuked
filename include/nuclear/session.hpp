#pragma once
#include "nuclear/human_commands.hpp"
#include "nuclear/resolver_markers.hpp"
#include "nuclear/revenge_state.hpp"
#include <functional>
#include <optional>
#include <utility>

namespace nuclear::session {
// Host bookkeeping phases; not original numeric action or interrupt codes.
enum class Phase {
    setup, living_human_entry, human_selection, banner_confirmation,
    ai_planning, attitudes, human_control, resolver_handoff, ai_production,
    resolver_control, post_resolver_relationships, survivor_counting,
    ending, return_routing, restart, exited
};
const char* phase_name(Phase) noexcept;
struct Data {
    State state{};
    Rng rng;
    // Runtime names and presentation globals supplied by real setup, not guessed.
    std::array<std::string,5> names{};
    Word welcome_flag{};
    MessageSequenceRng text;
    HumanCommandState human{};
    RevengeState revenge{};
    explicit Data(std::uint32_t seed, bool text_at_initial_origin)
        : rng(seed), text{text_at_initial_origin} {}
};
enum class CallbackPoint { before_human_entry, after_human_entry,
    before_human_command, after_human_command, resolver_owned };
// Hooks are explicit scheduling opportunities, NEVER ticks/interrupts. An adapter
// must reject unknown intra-slice interleavings; these hooks cannot represent
// callbacks inserted inside a verified synchronous helper. No default no-op.
class CallbackSchedule {
public:
    virtual ~CallbackSchedule() = default;
    virtual void apply(CallbackPoint, Data&);
};
struct ResolverRequest {
    Word microseconds_low16{};
    ResolverEntryState entry{}; // begin_resolver_entry already called exactly once
    std::array<std::size_t,5> actor_order{{0,1,2,3,4}};
};
// Providers operate on staged Data, not external devices. They must not retain
// mutable state/perform irreversible effects: failed transitions roll back Data,
// not an external provider. Resolver excludes entry reseed and exit production.
// It owns action/damage commits and its explicit scheduling; coordinator does not.
class Boundaries {
public:
    virtual ~Boundaries() = default;
    virtual Word setup(Data&, CallbackSchedule&);
    virtual Word resolve(Data&, const ResolverRequest&, CallbackSchedule&);
    virtual Word ending(Data&, CallbackSchedule&);
};
struct Adapters { Boundaries& boundaries; CallbackSchedule& callbacks; };
struct Diagnostic { Phase phase; std::string message; };
struct Event {
    Phase from{}, to{};
    std::string operation;
    std::uint32_t seed_before{}, seed_after{};
    Word retained_control{}, resolver_result{};
    State state_after{}; // committed snapshot; failed attempts retain prior snapshot
    std::optional<HumanCommand> command;
    std::optional<HumanEntry> human_entry;
    std::optional<HumanCommandResult> human_result;
    std::optional<AiProductionPhase> ai_production;
    std::optional<ResolverRequest> resolver_request;
    std::optional<Word> survivors;
    std::vector<ResolvedTurnEvent> resolution_events;
    std::string diagnostic;
};
// Normal-session main100658..100888 / new21f658..21f888. Setup/ending/native
// resolver are explicit boundaries, NOT initialize_state/full-game substitutes.
class Coordinator {
    struct Working {
        Data data;
        Phase phase{Phase::setup};
        Word retained_control{}, resolver_result{};
        explicit Working(Data d) : data(std::move(d)) {}
    } current_;
    std::vector<Event> events_;
    std::optional<Diagnostic> diagnostic_;
    bool transition(const char*, const std::function<void(Working&,Event&)>&);
    void human_command(Working&, Event&, HumanCommand, Adapters&);
public:
    explicit Coordinator(Data initial) : current_(std::move(initial)) {}
    Phase phase() const noexcept { return current_.phase; }
    const Data& data() const noexcept { return current_.data; }
    Word retained_control() const noexcept { return current_.retained_control; }
    Word resolver_result() const noexcept { return current_.resolver_result; }
    const std::vector<Event>& events() const noexcept { return events_; }
    const std::optional<Diagnostic>& diagnostic() const noexcept { return diagnostic_; }
    // One phase per call; no busy loop, autonomous fallback or implicit clock.
    // Time word required ONLY at resolver handoff. Failed transitions leave Data,
    // phase and prior committed effects unchanged, and append a diagnostic event.
    bool advance(Adapters&, std::optional<Word> microseconds_low16 = std::nullopt);
    bool select(HumanCommand, Adapters&); // confirm must use independent banner API
    bool request_banner_confirmation(); // queues confirmation phase; no command commit
};
} // namespace nuclear::session
