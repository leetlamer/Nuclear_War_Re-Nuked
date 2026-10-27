#include "nuclear/ending.hpp"
#include "nuclear/session.hpp"
#include <utility>

namespace nuclear::session {
const char *phase_name(Phase p) noexcept {
    switch (p) {
#define NAME(x)                                                                                    \
    case Phase::x:                                                                                 \
        return #x
        NAME(setup);
        NAME(living_human_entry);
        NAME(human_selection);
        NAME(banner_confirmation);
        NAME(ai_planning);
        NAME(attitudes);
        NAME(human_control);
        NAME(resolver_handoff);
        NAME(ai_production);
        NAME(resolver_control);
        NAME(post_resolver_relationships);
        NAME(survivor_counting);
        NAME(ending);
        NAME(return_routing);
        NAME(restart);
        NAME(exited);
#undef NAME
    }
    return "invalid";
}
void CallbackSchedule::apply(CallbackPoint, Data &) {
    throw Unsupported("native callback interleavings require an explicit scheduling adapter");
}
Word Boundaries::setup(Data &, CallbackSchedule &) {
    throw Unsupported("original setup/selection requires an explicit setup adapter");
}
Word Boundaries::resolve(Data &, const ResolverRequest &, CallbackSchedule &) {
    throw Unsupported(
        "full resolver/FFP composition and undefined follow-up guard are Unsupported");
}
Word Boundaries::ending(Data &, CallbackSchedule &) {
    throw Unsupported("native ending/name/persistence requires an explicit ending adapter");
}
bool Coordinator::transition(const char *operation,
                             const std::function<void(Working &, Event &)> &fn) {
    auto staged = current_;
    Event e;
    e.from = current_.phase;
    e.to = current_.phase;
    e.operation = operation;
    e.seed_before = current_.data.rng.seed();
    try {
        fn(staged, e);
        e.to = staged.phase;
        e.seed_after = staged.data.rng.seed();
        e.retained_control = staged.retained_control;
        e.resolver_result = staged.resolver_result;
        e.state_after = staged.data.state;
        // Append first so allocation failure cannot commit untraced state.
        events_.push_back(std::move(e));
        current_ = std::move(staged);
        diagnostic_.reset();
        return true;
    } catch (const Unsupported &error) {
        e.to = current_.phase;
        e.seed_after = current_.data.rng.seed();
        e.retained_control = current_.retained_control;
        e.resolver_result = current_.resolver_result;
        e.state_after = current_.data.state;
        // Remove reports from uncommitted work; keep requested command/operation.
        e.human_entry.reset();
        e.human_result.reset();
        e.ai_production.reset();
        e.resolver_request.reset();
        e.survivors.reset();
        e.diagnostic = error.what();
        events_.push_back(std::move(e));
        diagnostic_ = Diagnostic{current_.phase, error.what()};
        return false;
    }
}
void Coordinator::human_command(Working &w, Event &e, HumanCommand c, Adapters &a) {
    e.command = c;
    a.callbacks.apply(CallbackPoint::before_human_command, w.data);
    auto r =
        apply_human_command(w.data.state, w.data.human, c, w.data.names, w.data.text, w.data.rng);
    a.callbacks.apply(CallbackPoint::after_human_command, w.data);
    e.human_result = r;
    if (r.turn_finished) {
        w.retained_control = r.control_return;
        w.phase = Phase::ai_planning;
    } else
        w.phase = Phase::human_selection;
}
bool Coordinator::select(HumanCommand c, Adapters &a) {
    return transition("human_selection", [&](Working &w, Event &e) {
        if (w.phase != Phase::human_selection)
            throw Unsupported("human selection is not active");
        if (c.kind == HumanCommandKind::confirm)
            throw Unsupported("confirmation must use the independent banner phase");
        human_command(w, e, c, a);
    });
}
bool Coordinator::request_banner_confirmation() {
    return transition("banner_requested", [](Working &w, Event &) {
        if (w.phase != Phase::human_selection)
            throw Unsupported("banner requires active human selection");
        w.phase = Phase::banner_confirmation;
    });
}
bool Coordinator::advance(Adapters &a, std::optional<Word> time_word) {
    return transition("advance", [&](Working &w, Event &e) {
        if (time_word && w.phase != Phase::resolver_handoff)
            throw Unsupported("external reseed word is accepted only at resolver handoff");
        switch (w.phase) {
        case Phase::setup:
            w.retained_control = a.boundaries.setup(w.data, a.callbacks);
            w.phase = w.retained_control == 0 ? Phase::living_human_entry : Phase::exited;
            break;
        case Phase::living_human_entry:
            if (populated_cities(w.data.state.actors[human_actor]) == 0) {
                e.operation = "dead_human_entry_skipped";
                w.phase = Phase::ai_planning; // carried control untouched; no entry or production
            } else {
                a.callbacks.apply(CallbackPoint::before_human_entry, w.data);
                const auto start =
                    start_human_commands(w.data.state, w.data.human.selection.selected_class,
                                         w.data.human.selection.selected_capacity, w.data.names,
                                         w.data.welcome_flag, w.data.text, w.data.rng);
                w.data.human = start.command_state;
                e.human_entry = start.entry;
                a.callbacks.apply(CallbackPoint::after_human_entry, w.data);
                w.phase = Phase::human_selection;
            }
            break;
        case Phase::banner_confirmation:
            human_command(w, e, {HumanCommandKind::confirm, 0}, a);
            break;
        case Phase::ai_planning:
            ai_turn(w.data.state, w.data.rng); // verified ordered actors0..3, no dead filter
            w.phase = Phase::attitudes;
            break;
        case Phase::attitudes:
            update_attitudes(w.data.state);
            w.phase = Phase::human_control;
            break;
        case Phase::human_control:
            w.phase = w.retained_control == 0 ? Phase::resolver_handoff : Phase::return_routing;
            break;
        case Phase::resolver_handoff: {
            if (!time_word)
                throw Unsupported("resolver requires explicit CurrentTime microseconds low16 word");
            ResolverRequest request;
            request.microseconds_low16 = *time_word;
            request.entry = begin_resolver_entry(w.data.state, *time_word, w.data.rng);
            w.resolver_result = a.boundaries.resolve(w.data, request, a.callbacks);
            e.resolver_request = request;
            if (w.data.revenge.contains_death)
                e.resolution_events = w.data.revenge.events;
            // No assignment of resolver D0 to retained human/session control.
            w.phase = Phase::ai_production;
            break;
        }
        case Phase::ai_production:
            e.ai_production = finish_ai_production(w.data.state, w.data.rng);
            w.phase = Phase::resolver_control;
            break; // including nonzero/abort result
        case Phase::resolver_control:
            w.phase =
                w.resolver_result == 0 ? Phase::post_resolver_relationships : Phase::return_routing;
            break;
        case Phase::post_resolver_relationships:
            perturb_profile3_scores(w.data.state, w.data.rng);
            w.phase = Phase::survivor_counting;
            break;
        case Phase::survivor_counting:
            e.survivors = living_actors(w.data.state);
            w.phase = *e.survivors <= 1 ? Phase::ending : Phase::living_human_entry;
            break;
        case Phase::ending:
            w.retained_control = a.boundaries.ending(w.data, a.callbacks);
            w.phase = Phase::return_routing;
            break;
        case Phase::return_routing:
            w.phase = ending_requests_exit(w.retained_control) ? Phase::exited : Phase::restart;
            break;
        case Phase::restart:
            w.phase = Phase::setup;
            break; // full setup boundary; no implicit DATA/text reset
        case Phase::human_selection:
            throw Unsupported(
                "awaiting normalized human command or independent banner confirmation");
        case Phase::exited:
            throw Unsupported("session has exited");
        }
    });
}
} // namespace nuclear::session
