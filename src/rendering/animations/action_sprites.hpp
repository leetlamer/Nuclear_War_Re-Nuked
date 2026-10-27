#pragma once
#include "ui/player_controller.hpp"
#include <cmath>
namespace nuclear::player {
struct SpriteFrame {
    int index, width, height;
};
// Bounded cached native handler ranges; seconds are prototype timing only.
inline std::optional<SpriteFrame> action_sprite(Word action, double seconds) {
    int frame = int(std::fmod(std::max(0.0, seconds) * 8.0, 6.0));
    switch (action) {
    case 6:
        return SpriteFrame{160 + frame, 16, 16}; // 11729a
    case 3:
        return SpriteFrame{10 + frame, 16, 21}; // 116c84
    case 14:
        return SpriteFrame{250 + int(std::fmod(std::max(0.0, seconds) * 8.0, 5.0)), 16,
                           16}; // 110f64, launcher ownership remains unresolved
    case 8:
        return SpriteFrame{240 + int(std::fmod(std::max(0.0, seconds) * 8.0, 10.0)), 16,
                           18}; // 110cb2: native height0x12, width0x10
    case 4:
        return SpriteFrame{(seconds < .75 ? 209 + int(std::max(0.0, seconds) * 8.0)
                                          : 207 + int(std::fmod((seconds - .75) * 8.0, 8.0))),
                           16, 16}; // 1169f6
    case 7: {
        int n = int(std::fmod(std::max(0.0, seconds) * 8.0, 12.0));
        return SpriteFrame{53 + (n <= 6 ? n : 12 - n), 16, 16};
    } // 112c42
    default:
        return std::nullopt;
    }
}
inline Rect action_origin(Word action, std::size_t actor) {
    constexpr Rect origins[]{
        {57, 48, 0, 0}, {44, 152, 0, 0}, {260, 55, 0, 0}, {270, 150, 0, 0}, {143, 101, 0, 0}};
    if (actor >= 5)
        throw Unsupported("invalid presentation actor");
    auto origin = origins[actor];
    if (action == 4)
        origin.y -= 32;
    return origin;
}
// Same mounting point as action2 preparation: missile center at fully raised pad.
inline Rect missile_launch_origin(std::size_t actor) {
    auto pad = action_origin(2, actor);
    return {pad.x + 8, pad.y + 1, 0, 0};
}
inline bool prepared_missile(const State &s) {
    return s.actors[4].action == 2 && signed_word(s.human_prepared_capacity) > 0;
}
inline Rect centered_effect(int x, int y, int size) {
    return {x - size / 2, y - size / 2, size, size};
}
} // namespace nuclear::player
