#pragma once
#include "ui/turn_presentation.hpp"
#include <fstream>
#include <iomanip>
#include <map>
namespace nuclear::player {
// Independent table transcribed from signed comparisons in10221E/22121E.
inline Word original_city_icon(Word raw) {
    constexpr int limits[]{1, 5, 10, 20, 30, 60};
    constexpr Word frames[]{150, 220, 221, 222, 223, 224, 151};
    for (int i = 0; i < 6; ++i)
        if (signed_word(raw) < limits[i])
            return frames[i];
    return frames[6];
}
// Developer-only observer; never publishes replay writes or uses the live RNG.
class CityAudit {
    const session::Coordinator *owner_{};
    unsigned session_{};
    std::ofstream file_;
    std::size_t observed_{};
    State prior_{};
    bool have_prior_{};
    std::map<std::string, State> previous_;
    std::string render_key_;

  public:
    void new_session() {
        owner_ = nullptr;
        observed_ = 0;
        have_prior_ = false;
        previous_.clear();
        render_key_.clear();
    }
    explicit CityAudit(const std::string &path) : file_(path) {
        if (!file_)
            throw Unsupported("cannot open developer city trace");
    }
    void snapshot(const std::string &stream, const std::string &boundary, const std::string &cause,
                  const State &shown, const State &authority, std::uint32_t seed,
                  bool mutation_allowed = true, bool visible = true) {
        auto found = previous_.find(stream);
        const auto &previous = found == previous_.end() ? shown : found->second;
        for (std::size_t actor = 0; actor < 5; ++actor)
            for (std::size_t city = 0; city < 5; ++city) {
                auto pop = shown.actors[actor].population[city],
                     old = previous.actors[actor].population[city];
                auto frame = city_frame(pop), expected = original_city_icon(pop);
                if (frame != expected)
                    throw Unsupported("city sprite violates original signed threshold table");
                if (!mutation_allowed && old != pop)
                    throw Unsupported("population changed at non-gameplay boundary");
                file_ << "{\"session_generation\":" << session_
                      << ",\"stream\":" << std::quoted(stream)
                      << ",\"boundary\":" << std::quoted(boundary)
                      << ",\"cause\":" << std::quoted(cause) << ",\"stable_city_id\":"
                      << std::quoted("actor" + std::to_string(actor) + "/city" +
                                     std::to_string(city))
                      << ",\"actor\":" << actor << ",\"city_index\":" << city
                      << ",\"population\":" << pop << ",\"signed_population\":" << signed_word(pop)
                      << ",\"previous_population\":" << old
                      << ",\"population_delta\":" << (int(pop) - int(old))
                      << ",\"signed_delta\":" << (int(signed_word(pop)) - int(signed_word(old)))
                      << ",\"authoritative_population\":"
                      << authority.actors[actor].population[city] << ",\"selected_frame\":" << frame
                      << ",\"expected_frame\":" << expected
                      << ",\"sprite_asset\":\"smalltiles\",\"sprite_source\":[" << (frame % 20 * 16)
                      << "," << (frame / 20 * 16)
                      << ",16,16],\"visible_snapshot\":" << (visible ? "true" : "false")
                      << ",\"rng_seed\":" << seed << "}\n";
            }
        previous_[stream] = shown;
        file_.flush();
    }
    void observe(const Player &player) {
        if (!player.coordinator())
            return;
        const auto *context = player.coordinator();
        if (context != owner_ || context->events().size() < observed_) {
            owner_ = context;
            ++session_;
            observed_ = 0;
            have_prior_ = false;
            previous_.clear();
            render_key_.clear();
        }
        const auto &events = context->events();
        for (; observed_ < events.size(); ++observed_) {
            const auto &e = events[observed_];
            if (e.resolver_request && have_prior_ && e.diagnostic.empty()) {
                auto replay = replay_media(prior_, e.resolver_request->microseconds_low16);
                if (replay.seed == e.seed_after && same_state(replay.state, e.state_after)) {
                    for (std::size_t actor = 0; actor < 5; ++actor) {
                        auto &action = replay.actions[actor];
                        if (!action.supported)
                            continue;
                        auto cause = "resolver actor=" + std::to_string(actor) +
                                     " action=" + std::to_string(action.action) +
                                     " effect=" + std::to_string(action.effect);
                        snapshot("matched_resolver_replay", "before-action", cause,
                                 replay.before[actor], e.state_after, e.seed_after, true, false);
                        snapshot("matched_resolver_replay",
                                 action.action >= 8 ? "after-special-event" : "after-action", cause,
                                 replay.after[actor], e.state_after, e.seed_after, true, false);
                    }
                } else
                    file_ << "{\"replay\":\"unavailable; final state/RNG did not match\"}\n";
            }
            const bool allowed =
                e.from == session::Phase::setup || e.from == session::Phase::resolver_handoff;
            std::string boundary = e.to == session::Phase::human_selection
                                       ? "before-human-input"
                                       : session::phase_name(e.to);
            snapshot("authoritative", boundary,
                     std::string(session::phase_name(e.from)) + ":" + e.operation, e.state_after,
                     e.state_after, e.seed_after, allowed, false);
            prior_ = e.state_after;
            have_prior_ = true;
        }
    }
    void render(const Player &player, const ActionPresentation &scene, const State &shown) {
        if (!player.coordinator())
            return;
        std::string key = std::to_string(player.round()) + ":" + std::to_string(scene.serial()) +
                          ":" + std::to_string(int(scene.scene_id())) + ":" +
                          (scene.active() ? std::to_string(scene.index()) + ":" +
                                                std::to_string(scene.effect_complete())
                                          : "main");
        key += ":" + std::to_string(player.coordinator()->data().human.logical_view) + ":" +
               session::phase_name(player.coordinator()->phase());
        std::uint32_t hash = 2166136261u;
        for (const auto &actor : shown.actors)
            for (auto p : actor.population)
                hash = (hash ^ p) * 16777619u;
        key += ":" + std::to_string(hash);
        if (key == render_key_)
            return;
        render_key_ = key;
        std::string cause = scene.active()
                                ? "presentation historical snapshot; no live mutation"
                                : (&shown == &player.coordinator()->data().state
                                       ? "authoritative main/AI-only state"
                                       : "frozen pre-resolution interface; no live mutation");
        if (scene.active())
            cause += " actor=" + std::to_string(scene.event().actor) +
                     " action=" + std::to_string(scene.event().action);
        file_ << "{\"scene_id\":" << int(scene.scene_id())
              << ",\"door_owner\":" << int(scene.door_owner())
              << ",\"door_phase\":" << int(scene.door_phase())
              << ",\"door_visible\":" << (scene.door_visible() ? "true" : "false") << "}\n";
        snapshot("render",
                 scene.show_action_map()     ? "before-action-render"
                 : scene.transition_active() ? "human-interface-transition"
                                             : "main-or-console-render",
                 cause, shown, player.coordinator()->data().state,
                 player.coordinator()->data().rng.seed());
    }
};
} // namespace nuclear::player
