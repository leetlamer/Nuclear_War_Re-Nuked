#pragma once
#include "core/config.hpp"
#include "platform/resources.hpp"
#include "rendering/action_media.hpp"
#include "ui/interface_layout.hpp"
#include <SDL.h>
#include <fstream>
#include <map>
namespace nuclear::player {
// Original signed8 payloads; default clock is application policy; per-sample periods remain
// authoritative.
class SoundManager {
    SDL_AudioDeviceID device_{}, ticker_device_{};
    std::uint64_t ticker_after_{};
    std::string ticker_text_;
    bool ticker_owns_queue_{};
    SDL_AudioSpec spec_{};
    SoundGate gate_;
    std::vector<std::int16_t> loop_pcm_;
    std::uint64_t loop_token_{};
    bool looping_{};
    std::vector<std::int16_t> production_pcm_, production_tick_;
    std::size_t production_pos_{}, production_tick_pos_{};
    bool production_mixing_{};
    double clock_{};
    std::map<std::string, unsigned> rates_;
    bool muted_{};
    int volume_{96};

  public:
    std::string status;
    SoundManager(double clock = DEFAULT_AUDIO_CLOCK, std::map<std::string, unsigned> rates = {})
        : clock_(clock), rates_(std::move(rates)) {
        SDL_SetMainReady();
        if (clock_ <= 0 && rates_.empty()) {
            status = "Audio off: supply explicit clock or sample rates";
            return;
        }
        if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
            status = SDL_GetError();
            return;
        }
        SDL_AudioSpec desired{};
        desired.freq = 48000;
        desired.format = AUDIO_S16SYS;
        desired.channels = 1;
        desired.samples = 1024;
        device_ = SDL_OpenAudioDevice(nullptr, 0, &desired, &spec_, 0);
        if (!device_) {
            status = SDL_GetError();
            return;
        }
        SDL_PauseAudioDevice(device_, 0);
        ticker_device_ = device_;
        status = "Configured original PCM preview";
    }
    SoundManager(const SoundManager &) = delete;
    SoundManager &operator=(const SoundManager &) = delete;
    ~SoundManager() {
        if (device_)
            SDL_CloseAudioDevice(device_);
    }
    unsigned rate_for(const SoundCue &cue) const {
        auto override = rates_.find(cue.sample);
        double base =
            override != rates_.end() ? override->second : (clock_ > 0 ? clock_ / cue.period : 0);
        return unsigned(std::lround(base * cue.playback_rate));
    }
    double effective_clock() const { return clock_; }
    unsigned queued_bytes() const { return device_ ? SDL_GetQueuedAudioSize(device_) : 0; }
    void stop() {
        production_mixing_ = false;
        production_pcm_.clear();
        production_tick_.clear();
        production_pos_ = production_tick_pos_ = 0;
        if (device_)
            SDL_ClearQueuedAudio(device_);
        ticker_owns_queue_ = false;
        looping_ = false;
        loop_pcm_.clear();
    }
    void new_session() {
        stop();
        gate_ = SoundGate{};
        ticker_after_ = 0;
        ticker_text_.clear();
        ticker_plays = last_plays = 0;
        loop_token_ = 0;
    }
    bool defence_looping() const { return looping_; }
    void end_defence() {
        if (looping_)
            stop();
    }
    bool defence_loop(std::uint64_t token, bool active) {
        if (!active || muted_ || !device_) {
            end_defence();
            return false;
        }
        if (!looping_ || token != loop_token_) {
            stop();
            MediaEvidence e;
            e.supported = true;
            e.action = 7;
            auto cue = *sound_cue(e);
            auto pcm = decode(cue);
            if (pcm.empty())
                return false;
            loop_pcm_ = std::move(pcm);
            looping_ = true;
            loop_token_ = token;
        }
        if (SDL_GetQueuedAudioSize(device_) < loop_pcm_.size() * 2) {
            SDL_QueueAudio(device_, loop_pcm_.data(), Uint32(loop_pcm_.size() * 2));
            return true;
        }
        return false;
    }
    bool bleep(unsigned period) {
        if (!device_ || muted_ || !period)
            return false;
        stop();
        unsigned rate = unsigned(std::lround(clock_ / period));
        if (rate < 100 || rate > 192000)
            return false;
        auto wave = nuclear::assets::wave("selection_bleep").samples;
        auto stream =
            SDL_NewAudioStream(AUDIO_S8, 1, int(rate), spec_.format, spec_.channels, spec_.freq);
        if (!stream)
            return false;
        SDL_AudioStreamPut(stream, wave.data(), int(wave.size()));
        SDL_AudioStreamFlush(stream);
        std::vector<std::int16_t> pcm(std::max(0, SDL_AudioStreamAvailable(stream)) / 2);
        SDL_AudioStreamGet(stream, pcm.data(), int(pcm.size() * 2));
        SDL_FreeAudioStream(stream);
        for (auto &v : pcm)
            v = std::int16_t(std::int32_t(v) * volume_ * 14 / (128 * 64));
        return !pcm.empty() && SDL_QueueAudio(device_, pcm.data(), Uint32(pcm.size() * 2)) == 0;
    }

