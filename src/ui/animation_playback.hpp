#pragma once
#include <algorithm>
#include <cstddef>
#include <cstdint>
namespace nuclear::player {
enum class PlaybackMode { normal, fast_forward, instant_complete };
enum class PlaybackRelease { ignored, consumed, instant_complete };
struct PlaybackKey {
    std::uint64_t serial{};
    std::size_t action{};
    bool operator!=(const PlaybackKey &b) const { return serial != b.serial || action != b.action; }
};
// Captures only an already-visible action gesture. No simulation or SDL calls.
class TurnPlayback {
    PlaybackKey key_{};
    bool down_{}, held_{}, drain_{};
    std::uint64_t pressed_{}, sampled_{}, threshold_{200};
    double extra_ms_{};
    void account(std::uint64_t now) {
        if (!down_)
            return;
        if (now >= pressed_ && now - pressed_ >= threshold_) {
            held_ = true;
            auto start = std::max(sampled_, pressed_ + threshold_);
            if (now > start)
                extra_ms_ += .5 * double(now - start);
        }
        sampled_ = std::max(sampled_, now);
    }

  public:
    void threshold(std::uint64_t ms) { threshold_ = std::clamp<std::uint64_t>(ms, 20, 1000); }
    void cancel() {
        drain_ = drain_ || down_;
        down_ = held_ = false;
        extra_ms_ = 0;
    }
    void synchronize(PlaybackKey key, bool eligible) {
        if (key != key_ || !eligible)
            cancel();
        key_ = key;
    }
    bool press(std::uint64_t now, PlaybackKey key, bool eligible) {
        synchronize(key, eligible);
        if (!eligible)
            return false;
        if (down_)
            return true;
        down_ = true;
        held_ = drain_ = false;
        pressed_ = sampled_ = now;
        extra_ms_ = 0;
        return true;
    }
    PlaybackRelease release(std::uint64_t now, PlaybackKey key, bool eligible) {
        synchronize(key, eligible);
        if (!down_) {
            if (drain_) {
                drain_ = false;
                return PlaybackRelease::consumed;
            }
            return PlaybackRelease::ignored;
        }
        account(now);
        bool click = !held_ && now >= pressed_ && now - pressed_ < threshold_;
        down_ = held_ = drain_ = false;
        return click ? PlaybackRelease::instant_complete : PlaybackRelease::consumed;
    }
    PlaybackMode mode(std::uint64_t now) {
        account(now);
        return held_ ? PlaybackMode::fast_forward : PlaybackMode::normal;
    }
    // Integrate the threshold crossing/release interval, rather than accelerate an
    // entire frame retrospectively. Synthetic SDL replay dt uses the same ratio.
    double seconds(double dt, std::uint64_t from, std::uint64_t to, PlaybackKey key,
                   bool eligible) {
        synchronize(key, eligible);
        account(to);
        double fraction = to > from ? std::clamp(extra_ms_ / double(to - from), 0., .5) : 0.;
        extra_ms_ = 0;
        return dt * (1. + fraction);
    }
    bool captured() const { return down_; }
};
} // namespace nuclear::player
