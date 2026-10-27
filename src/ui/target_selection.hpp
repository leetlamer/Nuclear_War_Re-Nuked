#pragma once
#include "core/config.hpp"
#include "ui/selection_chrome.hpp"
namespace nuclear::player {
// Read-only conversion10DCF4; signed DIVS truncates toward zero, no RNG.
inline std::optional<int> ai_red_number(const State &s, std::size_t actor, std::size_t view) {
    if (actor >= 4 || view >= 5 || !populated_cities(s.actors[actor]))
        return std::nullopt;
    const Word raw = s.actors[actor == view ? 4 : actor].scores[view];
    int q = int((std::int32_t(signed_word(raw)) + 32767) / 661);
    return s.actors[actor].profile == 4 ? q : 99 - q;
}
inline bool ai_selection_active(session::Phase phase, bool human_alive, bool ended, bool presenting,
                                bool inset) {
    return phase == session::Phase::human_selection && human_alive && !ended && !presenting &&
           !inset;
}
inline Rect ai_marker_rect(std::size_t actor) {
    return {actor < 2 ? 46 : 297, actor % 2 ? 170 : 21, 16, 16};
}
inline Word ai_marker_frame(double seconds, double period = 1.12) {
    int step = int(std::max(0., seconds) / (period / 14)) % 14;
    return Word(320 + (step <= 7 ? step : 14 - step));
}
inline Word city_cursor_frame(double seconds, double period = .6) {
    int step = int(std::max(0., seconds) / (period / 10)) % 10;
    return Word(227 + (step <= 5 ? step : 10 - step));
}
inline Rect city_cursor_rect(Rect city) {
    return {city.x + (city.w - 16) / 2, city.y + (city.h - 16) / 2, 16, 16};
}
inline bool target_shutter_owner(Screen screen, bool alive, bool ended, session::Phase phase,
                                 bool presenting) {
    return screen == Screen::main && alive && !ended && phase == session::Phase::human_selection &&
           !presenting;
}
// Presentation ownership only. Commands run immediately through verified engine;
// the immutable view INDEX shown on screen switches only while covered.
class TargetViewShutters {
    Word visible_{4}, requested_{4};
    bool attached_{};
    unsigned phase_{};
    double elapsed_{};
    std::uint64_t serial_{};
    double duration_{SELECTION_SHUTTER_SECONDS};

  public:
    static constexpr double leg = SELECTION_SHUTTER_SECONDS;
    void duration(double d) {
        if (std::isfinite(d) && d >= .1 && d <= 2)
            duration_ = d;
    }
    void observe(Word requested, bool allowed) {
        if (requested > 4)
            requested = 4;
        if (!allowed) {
            visible_ = requested_ = requested;
            phase_ = 0;
            attached_ = false;
            elapsed_ = 0;
            return;
        }
        if (!attached_) {
            visible_ = requested_ = requested;
            attached_ = true;
            return;
        }
        if (!phase_ && requested != visible_) {
            requested_ = requested;
            elapsed_ = 0;
            phase_ = 1;
            ++serial_;
        }
    }
    void tick(double dt) {
        if (phase_ == 1 || phase_ == 3) {
            elapsed_ = std::min(duration_, elapsed_ + std::max(0., dt));
            if (elapsed_ >= duration_) {
                if (phase_ == 1)
                    phase_ = 2;
                else {
                    phase_ = 0;
                    elapsed_ = 0;
                }
            }
        }
    }
    // Called only after the fully closed image was actually presented.
    void presented() {
        if (phase_ == 2) {
            visible_ = requested_;
            phase_ = 3;
            elapsed_ = 0;
        }
    }
    bool active() const { return phase_ != 0; }
    Word visible() const { return visible_; }
    Word requested() const { return requested_; }
    unsigned phase() const { return phase_; }
    std::uint64_t audio_token() const { return 0xb000000000000000ull | (serial_ << 3) | phase_; }
    bool opening() const { return phase_ == 3; }
    double fraction() const {
        return phase_ == 2   ? 1
               : phase_ == 1 ? elapsed_ / duration_
               : phase_ == 3 ? 1 - elapsed_ / duration_
                             : 0;
    }
};
struct VictoryPose {
    Rect body_source, head_source, body_destination, head_destination;
    int height;
};
inline VictoryPose victory_pose(Word identity, double elapsed, double jump_seconds = 1.2) {
    if (identity >= 10)
        throw Unsupported("victory identity outside original ten heads");
    // Native stepped 0..15..0, with floor(height/6)+1 wait weights (60/cycle).
    double phase = elapsed >= 3 * jump_seconds
                       ? 0
                       : std::fmod(std::max(0., elapsed), jump_seconds) / jump_seconds;
    int tick = std::min(59, int(phase * 60)), height = 0;
    for (int i = 0; i < 32; ++i) {
        int h = i < 16 ? i : 31 - i;
        int waits = h / 6 + 1;
        if (tick < waits) {
            height = h;
            break;
        }
        tick -= waits;
    }
    constexpr int columns[]{0, 1, 6, 9, 4, 7, 2, 5, 3, 8};
    return {{80 * (height / 6), 48, 80, 52},
            {32 * columns[identity], 0, 32, 32},
            {148, 93 - height, 80, 52},
            {171, 85 - height, 32, 32},
            height};
}
// Return at most3 single-play requests, never overlap a queued sample. Elapsed
// phases may wait for actual queue drain. Missing/muted audio does not deadlock.
class VictorySoundSequence {
    unsigned requested_{};

  public:
    std::optional<unsigned> next(double elapsed, double jump_seconds, bool busy) {
        if (requested_ == 3 || busy || elapsed < requested_ * jump_seconds)
            return std::nullopt;
        return requested_++;
    }
    bool complete(bool busy) const { return requested_ == 3 && !busy; }
    unsigned requests() const { return requested_; }
};
} // namespace nuclear::player