    std::vector<std::int16_t> decode(const SoundCue &cue) {
        unsigned rate = rate_for(cue);
        if (rate < 100 || rate > 192000)
            return {};
        auto source = nuclear::assets::wave(cue.sample);
        if (source.samples.size() < cue.length)
            return {};
        std::vector<unsigned char> payload(source.samples.begin(),
                                           source.samples.begin() + cue.length);
        auto stream =
            SDL_NewAudioStream(AUDIO_S8, 1, int(rate), spec_.format, spec_.channels, spec_.freq);
        if (!stream)
            return {};
        SDL_AudioStreamPut(stream, payload.data(), int(payload.size()));
        SDL_AudioStreamFlush(stream);
        std::vector<std::int16_t> pcm(std::max(0, SDL_AudioStreamAvailable(stream)) / 2);
        SDL_AudioStreamGet(stream, pcm.data(), int(pcm.size() * 2));
        SDL_FreeAudioStream(stream);
        for (auto &v : pcm)
            v = std::int16_t(std::int32_t(v) * volume_ * int(cue.volume) / (128 * 64));
        if (cue.smooth_edges) {
            const auto fade = std::min<std::size_t>(240, pcm.size() / 2);
            for (std::size_t i = 0; i < fade; ++i) {
                pcm[i] = std::int16_t(std::int32_t(pcm[i]) * int(i) / int(fade));
                auto j = pcm.size() - 1 - i;
                pcm[j] = std::int16_t(std::int32_t(pcm[j]) * int(i) / int(fade));
            }
        }
        return pcm;
    }
    void mute() {
        muted_ = !muted_;
        if (muted_)
            stop();
    }
    void volume(int delta) {
        volume_ = std::clamp(volume_ + delta, 0, 128);
        end_defence();
    }
    bool muted() const { return muted_; }
    int volume() const { return volume_; }
    bool available() const { return device_ != 0; }
    unsigned ticker_plays{}, last_plays{};
    // Only the production exclamation mixes with ongoing console characters.
    // Keep a short queue so subsequent glyph pulses can be mixed without a modal wait.
    bool production(std::uint64_t token, const SoundCue &cue) {
        if (!gate_.claim(token))
            return false;
        stop();
        if (!device_ || muted_)
            return false;
        production_pcm_ = decode(cue);
        if (production_pcm_.empty())
            return false;
        production_mixing_ = true;
        last_plays = 1;
        pump_production();
        return true;
    }
    void pump_production() {
        if (!production_mixing_ || !device_)
            return;
        while (SDL_GetQueuedAudioSize(device_) < 11520) {
            std::vector<std::int16_t> block;
            block.reserve(960);
            for (unsigned i = 0; i < 960 && (production_pos_ < production_pcm_.size() ||
                                             production_tick_pos_ < production_tick_.size());
                 ++i) {
                int v = production_pos_ < production_pcm_.size()
                            ? production_pcm_[production_pos_++]
                            : 0;
                if (production_tick_pos_ < production_tick_.size())
                    v += production_tick_[production_tick_pos_++];
                block.push_back(std::int16_t(std::clamp(v, -32768, 32767)));
            }
            if (block.empty()) {
                if (!SDL_GetQueuedAudioSize(device_))
                    production_mixing_ = false;
                break;
            }
            SDL_QueueAudio(device_, block.data(), Uint32(block.size() * 2));
        }
    }
    bool ticker(const std::string &visible, bool typing, std::uint64_t now) {
        bool changed = visible != ticker_text_;
        ticker_text_ = visible;
        if (!ticker_device_ || muted_ || !typing) {
            if (ticker_device_ && ticker_owns_queue_) {
                SDL_ClearQueuedAudio(ticker_device_);
                ticker_owns_queue_ = false;
            }
            return false;
        }
        if (!changed || visible.empty() || visible.back() == ' ' || visible.back() == '\n' ||
            now < ticker_after_ || (!production_mixing_ && SDL_GetQueuedAudioSize(ticker_device_)))
            return false;
        unsigned rate = clock_ > 0 ? unsigned(std::lround(clock_ / 127)) : 0;
        auto hz = rates_.find("ticker");
        if (hz != rates_.end())
            rate = hz->second;
        if (rate < 100 || rate > 192000)
            return false;
        auto waveform = nuclear::assets::wave("ticker").samples;
        auto stream = SDL_NewAudioStream(AUDIO_S8, 1, int(rate), AUDIO_S16SYS, 1, 48000);
        if (!stream)
            return false;
        SDL_AudioStreamPut(stream, waveform.data(), int(waveform.size()));
        SDL_AudioStreamFlush(stream);
        std::vector<std::int16_t> pcm(std::max(0, SDL_AudioStreamAvailable(stream)) / 2);
        int n = SDL_AudioStreamGet(stream, pcm.data(), int(pcm.size() * 2));
        SDL_FreeAudioStream(stream);
        if (n <= 0)
            return false;
        for (auto &v : pcm)
            v = std::int16_t(std::int32_t(v) * volume_ * 10 / (128 * 64));
        bool ok;
        if (production_mixing_) {
            production_tick_ = std::move(pcm);
            production_tick_pos_ = 0;
            ok = true;
            pump_production();
        } else
            ok = SDL_QueueAudio(ticker_device_, pcm.data(), Uint32(n)) == 0;
        if (ok) {
            ++ticker_plays;
            ticker_owns_queue_ = !production_mixing_;
            ticker_after_ = now + 40;
        }
        return ok;
    }

