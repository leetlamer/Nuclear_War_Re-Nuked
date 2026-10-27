#pragma once
#include "core/game_text.hpp"
#include "rendering/action_media.hpp"
#include "rendering/animations/action_sprites.hpp"
namespace nuclear::player {
// PROTOTYPE_APPROXIMATION: elapsed-time host trajectories, never native cadence.
inline int preparation_height(double seconds, double duration = .4) {
    return int(16 * std::clamp(seconds / duration, 0.0, 1.0));
}
inline Word bomber_heading(double dx, double dy) {
    double a = std::atan2(dx, -dy);
    int n = int(std::lround(a / (3.141592653589793 / 4)));
    return Word(207 + (n + 8) % 8);
}
struct BomberPose {
    double x, y;
    Word frame;
};
inline BomberPose bomber_orbit(std::size_t actor, double seconds) {
    auto c = action_origin(0, actor);
    double a = seconds * 2 * 3.141592653589793 / 2.5;
    return {std::clamp(c.x + 18 * std::sin(a), 8., 304.),
            std::clamp(c.y - 16 + (actor == 4 ? 6 : 0) - 12 * std::cos(a), 8., 176.),
            bomber_heading(18 * std::cos(a), 12 * std::sin(a))};
}
inline BomberPose bomber_attack(std::size_t actor, double seconds, double tx, double ty) {
    auto s = bomber_orbit(actor, 0);
    double t = std::clamp(seconds / 2.8, 0., 1.);
    return {s.x + (tx - s.x) * t, s.y + (ty - s.y) * t, bomber_heading(tx - s.x, ty - s.y)};
}
inline constexpr double bomber_short_medium_limit = 200., bomber_short_medium_speed = 1.5,
                        bomber_long_speed = 1.25;
inline double bomber_travel_clock(std::size_t actor, double seconds, double tx, double ty,
                                  double multiplier = 1.) {
    auto home = bomber_orbit(actor, 0);
    return seconds * multiplier *
           (std::hypot(tx - home.x, ty - home.y) <= bomber_short_medium_limit
                ? bomber_short_medium_speed
                : bomber_long_speed);
}
inline constexpr double bomber_return_seconds = 1.8;
inline bool bomber_returns(const MediaEvidence &e) {
    return e.supported && e.action == 5 && e.effect >= 3 && e.effect <= 12;
}
inline BomberPose bomber_return(std::size_t actor, double seconds, double tx, double ty) {
    auto home = bomber_orbit(actor, 0);
    double t = std::clamp(seconds / bomber_return_seconds, 0., 1.);
    return {tx + (home.x - tx) * t, ty + (home.y - ty) * t,
            bomber_heading(home.x - tx, home.y - ty)};
}
inline constexpr double bomber_drop_seconds = 2.8;
inline std::optional<BomberPose> bomber_sequence(std::size_t actor, double seconds, double tx,
                                                 double ty, const MediaEvidence &e) {
    if (seconds < 0)
        return std::nullopt;
    if (seconds < bomber_drop_seconds)
        return bomber_attack(actor, seconds, tx, ty);
    if (!bomber_returns(e) || seconds >= bomber_drop_seconds + bomber_return_seconds)
        return std::nullopt;
    return bomber_return(actor, seconds - bomber_drop_seconds, tx, ty);
}
inline bool defector_flip(double source_x, double target_x) { return target_x < source_x; }
struct SelectorPixel {
    int x, y;
};
inline std::vector<SelectorPixel> selector_pixels(Rect city, double seconds,
                                                  Rect bounds = {0, 0, 320, 200}) {
    int cx = city.x + city.w / 2, cy = city.y + city.h / 2;
    int outer = std::min(14, std::max(2, std::min({cx - bounds.x, bounds.x + bounds.w - 1 - cx,
                                                   cy - bounds.y, bounds.y + bounds.h - 1 - cy}) -
                                             5));
    int inner = std::max(2, outer - 3);
    int gap =
        inner + int((outer - inner) * .5 * (1 + std::cos(seconds * 2 * 3.141592653589793 / .8)));
    std::vector<SelectorPixel> pixels;
    for (auto direction :
         std::array<Rect, 4>{{{0, 1, 0, 0}, {0, -1, 0, 0}, {1, 0, 0, 0}, {-1, 0, 0, 0}}}) {
        int dx = direction.x, dy = direction.y;
        for (int depth = 0; depth < 3; ++depth)
            for (int side = -depth; side <= depth; ++side)
                pixels.push_back(
                    {cx - dx * (gap + depth) - dy * side, cy - dy * (gap + depth) + dx * side});
        for (int stem = 3; stem < 5; ++stem)
            pixels.push_back({cx - dx * (gap + stem), cy - dy * (gap + stem)});
    }
    return pixels;
}
inline bool selector_visible(const session::Data &d, session::Phase phase, std::size_t view,
                             bool presenting) {
    auto &h = d.state.actors[4];
    return !presenting && phase == session::Phase::human_selection &&
           d.human.selection.pending_action != 6 && d.human.selection.pending_action != 7 &&
           d.human.target_marker != 0xffff && h.target_actor == view && h.target_city < 5;
}
struct Indicator {
    Rect box;
    bool available;
};
inline std::vector<Indicator> availability(const Actor &a) {
    std::vector<Indicator> r;
    for (int i = 0; i < 4; ++i) {
        r.push_back({{61, 58 + 9 * i, 3, 3}, a.missiles[i] != 0});
        r.push_back({{61, 107 + 9 * i, 3, 3}, a.warheads[i] != 0});
    }
    for (int i = 0; i < 2; ++i) {
        r.push_back({{256, 61 + 18 * i, 3, 3}, a.bombers[i] != 0});
        r.push_back({{256, 109 + 18 * i, 3, 3}, a.defenses[i] != 0});
    }
    return r;
}
inline std::string reaction_text(Word identity, Word roll) {
    if (identity >= 10 || roll < 1 || roll > 69)
        return {};
    try {
        return std::string(
            nuclear::text::table(roll < 4 ? "reaction_identity_speech" : "reaction_general_speech",
                                 roll < 4 ? unsigned(identity * 3 + roll - 1) : unsigned(roll - 4))
                .text);
    } catch (const std::exception &) {
        return {};
    }
}
inline Rect bubble_rect(std::size_t actor, std::size_t chars) {
    if (actor >= 4)
        return {};
    int width = std::min(144, 8 * ((int(std::min<std::size_t>(chars, 26)) * 5 + 19) / 8)),
        x = actor < 2 ? 63 : 257 - width, y = actor % 2 ? (chars > 26 ? 180 : 187) : 38;
    return {x, y, width, chars > 26 ? 20 : 13};
} // native anchor and tile-width rounding; caller truncates overlong one-line speech
// 113170 envelope, but private deterministic noise instead of original live LCG words.
inline std::vector<std::int8_t> ticker_wave() {
    std::vector<std::int8_t> v;
    std::uint32_t private_noise = 0x4e575449;
    for (int divisor = 255; divisor > 0; divisor -= 2) {
        private_noise = private_noise * 1664525u + 1013904223u;
        int n = int((private_noise >> 16) & 32767) % divisor + 1 - divisor / 2;
        v.push_back(std::int8_t(n));
    }
    return v;
}
} // namespace nuclear::player
