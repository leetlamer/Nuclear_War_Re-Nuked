#pragma once
#include "rendering/animations/special_events.hpp"
namespace nuclear::player {
// 11245E: original threshold treats population20 as tile222, unlike normal city UI.
inline Word earthquake_city_frame(Word population) {
    int p = signed_word(population);
    return p >= 60   ? 151
           : p >= 30 ? 224
           : p >= 21 ? 223
           : p >= 10 ? 222
           : p >= 5  ? 221
           : p != 0  ? 220
                     : 150;
}
inline SoundCue earthquake_sound() { return {"exp", 1300, 4998, 12, 2}; }
// Original art/cropping/order, with configurable HOST seconds. No global/world
// shake is established by this handler. Logical coordinates never move.
inline std::vector<DrawSprite> earthquake_sprites(double seconds, int city_x, int city_y,
                                                  Word population, double duration = 3.) {
    if (!std::isfinite(duration) || duration <= 0)
        throw Unsupported("invalid quake duration");
    std::vector<DrawSprite> out;
    if (!std::isfinite(seconds) || seconds < 0 || seconds >= duration)
        return out;
    // Native frame<800, ground counter starts16 and advances330->332->334->336;
    // after ground336, remaining city height decrements toward1 every10 calls.
    // Mapping calls to800/duration is INFERRED host timing, not measured cadence.
    int step = std::min(799, int(seconds / duration * 800));
    int ground = 330 + 2 * std::min(3, step / 16);
    int remaining = 16;
    if (step >= 48)
        remaining = std::max(1, 15 - (step - 48) / 10);
    auto city = small_sprite(earthquake_city_frame(population), 16, remaining, city_x,
                             city_y + 16 - remaining);
    out.push_back(city);
    out.push_back(small_sprite(ground, 32, 16, std::clamp(city_x - 8, 0, 287), city_y + 9));
    return out;
}
} // namespace nuclear::player