    bool play(std::uint64_t token, std::optional<SoundCue> cue) {
        if (!gate_.claim(token))
            return false;
        stop();
        if (!device_ || muted_)
            return false;
        last_plays = 0;
        if (!cue)
            return false;
        unsigned rate = rate_for(*cue);
        if (rate < 100 || rate > 192000)
            return false;
        auto source = nuclear::assets::wave(cue->sample);
        if (source.samples.size() < cue->length)
            return false;
        std::vector<unsigned char> payload(source.samples.begin(),
                                           source.samples.begin() + cue->length);
        auto stream =
            SDL_NewAudioStream(AUDIO_S8, 1, int(rate), spec_.format, spec_.channels, spec_.freq);
        if (!stream)
            return false;
        bool ok = SDL_AudioStreamPut(stream, payload.data(), int(payload.size())) == 0 &&
                  SDL_AudioStreamFlush(stream) == 0;
        if (ok) {
            int n = SDL_AudioStreamAvailable(stream);
            if (n > 0) {
                std::vector<std::int16_t> out(std::size_t(n) / 2);
                n = SDL_AudioStreamGet(stream, out.data(), int(out.size() * 2));
                if (n > 0) {
                    for (auto &v : out)
                        v = std::int16_t(std::int32_t(v) * volume_ * int(cue->volume) / (128 * 64));
                    for (unsigned repeat = 0; repeat < cue->repeats; ++repeat) {
                        ok = SDL_QueueAudio(device_, out.data(), Uint32(n)) == 0;
                        if (!ok)
                            break;
                        ++last_plays;
                    }
                } else
                    ok = false;
            } else
                ok = false;
        }
        SDL_FreeAudioStream(stream);
        return ok;
    }
    bool event(std::uint64_t token, const MediaEvidence &evidence) {
        return play(token, sound_cue(evidence));
    }
};
} // namespace nuclear::player
