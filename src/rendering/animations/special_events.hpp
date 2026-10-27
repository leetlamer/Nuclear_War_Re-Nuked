#pragma once
#include "core/config.hpp"
#include "rendering/animations/console_and_sound_cues.hpp"
namespace nuclear::player {
enum class Atlas { small, big, large };
struct DrawSprite {
    Atlas atlas;
    Rect source, destination;
    bool flip{};
};
inline DrawSprite small_sprite(int index, int w, int h, int x, int y, int row = 0,
                               bool flip = false) {
    return {Atlas::small, {index % 20 * 16, index / 20 * 16 + row, w, h}, {x, y, w, h}, flip};
}
// All seconds/paths below are configurable host presentation, not native cadence.
struct SpecialTiming {
    double release{.7}, arrival{2.0}, duration{3.0}, stork_speed{DEFAULT_STORK_SPEED};
};
inline bool special_art(Word action) {
    return action == 9 || action == 11 || action == 12 || action == 13 || action == 14 ||
           action == 15 || action == 16;
}
inline std::vector<DrawSprite> special_sprites(Word action, std::size_t actor, double seconds,
                                               int tx, int ty, Word population = 1,
                                               SpecialTiming timing = {}) {
    if (!std::isfinite(timing.release) || !std::isfinite(timing.arrival) ||
        !std::isfinite(timing.duration) || timing.release <= 0 ||
        timing.arrival <= timing.release || timing.duration < timing.arrival)
        throw Unsupported("invalid presentation-only special timing");
    if (!std::isfinite(timing.stork_speed) || timing.stork_speed <= 0 || timing.stork_speed > 4)
        throw Unsupported("invalid stork presentation speed");
    int frame = int(std::max(0., seconds) * 8);
    if (action == 9)
        seconds *= timing.stork_speed;
    std::vector<DrawSprite> out;
    if (seconds < 0 || seconds > timing.duration || !special_art(action))
        return out;
    const auto base = action_origin(0, actor);
    double t = std::clamp((seconds - timing.release) / (timing.arrival - timing.release), 0., 1.);
    if (action == 9) {
        bool flip = tx > 160;
        double approach = std::min(1., seconds / timing.release),
               depart = std::clamp((seconds - timing.release) / (timing.duration - timing.release),
                                   0., 1.);
        int x = int(seconds < timing.release ? (flip ? 0 : 290) + (tx - (flip ? 0 : 290)) * approach
                                             : tx + ((flip ? 320 : -32) - tx) * depart),
            y = std::max(0, ty - 48);
        out.push_back(small_sprite(72 + 2 * (frame % 3), 32, 16, x, y, 1, flip));
        out.push_back(small_sprite(
            203 + frame % 4, 16, 16, seconds < timing.release ? x + 6 : tx,
            seconds < timing.release ? y + 10 : int((ty - 36) + (36 * t * t)), 0, flip));
    }
    if (action == 12) {
        out.push_back(
            small_sprite(131, 16, 16, tx,
                         int(10 + (ty - 10) * std::clamp(seconds / timing.arrival, 0., 1.) *
                                      std::clamp(seconds / timing.arrival, 0., 1.))));
    }
    if (action == 13 || action == 14) {
        bool flip = tx < base.x;
        out.push_back(small_sprite(280 + 2 * std::min(2, int(seconds / timing.release * 3)), 32, 32,
                                   base.x - 4, base.y - 4, 0, flip));
        if (seconds >= timing.release) {
            double x = base.x + (tx - base.x) * t,
                   y = base.y + (ty - base.y) * t - 45 * std::sin(3.141592653589793 * t);
            out.push_back(small_sprite((action == 13 ? 255 : 250) + frame % 5, 16, 16, int(x),
                                       int(y), 0, flip));
        }
    }
    if (action == 15)
        out.push_back(small_sprite(286 + 2 * std::min(6, int(seconds / timing.duration * 7)), 32,
                                   32, base.x, base.y));
    if (action == 16) {
        int y = int(-10 + (std::max(0, ty - 24) + 10) * std::min(1., seconds / timing.arrival));
        out.push_back(small_sprite(260 + 2 * (frame % 8), 32, 10, tx - 8, y, 2));
    }
    // Original action11 lifts the city; it is not an alien laser attack.
    if (action == 11) {
        int y = ty - int(std::min(1., seconds / timing.arrival) * (ty + 16));
        out.push_back(
            small_sprite(city_frame(population), 16, 16, tx,
                         y)); // Lower half of the original upright missile, never the UFO strip.
        out.push_back({Atlas::big, {0, 208, 16, 16}, {tx, y + 16, 16, 16}, bool(frame % 2)});
    }
    return out;
}
inline std::optional<DrawSprite> deployed_sprite(const State &state, std::size_t actor,
                                                 double seconds) {
    if (actor >= 5 || !populated_cities(state.actors[actor]))
        return std::nullopt;
    auto a = state.actors[actor].action;
    auto base = action_origin(0, actor);
    if (a == 2)
        return DrawSprite{Atlas::big, {0, 192, 16, 16}, {base.x, base.y, 16, 16}};
    if (a == 5)
        return small_sprite(209, 16, 16, base.x, base.y);
    if (a == 4) {
        auto p = bomber_orbit(actor, seconds);
        return small_sprite(p.frame, 16, 16, int(p.x) - 8, int(p.y) - 8);
    }
    if (a == 7) {
        auto p = action_sprite(7, seconds);
        return small_sprite(p->index, 16, 16, base.x, base.y);
    }
    return std::nullopt;
}
struct FingerPose {
    Rect source, destination;
};
inline std::optional<FingerPose> finger_pose(double seconds, double duration = 1.2) {
    if (seconds < 0 || seconds >= duration)
        return std::nullopt;
    double t = seconds / duration;
    double rise = t < .4 ? t / .4 : t > .6 ? (1 - t) / .4 : 1;
    int h = int(64 * std::clamp(rise, 0., 1.));
    if (!h)
        return std::nullopt;
    return FingerPose{{t >= .4 && t <= .6 ? 96 : 0, 32, 96, h}, {100, 137 - h, 96, h}};
}
inline bool payload_visual_accepted(const session::Data &d, HumanCommand cmd) {
    if (cmd.kind != HumanCommandKind::warhead || cmd.argument > 4)
        return false;
    auto copy = d.human.selection;
    return select_human_warhead_fields(d.state, copy, cmd.argument, d.human.target_marker) ==
           HumanWarheadCheck::accepted;
}
} // namespace nuclear::player
