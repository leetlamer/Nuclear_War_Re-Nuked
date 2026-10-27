#pragma once
#include "game/events/event_text.hpp"
#include "rendering/action_media.hpp"
#include "rendering/animations/console_and_sound_cues.hpp"
#include "rendering/animations/selection_sprites.hpp"
#include "ui/console_display.hpp"
#include "ui/interface_layout.hpp"
#include "ui/player_controller.hpp"
namespace nuclear::player {
enum class EventRegion { action, human_console, speech };
enum class SkipPolicy { attack_result, advance_action, continue_result, timed_result };
enum class SceneId { inactive, human_exit, action, human_entry, human_console, speech };
enum class DoorOwner { none, human_exit, human_entry };
struct ActionEvent {
    std::size_t actor{};
    Word identity{}, action{}, target{}, city{};
    std::string name, category, outcome;
    MediaEvidence media{};
    std::string caption;
    std::vector<std::string> production_lines{};
    std::string production_heading{"Production results:"};
    EventRegion region{EventRegion::action};
    std::string speech;
    bool laser_preview{}, settled_notice{};
    Production production{};
    bool auto_continue{}, system_notice{}, revenge{};
    std::optional<State> before{}, after{};
};
inline std::string action_category(Word a) {
    switch (a) {
    case 0:
        return "No action";
    case 1:
        return "Missile launch";
    case 2:
        return "Missile preparation";
    case 3:
        return "Propaganda / event";
    case 4:
        return "Bomber preparation";
    case 5:
        return "Bomber attack";
    case 6:
        return "Production";
    case 7:
        return "Defense";
    case 88:
        return "Inactive";
    default:
        return "Special event " + std::to_string(a);
    }
}
inline bool completed_without_human_entry(const session::Event &e) {
    return e.from == session::Phase::survivor_counting && e.survivors &&
           (*e.survivors <= 1 || populated_cities(e.state_after.actors[4]) == 0);
}
// Presentation-only controller. Never calls gameplay, RNG or acknowledgment APIs.
class ActionPresentation {
    const session::Coordinator *owner_{};
    State selection_snapshot_{};
    bool have_selection_snapshot_{}, waiting_for_resolution_{};
    bool transitions_{}, human_window_{true}, closing_started_{};
    DoorOwner door_owner_{DoorOwner::none};
    std::uint64_t door_serial_{};
    DoorPhase door_{DoorPhase::none};
    double door_elapsed_{};
    bool finish_after_door_{}, door_done_{};
    bool instant_result_{}, reveal_pending_{}, instant_audio_cleanup_{},
        propaganda_audio_pending_{};
    bool production_presented_{}, notice_presented_{};
    bool autoplay_{}, human_alive_{true}, result_shown_{};
    double result_at_{}, stork_speed_{DEFAULT_STORK_SPEED};
    std::string instant_;
    TextFlow text_;
    ScrollingTextFlow production_text_;
    std::vector<std::string> messages_;
    std::size_t message_{};
    std::uint64_t blocked_until_{};
    bool text_ready_{};
    double bomber_speed_{1.};
    std::size_t observed_{}, index_{};
    bool active_{}, rendered_{};
    unsigned round_{1};
    std::array<Production, 5> produced_items_{};
    std::array<std::vector<std::string>, 5> production_lines_{};
    std::string human_production_heading_;
    std::optional<ActionEvent> pending_speech_;
    std::array<MediaEvidence, 5> committed_transfers_{};
    std::vector<session::ResolvedTurnEvent> recorded_;
    State planned_{}, before_{};
    bool have_plan_{}, have_before_{};
    std::string failure_;
    double elapsed_{}, playback_carry_{};
    std::optional<MediaReplay> replay_;
    bool completed_without_entry_{}, completed_round_{};
    std::uint64_t serial_{};
    State snapshot_{};
    std::vector<ActionEvent> events_;

