#pragma once
#include "core/config.hpp"
#include "ui/target_selection.hpp"
namespace nuclear::player {
inline bool action_selection_shutter_owner(Screen screen, bool alive, bool ended,
                                           session::Phase phase, bool presenting, bool inset) {
    return target_shutter_owner(screen, alive, ended, phase, presenting) && inset;
}
inline bool shutter_selection_action(Word action) {
    return action == 0 || action == 2 || action == 4 || action == 7 || action == 100;
}
// Presentation-only central content. Content0 is the country-map return, not
// a gameplay action. Ownership must survive a covered action-to-map transition.
// Commands have already run through the existing verified selection handler.
class ActionSelectionShutters {
    Word visible_{}, requested_{};
    std::uint64_t seen_{}, visible_serial_{}, requested_serial_{}, cycle_{};
    unsigned phase_{};
    double elapsed_{}, visible_seconds_{}, duration_{SELECTION_SHUTTER_SECONDS};
    bool attached_{};

  public:
    static constexpr double leg = SELECTION_SHUTTER_SECONDS;
    double leg_seconds() const { return duration_; }
    void duration(double seconds) {
        if (std::isfinite(seconds) && seconds >= .1 && seconds <= 2)
            duration_ = seconds;
    }
    void observe(std::uint64_t serial, Word action, bool allowed) {
        if (!allowed || !shutter_selection_action(action)) {
            seen_ = serial;
            visible_serial_ = requested_serial_ = serial;
            visible_ = requested_ = 0;
            phase_ = 0;
            elapsed_ = visible_seconds_ = 0;
            attached_ = false;
            return;
        }
        if (!attached_) {
            attached_ = true;
            // Re-entering an owning scene must not replay a stale accepted command.
            if (serial <= seen_) {
                visible_ = requested_ = action;
                visible_serial_ = requested_serial_ = serial;
                return;
            }
        }
        if (serial > seen_) {
            seen_ = serial;
            requested_ = action;
            requested_serial_ = serial;
        }
        if (!phase_ && requested_serial_ != visible_serial_) {
            phase_ = 1;
            elapsed_ = 0;
            ++cycle_;
        }
    }
    void tick(double dt) {
        if (!std::isfinite(dt) || dt < 0)
            return;
        if (visible_)
            visible_seconds_ += dt;
        if (phase_ == 1 || phase_ == 3) {
            elapsed_ = std::min(duration_, elapsed_ + dt);
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
    // Must run only after SDL_RenderPresent: fully closed cover precedes the switch.
    void presented() {
        if (phase_ == 2) {
            visible_ = requested_;
            visible_serial_ = requested_serial_;
            visible_seconds_ = 0;
            phase_ = 3;
            elapsed_ = 0;
        }
    }
    bool active() const { return phase_ != 0; }
    unsigned phase() const { return phase_; }
    Word visible() const { return visible_; }
    Word requested() const { return requested_; }
    double visible_seconds() const { return visible_seconds_; }
    double fraction() const {
        return phase_ == 2   ? 1
               : phase_ == 1 ? elapsed_ / duration_
               : phase_ == 3 ? 1 - elapsed_ / duration_
                             : 0;
    }
    std::uint64_t audio_token() const { return 0xa000000000000000ull | (cycle_ << 3) | phase_; }
};
} // namespace nuclear::player