  public:
    std::optional<ActionEvent> load_recorded(const std::array<std::string, 5> &names) {
        events_.clear();
        std::optional<ActionEvent> human_report;
        for (const auto &r : recorded_) {
            ActionEvent item;
            item.actor = r.actor;
            item.identity = planned_.identity[r.actor];
            item.action = r.action;
            item.target = r.target;
            item.city = r.city;
            item.name = names[r.actor];
            item.media = recorded_media(r);
            item.category = action_category(r.action);
            const auto position = std::size_t(&r - recorded_.data());
            item.auto_continue = r.death || r.revenge ||
                                 (position + 1 < recorded_.size() && recorded_[position + 1].death);
            item.revenge = r.revenge;
            item.system_notice = r.death;
            item.settled_notice = r.death;
            item.before = r.before;
            item.after = r.after;
            if (r.death)
                item.caption = item.name + " has been eliminated.";
            else {
                item.caption = original_caption(item.media, item.name,
                                                r.target < 5 ? names[r.target] : std::string{});
                if (r.action == 1 || r.action == 5)
                    item.outcome = combat_caption(item.media);
                if (r.action == 3 && r.effect == 13 && r.target < 5)
                    item.outcome = nuclear::text::format(
                        0x10538e, {std::int32_t(signed_word(r.loss)), names[r.target], item.name});
                if (audited_special_event(r.action)) {
                    item.caption =
                        special_event_announcement(item.media, item.name,
                                                   r.target < 5 ? names[r.target] : std::string{})
                            .value_or(std::string{});
                    item.outcome =
                        r.result_recorded
                            ? special_event_result(item.media, r.result_variant, r.result_amount,
                                                   item.name,
                                                   r.target < 5 ? names[r.target] : std::string{})
                                  .value_or(std::string{})
                            : std::string{};
                }
            }
            if (!r.death && !r.revenge) {
                item.production = produced_items_[r.actor];
                item.production_lines = production_lines_[r.actor];
                if (r.actor == 4) {
                    item.production_heading = human_production_heading_;
                    human_report = item;
                }
            }
            events_.push_back(std::move(item));
        }
        return human_report;
    }
    static ActionPresentation resolved_fixture(const session::Data &d) {
        ActionPresentation s;
        s.snapshot_ = s.planned_ = d.state;
        s.recorded_ = d.revenge.events;
        s.human_alive_ = populated_cities(d.state.actors[4]) != 0;
        s.load_recorded(d.names);
        s.active_ = !s.events_.empty();
        s.serial_ = 1;
        return s;
    }
    // Test harness supplies only private-copy replay metadata; no live state access.
    static ActionPresentation fixture(MediaReplay replay, const std::array<std::string, 5> &names,
                                      std::size_t actor = 4) {
        ActionPresentation scene;
        if (actor >= 5)
            throw Unsupported("invalid replay actor");
        scene.replay_ = replay;
        scene.planned_ = replay.before[actor];
        scene.before_ = replay.before[actor];
        scene.have_before_ = true;
        scene.snapshot_ = replay.state;
        const auto &e = replay.actions[actor];
        ActionEvent event;
        event.actor = actor;
        event.action = e.action;
        event.target = e.target;
        event.city = e.city;
        event.name = names[actor];
        event.category = action_category(e.action);
        event.media = e;
        event.caption =
            original_caption(e, names[actor], e.target < 5 ? names[e.target] : "unknown target");
        event.outcome = (e.action == 1 || e.action == 5) ? combat_caption(e)
                                                         : "Developer replay: presentation only.";
        if (audited_special_event(event.action)) {
            event.caption = special_event_announcement(
                                e, names[actor], e.target < 5 ? names[e.target] : std::string{})
                                .value_or(std::string{});
            event.outcome =
                e.result_recorded
                    ? special_event_result(e, e.result_variant, e.result_amount, names[actor],
                                           e.target < 5 ? names[e.target] : std::string{})
                          .value_or(std::string{})
                    : std::string{};
        }
        scene.events_.push_back(event);
        scene.active_ = true;
        scene.serial_ = 1;
        return scene;
    }
    void reset_owner() {
        bool enabled = transitions_;
        double speed = stork_speed_;
        auto generation = serial_ + 1;
        *this = ActionPresentation{};
        transitions_ = enabled;
        stork_speed_ = speed;
        serial_ = generation;
    }
    void clear_door() {
        door_ = DoorPhase::none;
        door_owner_ = DoorOwner::none;
        door_serial_ = 0;
        door_elapsed_ = 0;
        door_done_ = false;
    }
    void transitions(bool enabled) {
        transitions_ = enabled;
        if (!enabled)
            clear_door();
    }
    void constrain_door_owner(bool allowed) {
        human_window_ = allowed;
        if (!allowed) {
            if (finish_after_door_) {
                active_ = false;
                waiting_for_resolution_ = false;
                finish_after_door_ = false;
            }
            clear_door();
        }
    }
    bool transition_active() const {
        return active_ && transitions_ && human_window_ && door_serial_ == serial_ &&
               door_ != DoorPhase::none && door_owner_ != DoorOwner::none;
    }
    DoorOwner door_owner() const { return transition_active() ? door_owner_ : DoorOwner::none; }
    SceneId scene_id() const {
        if (!active_)
            return SceneId::inactive;
        if (transition_active())
            return door_owner_ == DoorOwner::human_exit ? SceneId::human_exit
                                                        : SceneId::human_entry;
        return event().region == EventRegion::action          ? SceneId::action
               : event().region == EventRegion::human_console ? SceneId::human_console
                                                              : SceneId::speech;
    }
    bool door_visible() const {
        return scene_id() == SceneId::human_exit || scene_id() == SceneId::human_entry;
    }
    DoorPhase door_phase() const { return door_; }
    double closed_fraction() const { return door_fraction(door_, door_elapsed_); }
    bool show_action_map() const {
        return active_ && !transition_active() && event().region == EventRegion::action;
    }
    void begin_transition(std::optional<std::uint64_t> expected = std::nullopt) {
        if (expected && *expected != serial_)
            return;
        if (active_ && transitions_ && human_window_ && !closing_started_ && index_ == 0 &&
            event().region == EventRegion::action) {
            closing_started_ = true;
            door_ = DoorPhase::closing;
            door_owner_ = DoorOwner::human_exit;
            door_serial_ = serial_;
            door_elapsed_ = 0;
            door_done_ = false;
        }
    }
    std::uint64_t serial() const { return serial_; }
    const State &visual_state() const {
        if (active_ && event().before)
            return effect_complete() ? *event().after : *event().before;
        if (replay_ && active_ && event().region == EventRegion::action && !event().settled_notice)
            return effect_complete() ? replay_->after[event().actor]
                                     : replay_->before[event().actor];
        return snapshot_;
    }
    // Population growth prepared by human confirmation remains hidden until that action resolves.
    State population_state() const {
        auto s = visual_state();
        if (!active_ || !have_selection_snapshot_ || !have_before_)
            return s;
        bool human_done = false;
        for (std::size_t n = 0; n <= index_ && n < events_.size(); ++n)
            if (events_[n].actor == 4 && events_[n].region == EventRegion::action &&
                !events_[n].system_notice && (n < index_ || effect_complete()))
                human_done = true;
        if (!human_done)
            for (std::size_t c = 0; c < 5; ++c) {
                int growth = int(signed_word(before_.actors[4].population[c])) -
                             signed_word(selection_snapshot_.actors[4].population[c]);
                auto &pop = s.actors[4].population[c];
                if (growth > 0 && signed_word(pop) > 0)
                    pop = Word(std::max(0, int(signed_word(pop)) - growth));
            }
        return s;
    }
    const State &interface_state(const State &live) const {
        if (transition_active())
            return transition_state();
        if (active_)
            return have_selection_snapshot_
                       ? (event().region == EventRegion::action ? selection_snapshot_ : snapshot_)
                       : visual_state();
        if (waiting_for_resolution_ && have_selection_snapshot_)
            return selection_snapshot_;
        return live;
    }
    const State &transition_state() const {
        return door_owner() == DoorOwner::human_exit && have_selection_snapshot_
                   ? selection_snapshot_
                   : (door_owner() == DoorOwner::human_exit && have_before_ ? before_ : snapshot_);
    }
    const State &snapshot() const { return snapshot_; }
    const std::vector<ActionEvent> &events() const { return events_; }
    const ActionEvent &event() const { return events_.at(index_); }
    std::size_t index() const { return index_; }
    bool active() const { return active_; }
    bool rendered() const { return rendered_; }
    void observe(const Player &p) {
        if (!p.coordinator()) {
            if (owner_)
                reset_owner();
            return;
        }
        if (!owner_ && active_)
            return; // Standalone developer replay has no live-session owner.
        const auto *context = p.coordinator();
        if (owner_ != context || (owner_ && context->events().size() < observed_)) {
            reset_owner();
            owner_ = context;
            if (context->phase() == session::Phase::human_selection) {
                observed_ = context->events().size();
                round_ = p.round();
                selection_snapshot_ = p.display_state();
                have_selection_snapshot_ = true;
                return;
            }
        }
        if (active_)
            return;
        const auto &c = *context;
        const auto &history = c.events();
        if (c.phase() == session::Phase::human_selection && observed_ == history.size()) {
            selection_snapshot_ = p.display_state();
            have_selection_snapshot_ = true;
        }
        for (; observed_ < history.size(); ++observed_) {
            const auto &e = history[observed_];
            if (!e.diagnostic.empty()) {
                replay_.reset();
                if (e.from == session::Phase::ai_planning) {
                    planned_ = e.state_after;
                    have_plan_ = false;
                    have_before_ = false;
                    for (auto &lines : production_lines_)
                        lines.clear();
                }
            }
            if ((e.from == session::Phase::banner_confirmation ||
                 e.from == session::Phase::human_selection) &&
                e.to == session::Phase::ai_planning) {
                waiting_for_resolution_ = true;
                recorded_.clear();
                for (auto &lines : production_lines_)
                    lines.clear();
                planned_ = e.state_after;
                have_plan_ = false;
                have_before_ = false;
                replay_.reset();
            }
            if (e.from == session::Phase::survivor_counting && e.survivors)
                completed_round_ = true;
            if (completed_without_human_entry(e))
                completed_without_entry_ = true;
            if (e.from == session::Phase::ai_planning && e.diagnostic.empty()) {
                committed_transfers_ = {};
                for (auto &lines : production_lines_)
                    lines.clear();
                replay_.reset();
                planned_ = e.state_after;
                have_plan_ = true;
                have_before_ = false;
            }
            if (e.to == session::Phase::resolver_handoff && e.diagnostic.empty()) {
                before_ = e.state_after;
                have_before_ = true;
            }
            if (e.ai_production)
                for (std::size_t i = 0; i < 4; ++i)
                    if (e.ai_production->produced[i]) {
                        produced_items_[i] = e.ai_production->production[i];
                        production_lines_[i] = production_summary_messages(produced_items_[i]);
                    }
            if (e.human_entry && e.human_entry->tail.speech_roll && observed_ > 0) {
                const auto &prior = history[observed_ - 1].state_after;
                auto speaker = prior.actors[4].target_actor;
                if (speaker < 4 && populated_cities(e.state_after.actors[speaker])) {
                    ActionEvent speech;
                    speech.actor = speaker;
                    speech.identity = e.state_after.identity[speaker];
                    speech.region = EventRegion::speech;
                    speech.speech = reaction_text(speech.identity, e.human_entry->tail.speech_roll);
                    if (!speech.speech.empty())
                        pending_speech_ = speech;
                }
            }
            if (e.human_entry && e.human_entry->prefix.produced) {
                produced_items_[4] = e.human_entry->prefix.production.inventory;
                production_lines_[4] = production_summary_messages(produced_items_[4]);
                human_production_heading_ = e.human_entry->prefix.production.messages.front();
            }
            if (e.from == session::Phase::resolver_handoff && e.resolver_request && have_before_ &&
                e.diagnostic.empty()) {
                recorded_ = e.resolution_events;
                for (std::size_t i = 0; i < 5; ++i)
                    committed_transfers_[i] = committed_transfer_visual(e.state_after.actors[i]);
                try {
                    auto candidate = replay_media(before_, e.resolver_request->microseconds_low16);
                    if (candidate.seed == e.seed_after &&
                        same_state(candidate.state, e.state_after))
                        replay_ = std::move(candidate);
                    else
                        replay_.reset();
                } catch (const Unsupported &) {
                    replay_.reset();
                }
            }
        }
        if (((!completed_round_ ||
              (!completed_without_entry_ && c.phase() != session::Phase::human_selection)) &&
             p.diagnostic().empty()) ||
            (!have_plan_ && p.diagnostic().empty()))
            return;
        completed_round_ = false;
        completed_without_entry_ = false;
        instant_result_ = reveal_pending_ = false;
        result_at_ = 0;
        playback_carry_ = 0;
        ++serial_;
        round_ = p.round();
        failure_ = p.diagnostic();
        snapshot_ = c.data().state;
        clear_door();
        finish_after_door_ = false;
        closing_started_ = false;
        events_.clear();
        index_ = 0;
        human_window_ = p.human_alive() && c.phase() == session::Phase::human_selection;
        human_alive_ = p.human_alive();
        autoplay_ = !human_alive_;
        std::optional<ActionEvent> human_report;
        for (std::size_t i = 0; i < 5; ++i) {
            const auto &a = planned_.actors[i];
            // Presentation filtering only: coordinator still plans every AI slot.
            if (populated_cities(a) == 0 || a.action == 0 || a.action == 88 ||
                (replay_ && !replay_->actions[i].supported &&
                 populated_cities(replay_->before[i].actors[i]) == 0))
                continue;
            ActionEvent item{i,
                             planned_.identity[i],
                             a.action,
                             a.target_actor,
                             a.target_city,
                             c.data().names[i],
                             have_plan_ ? action_category(a.action) : "Planning unavailable",
                             {},
                             {},
                             {},
                             {},
                             "Production results:",
                             EventRegion::action,
                             {}};
            if (!p.diagnostic().empty())
                item.outcome = "Unsupported: turn incomplete";
            else if (populated_cities(a) == 0)
                item.outcome = "Dead: not dispatched";
            else if (a.action == 0 || a.action == 88)
                item.outcome = "Inactive: no effect dispatched";
            else if (a.action == 2 || a.action == 4)
                item.outcome = "Weapon prepared";
            else if (a.action == 7)
                item.outcome = "Defense active";
            else if (a.action == 6)
                item.outcome = "Production boundary completed";
            else
                item.outcome.clear();
            if (replay_) {
                item.media = replay_->actions[i];
                if (item.media.supported) {
                    item.action = item.media.action;
                    item.target = item.media.target;
                    item.city = item.media.city;
                    item.category = action_category(item.action);
                }
            }
            if (!replay_ && committed_transfers_[i].supported) {
                item.media = committed_transfers_[i];
                item.action = 8;
                item.target = item.media.target;
                item.city = item.media.city;
                item.category = action_category(8);
                item.outcome = "Defector transfer resolved; amount unavailable.";
            }
            item.caption =
                original_caption(item.media, item.name,
                                 item.target < 5 ? c.data().names[item.target] : "unknown target");
            if (item.media.supported && (item.action == 1 || item.action == 5))
                item.outcome = combat_caption(item.media);
            if (item.media.supported && item.action >= 9 && item.action <= 16 &&
                item.media.population_known)
                item.outcome = (item.target < 5 ? c.data().names[item.target] : item.name) +
                               ": population " + (item.media.population_delta >= 0 ? "+" : "") +
                               std::to_string(item.media.population_delta) + " million.";
            if (item.media.supported && item.action == 3 && item.media.effect == 0x0d &&
                item.target < 5)
                item.outcome =
                    nuclear::text::format(0x10538e, {std::int32_t(signed_word(item.media.loss)),
                                                     c.data().names[item.target], item.name});
            if (item.media.supported && item.action == 8 && item.media.effect == 0x0f &&
                item.media.amount_known && item.target < 5)
                item.outcome =
                    nuclear::text::format(0x1053c3, {c.data().names[item.target],
                                                     std::int32_t(signed_word(item.media.loss))});
            if (replay_ && !item.media.supported) {
                item.outcome = populated_cities(replay_->before[i].actors[i]) == 0
                                   ? "Eliminated before action."
                                   : "No active action dispatched.";
                item.caption = item.name + ": " + item.outcome;
            }
            if (audited_special_event(item.action)) {
                item.caption = special_event_announcement(
                                   item.media, item.name,
                                   item.target < 5 ? c.data().names[item.target] : std::string{})
                                   .value_or(std::string{});
                item.outcome =
                    item.media.result_recorded
                        ? special_event_result(item.media, item.media.result_variant,
                                               item.media.result_amount, item.name,
                                               item.target < 5 ? c.data().names[item.target]
                                                               : std::string{})
                              .value_or(std::string{})
                        : std::string{};
            }
            item.production = produced_items_[i];
            item.production_lines = production_lines_[i];
            if (i == 4 && !item.production_lines.empty())
                item.production_heading = human_production_heading_;
            if (i == 4)
                human_report = item;
            events_.push_back(std::move(item));
        }
        if (!recorded_.empty())
            human_report = load_recorded(c.data().names);
        if (human_alive_ && human_report && !production_lines_[4].empty()) {
            auto console = *human_report;
            console.region = EventRegion::human_console;
            events_.push_back(console);
        }
        if (recorded_.empty())
            for (std::size_t i = 0; i < 5; ++i)
                if (populated_cities(planned_.actors[i]) &&
                    !populated_cities(snapshot_.actors[i])) {
                    ActionEvent notice;
                    notice.actor = i;
                    notice.settled_notice = true;
                    notice.caption = c.data().names[i] + " has been eliminated.";
                    events_.push_back(notice);
                }
        if (!human_alive_)
            pending_speech_.reset();
        if (pending_speech_) {
            events_.push_back(*pending_speech_);
            pending_speech_.reset();
        }
        have_plan_ = false;
        active_ = !events_.empty();
        if (!active_)
            waiting_for_resolution_ = false;
        rendered_ = false;
        elapsed_ = 0;
        text_ready_ = false;
        result_shown_ = false;
        if (active_)
            begin_transition(serial_);
    }
    // Must be called after SDL_RenderPresent, not merely after event creation.
    void presented() {
        if (active_ && transition_active() && door_done_) {
            clear_door();
            rendered_ = false;
            if (finish_after_door_) {
                active_ = false;
                waiting_for_resolution_ = false;
                finish_after_door_ = false;
            }
            return;
        }
        if (active_) {
            rendered_ = true;
            reveal_pending_ = false;
            if (event().system_notice && text_ready_ && text_.complete())
                notice_presented_ = true;
            if (automatic_production() && text_ready_ && production_text_.complete())
                production_presented_ = true;
        }
    }
    bool next() {
        if (!active_ || !rendered_ || transition_active())
            return false;
        rendered_ = false;
        notice_presented_ = false;
        elapsed_ = 0;
        text_ready_ = false;
        result_shown_ = false;
        result_at_ = 0;
        instant_result_ = reveal_pending_ = propaganda_audio_pending_ = false;
        playback_carry_ = 0;
        bool action = event().region == EventRegion::action;
        ++index_;
        if (transitions_ && human_window_ && action &&
            (index_ == events_.size() || events_[index_].region != EventRegion::action)) {
            door_ = DoorPhase::opening;
            door_owner_ = DoorOwner::human_entry;
            door_serial_ = serial_;
            door_elapsed_ = 0;
            door_done_ = false;
            if (index_ == events_.size()) {
                --index_;
                finish_after_door_ = true;
            }
            return true;
        }
        if (index_ == events_.size()) {
            active_ = false;
            waiting_for_resolution_ = false;
        }
        return true;
    }
    double animation_seconds() const {
        if (event().action == 3 && instant_result_)
            return 4.;
        if (event().action == 5)
            return (bomber_returns(event().media) ? bomber_drop_seconds + bomber_return_seconds
                                                  : 4.) /
                   bomber_clock_rate();
        return event().action == 5 && bomber_returns(event().media)
                   ? bomber_drop_seconds + bomber_return_seconds
               : (event().action == 1 || event().action == 5) ? 4.0
               : event().action == 9                          ? 3.0 / stork_speed_
               : ((event().action >= 9 && event().action <= 16) || event().action == 86) ? 3.0
                                                                                         : 2.0;
    }
    void stork_speed(double speed) {
        if (!std::isfinite(speed) || speed <= 0 || speed > 4)
            throw Unsupported("invalid stork speed");
        stork_speed_ = speed;
    }
    bool automatic_result() const { return result_shown_; }
    bool autoplay() const { return autoplay_; }
    void tick(double seconds) {
        if (!std::isfinite(seconds) || seconds < 0)
            throw Unsupported("invalid presentation elapsed time");
        if (reveal_pending_)
            return;
        if (active_ && transition_active()) {
            door_elapsed_ = std::min(door_seconds, door_elapsed_ + seconds);
            door_done_ = door_elapsed_ >= door_seconds;
            return;
        }
        if (active_) {
            if (event().system_notice && notice_presented_) {
                next();
                return;
            }
            if (instant_result_ && skip_policy() == SkipPolicy::advance_action && rendered_) {
                next();
                return;
            }
            ensure_text();
            elapsed_ = std::min(3600.0, elapsed_ + std::min(seconds, 60.0));
            text_.tick(seconds);
            if (automatic_production()) {
                production_text_.tick(seconds);
                if (production_presented_) {
                    next();
                    return;
                }
            }
            if (event().region == EventRegion::action && event().media.supported &&
                messages_.size() > 1 && message_ == 0 && rendered_ && animation_complete()) {
                message_ = 1;
                set_page(messages_[message_]);
                result_shown_ = true;
                result_at_ = elapsed_;
                rendered_ = false;
            }
            if ((autoplay_ || event().auto_continue || (instant_result_ && event().action != 3) ||
                 (!instant_result_ && event().action == 3)) &&
                rendered_ && animation_complete() &&
                elapsed_ >= std::max(animation_seconds(), result_at_) + 2.0) {
                if (message_ + 1 < messages_.size()) {
                    ++message_;
                    set_page(messages_[message_]);
                    result_at_ = elapsed_;
                    rendered_ = false;
                } else
                    next();
            }
        }
    }
    SkipPolicy skip_policy() const {
        switch (event().action) {
        case 1:
        case 5:
            return SkipPolicy::attack_result;
        case 2:
        case 4:
        case 6:
        case 7:
            return SkipPolicy::advance_action;
        case 3:
            return SkipPolicy::continue_result;
        default:
            return SkipPolicy::timed_result;
        }
    }
    // Completed deployment poses still own their acknowledgement click. Attacks
    // retain their original movement-only gesture boundary.
    bool input_eligible() const {
        return (active_ && rendered_ && !transition_active() &&
                event().region == EventRegion::action && event().action == 3 && instant_result_) ||
               playback_eligible() ||
               (active_ && rendered_ && !transition_active() &&
                event().region == EventRegion::action && event().media.supported &&
                !event().laser_preview && !instant_result_ &&
                (skip_policy() == SkipPolicy::advance_action ||
                 skip_policy() == SkipPolicy::continue_result));
    }
    bool playback_eligible() const {
        if (!active_ || !rendered_ || transition_active() ||
            event().region != EventRegion::action || !event().media.supported ||
            event().laser_preview || animation_complete())
            return false;
        Word a = event().action;
        return (a >= 1 && a <= 9) || (a >= 11 && a <= 16) || a == 86;
    }
    bool instant_complete(std::uint64_t now) {
        if (!input_eligible())
            return false;
        if (event().action == 3 && instant_result_) {
            instant_audio_cleanup_ = true;
            propaganda_audio_pending_ = false;
            return next_after_input();
        }
        if (now < blocked_until_)
            return false;
        ensure_text();
        double target = skip_policy() == SkipPolicy::continue_result ? 2. : animation_seconds();
        if (event().action == 1 || event().action == 5) {
            double t = visual_elapsed();
            target = (t < 2.8  ? 2.8
                      : t < 4. ? 4.
                               : bomber_drop_seconds + bomber_return_seconds) /
                     bomber_clock_rate();
        }
        elapsed_ =
            skip_policy() == SkipPolicy::continue_result ? target : std::max(elapsed_, target);
        playback_carry_ = 0;
        instant_result_ = reveal_pending_ = true;
        instant_audio_cleanup_ = skip_policy() != SkipPolicy::continue_result;
        propaganda_audio_pending_ = skip_policy() == SkipPolicy::continue_result;
        result_shown_ = true;
        result_at_ = elapsed_;
        blocked_until_ = now + 180;
        rendered_ = false;
        if (messages_.size() > 1) {
            message_ = 1;
            set_page(messages_[message_]);
        }
        return true;
    }
    bool instant_result() const { return instant_result_; }
    bool take_propaganda_result_audio() {
        bool pending = propaganda_audio_pending_;
        propaganda_audio_pending_ = false;
        return pending;
    }
    bool take_instant_audio_cleanup() {
        bool pending = instant_audio_cleanup_;
        instant_audio_cleanup_ = false;
        return pending;
    }
    double draw_elapsed() const {
        double t = visual_elapsed();
        if (instant_result_ && animation_complete() && event().action != 5 && event().action != 3)
            return std::min(t, animation_seconds());
        return t;
    }
    // Essential arrival/result boundary gets a frame even with a stall or boost.
    void playback_tick(double seconds, bool boosted) {
        if (!std::isfinite(seconds) || seconds < 0)
            throw Unsupported("invalid playback elapsed time");
        if (reveal_pending_) {
            tick(0);
            return;
        }
        if ((boosted || playback_carry_ > 0) && playback_eligible()) {
            double available = seconds + playback_carry_;
            seconds = available;
            playback_carry_ = 0;
            double end = animation_seconds();
            for (double boundary :
                 {event().action == 1 || event().action == 5 ? 2.8 / bomber_clock_rate()
                  : event().action == 9                      ? 2. / stork_speed_
                                                             : 2.,
                  event().action == 1 || event().action == 5 ? 4. / bomber_clock_rate() : end, end})
                if (elapsed_ < boundary && elapsed_ + seconds > boundary)
                    seconds = std::min(seconds, boundary - elapsed_);
            playback_carry_ = available - seconds;
        } else
            playback_carry_ = 0;
        tick(seconds);
        if (!active_ || animation_complete())
            playback_carry_ = 0;
    }

    void typing_speed(double cps) {
        text_.speed(cps);
        production_text_.speed(cps);
    }
    void ensure_text() {
        if (!active_ || text_ready_)
            return;
        messages_.clear();
        message_ = 0;
        const auto &e = event();
        if (e.region == EventRegion::human_console) {
            std::string items = e.production_heading;
            for (const auto &line : e.production_lines)
                items += "\n" + line;
            messages_.push_back(items);
        } else if (e.region == EventRegion::speech)
            messages_.push_back(e.speech);
        else {
            messages_.push_back(e.caption);
            if (e.action != 7 && e.action != 6 &&
                !((e.action == 2 || e.action == 4) && e.outcome == "Weapon prepared")) {
                if (!e.outcome.empty())
                    messages_.push_back(e.outcome);
                if (e.action != 1 && e.action != 5) {
                    auto payload = payload_caption(e.media);
                    if (!payload.empty())
                        messages_.push_back(payload);
                }
            }
        }
        set_page(messages_[0]);
        text_ready_ = true;
    }
    // 10D202..10D238: class3 warhead is last summary; cue follows full typing.
    bool production_100mt_ready() const {
        return automatic_production() && text_ready_ && !transition_active() &&
               production_presented_ && event().production.warheads[3] != 0;
    }
    bool finish_production() {
        return production_presented_ && automatic_production() && text_ready_ &&
               !transition_active() && next();
    }
    bool selection_blocked() const { return active_ && !accepts_action_click(); }
    bool automatic_production() const {
        return active_ && event().region == EventRegion::human_console && event().action == 6;
    }
    void set_page(const std::string &s) {
        if (automatic_production()) {
            production_text_.set(s);
            production_presented_ = false;
            return;
        }
        if (event().region == EventRegion::action && !event().system_notice) {
            instant_ = action_line(s);
            return;
        }
        text_.set(s,
                  event().region == EventRegion::human_console ? 27
                  : event().region == EventRegion::speech      ? 26
                                                               : 52,
                  event().region == EventRegion::human_console ? 3 : 2);
    }
    bool main_text_visible() const {
        return human_alive_ &&
               (!active_ || (!transition_active() && event().region == EventRegion::speech));
    }
    bool speech_visible() const {
        return active_ && !transition_active() && event().region == EventRegion::speech;
    }
    bool accepts_action_click() const {
        return speech_visible() && rendered_ && index_ + 1 == events_.size();
    }
    bool dismiss_for_action() {
        if (!accepts_action_click())
            return false;
        active_ = false;
        return true;
    }
    bool human_console() const {
        return active_ && !transition_active() && event().region == EventRegion::human_console;
    }
    static ActionPresentation replay_event(ActionEvent event, const State &state) {
        ActionPresentation scene;
        scene.snapshot_ = state;
        scene.events_.push_back(std::move(event));
        scene.active_ = true;
        scene.serial_ = 1;
        return scene;
    }

    std::uint64_t console_source() const {
        return serial_ * 4096 + index_ * 64 + message_ * 8 + text_.page();
    }
    std::string console_prefix() const {
        return automatic_production() ? production_text_.revealed() : visible_text();
    }
    std::string visible_text() const {
        if (automatic_production())
            return production_text_.visible();
        return text_ready_ ? (event().region == EventRegion::action && !event().system_notice
                                  ? instant_
                                  : text_.visible())
                           : std::string();
    }
    bool text_complete() const {
        if (automatic_production())
            return production_text_.complete();
        return text_ready_ && ((event().region == EventRegion::action && !event().system_notice) ||
                               text_.complete());
    }
    // All normal presentation inputs use this first-frame and debounce barrier.
    bool click(std::uint64_t now) {
        if (active_ && event().system_notice)
            return false;
        if (!active_ || !rendered_ || transition_active() || now < blocked_until_ ||
            automatic_production())
            return false;
        ensure_text();
        blocked_until_ = now + 180;
        rendered_ = false;
        if (event().region != EventRegion::action && text_.advance())
            return true;
        if ((event().action == 1 || event().action == 5) && event().media.supported &&
            !animation_complete())
            return false;
        if (message_ + 1 < messages_.size()) {
            ++message_;
            set_page(messages_[message_]);
            return true;
        }
        if (bomber_returns(event().media) &&
            visual_elapsed() < bomber_drop_seconds + bomber_return_seconds) {
            rendered_ = false;
            return true;
        }
        return next_after_input();
    }
    bool next_after_input() {
        rendered_ = true;
        return next();
    }
    double elapsed() const { return elapsed_; }
    void bomber_speed(double multiplier) {
        if (!std::isfinite(multiplier) || multiplier <= 0 || multiplier > 4)
            throw Unsupported("invalid bomber visual speed");
        bomber_speed_ = multiplier;
    }
    double bomber_clock_rate() const {
        const auto &e = event();
        if (e.action != 5 || e.target >= 5 || e.city >= 5)
            return 1.;
        const auto &a = snapshot_.actors[e.target];
        return bomber_travel_clock(e.actor, 1., a.city_x[e.city] + 8, a.city_y[e.city] + 7,
                                   bomber_speed_);
    }
    double visual_elapsed() const { return elapsed_ * bomber_clock_rate(); }
    bool effect_complete() const {
        return active_ &&
               (event().action == 5 ? visual_elapsed() : elapsed_) >=
                   (event().action == 9                            ? 3.0 / stork_speed_
                    : (event().action == 1 || event().action == 5) ? 4.0
                    : ((event().action >= 9 && event().action <= 16) || event().action == 86)
                        ? 3.0
                        : 2.0);
    }
    bool animation_complete() const {
        return !transition_active() &&
               (!(event().action == 3 && instant_result_) || elapsed_ >= animation_seconds()) &&
               effect_complete() &&
               (!bomber_returns(event().media) ||
                visual_elapsed() >= bomber_drop_seconds + bomber_return_seconds);
    }
    bool skip() {
        if (!active_ || !rendered_)
            return false;
        active_ = false;
        waiting_for_resolution_ = false;
        rendered_ = false;
        propaganda_audio_pending_ = false;
        clear_door();
        finish_after_door_ = false;
        return true;
    }
    std::string inventory_change(std::size_t actor) const {
        auto total = [](const Actor &a) {
            unsigned n = 0;
            for (auto v : a.missiles)
                n += v;
            for (auto v : a.warheads)
                n += v;
            for (auto v : a.bombers)
                n += v;
            for (auto v : a.defenses)
                n += v;
            return n;
        };
        return "Inventory " + std::to_string(total(planned_.actors.at(actor))) + " -> " +
               std::to_string(total(snapshot_.actors.at(actor)));
    }
    std::string population_change(std::size_t actor) const {
        if (!have_before_)
            return "Population outcome unavailable";
        unsigned old = 0, now = 0;
        for (auto v : before_.actors.at(actor).population)
            old += v;
        for (auto v : snapshot_.actors.at(actor).population)
            now += v;
        return "Round population " + std::to_string(old) + " -> " + std::to_string(now);
    }
};
} // namespace nuclear::player
