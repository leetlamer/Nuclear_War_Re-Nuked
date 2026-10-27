#include "audio/audio_manager.hpp"
#include "rendering/animations/console_and_sound_cues.hpp"
#include <cstdlib>
#include <filesystem>
#include <iostream>
using namespace nuclear::player;
void check_at(bool ok, int line) {
    if (!ok) {
        std::cerr << "audio test failed line " << line << "\n";
        std::exit(1);
    }
}
#define check(x) check_at(bool(x), __LINE__)
int main() {
    SDL_SetHint(SDL_HINT_AUDIODRIVER, "dummy");
    SDL_Init(0);
    {
        auto stored = nuclear::assets::wave("ticker").samples;
        auto original = ticker_wave();
        check(stored.size() == original.size());
        for (std::size_t i = 0; i < stored.size(); ++i)
            check(stored[i] == static_cast<unsigned char>(original[i]));
        std::vector<unsigned char> expected;
        for (int v = 1; v < 127; ++v) {
            expected.push_back(static_cast<unsigned char>(v));
            expected.push_back(static_cast<unsigned char>(-v));
        }
        for (int v = 127; v > 0; --v) {
            expected.push_back(static_cast<unsigned char>(v));
            expected.push_back(static_cast<unsigned char>(-v));
        }
        check(nuclear::assets::wave("selection_bleep").samples == expected);
    }
    {
        SoundManager defaulted;
        check(defaulted.available());
        check(defaulted.effective_clock() == 3546895);
        SoundCue cue{"exp", 650, 4998, 20};
        check(defaulted.rate_for(cue) == 5457);
        SoundManager explicit_clock(DEFAULT_AUDIO_CLOCK, {});
        check(defaulted.rate_for(cue) == explicit_clock.rate_for(cue));
        SoundManager override_clock(4000000, {});
        check(override_clock.effective_clock() == 4000000 && override_clock.rate_for(cue) == 6154);
        SoundManager sample_override(DEFAULT_AUDIO_CLOCK, {{"exp", 12345}});
        check(sample_override.rate_for(cue) == 12345);
    }
    {
        SoundManager loop;
        check(loop.bleep(700));
        check(loop.queued_bytes() > 0);
        loop.stop();
        check(loop.defence_loop(1, true));
        check(loop.defence_looping() && loop.queued_bytes() > 0);
        loop.defence_loop(1, true);
        check(loop.defence_looping());
        loop.defence_loop(1, false);
        check(!loop.defence_looping() && loop.queued_bytes() == 0);
        check(loop.defence_loop(2, true));
        loop.mute();
        check(!loop.defence_looping() && loop.queued_bytes() == 0);
        check(!loop.defence_loop(3, true));
        loop.mute();
        check(loop.defence_loop(3, true));
        loop.stop();
        check(!loop.defence_looping());
    }
    {
        SoundManager rates;
        auto fast = production_100mt_sound();
        auto normal = fast;
        normal.playback_rate = 1.;
        check(rates.rate_for(normal) == 4961 && rates.rate_for(fast) == 5358);
        check(rates.rate_for(SoundCue{"ooo", 300, 8820, 24}) == 11823);
        check(rates.effective_clock() == 3546895);
        auto a = rates.decode(normal), b = rates.decode(fast);
        check(!a.empty() && !b.empty());
        check(double(a.size()) / b.size() > 1.079 && double(a.size()) / b.size() < 1.081);
    }
    {
        SoundManager clean;
        auto cue = production_100mt_sound();
        auto raw = cue;
        raw.smooth_edges = false;
        auto a = clean.decode(raw), b = clean.decode(cue);
        check(a.size() == b.size() && a.size() > 480);
        check(b.front() == 0 && b.back() == 0);
        check(a[300] == b[300] && a[a.size() / 2] == b[b.size() / 2]);
        check(clean.production(0x7777, cue));
        check(clean.queued_bytes() >= 11520);
        SDL_Delay(80);
        check(clean.queued_bytes() > 0);
        clean.stop();
    }
    {
        SoundManager production;
        auto cue = production_100mt_sound();
        check(production.play(0x9000, cue));
        auto queued = production.queued_bytes();
        check(queued > 0 && production.last_plays == 1);
        check(!production.ticker("02 100MT Warheads", false, 1000));
        check(production.queued_bytes() > 0);
        check(!production.play(0x9000, cue));
        production.stop();
        check(production.ticker("New message", true, 2000));
        check(production.ticker_plays == 1);
    }
    {
        SoundManager reset;
        reset.volume(-8);
        auto cue = production_100mt_sound();
        check(reset.play(77, cue));
        reset.new_session();
        check(reset.queued_bytes() == 0 && reset.volume() == 88 &&
              reset.effective_clock() == 3546895);
        check(reset.play(77, cue));
        reset.mute();
        reset.new_session();
        check(reset.muted() && reset.volume() == 88 && !reset.play(78, cue));
    }
    {
        SoundManager mix;
        auto cue = production_100mt_sound();
        check(mix.production(0x9100, cue));
        check(!mix.production(0x9100, cue));
        check(mix.ticker("A", true, 1000));
        check(mix.ticker_plays == 1);
        check(!mix.ticker("A", true, 1100));
        check(mix.queued_bytes() > 0);
        mix.pump_production();
        mix.stop();
        check(mix.queued_bytes() == 0);
    }
    MediaEvidence e;
    e.supported = true;
    e.action = 2;
    {
        SoundManager victory;
        check(victory.play(900, SoundCue{"iwon", 370, 5913, 32, 3}));
        check(victory.last_plays == 3 && victory.queued_bytes() > 0);
        check(!victory.play(900, SoundCue{"iwon", 370, 5913, 32, 3}));
        victory.stop();
        check(victory.queued_bytes() == 0);
    }
    {
        SoundManager audio(3546895, {});
        check(audio.available());
        e.action = 7;
        check(audio.event(100, e));
        check(audio.last_plays == 1);
        check(!audio.event(100, e));
        audio.stop();
        e.action = 2;
        check(audio.ticker("A", true, 100));
        check(!audio.ticker("A", true, 200));
        check(!audio.ticker("AB", true, 101));
        check(!audio.ticker("ABC", false, 300));
        check(audio.event(1, e));
        check(!audio.event(1, e));
        e.action = 6;
        check(audio.event(10, e));
        check(audio.last_plays == 2);
        check(audio.queued_bytes() > 0);
        check(!audio.event(10, e));
        audio.stop();
        check(audio.queued_bytes() == 0);
        e.action = 1;
        e.effect = 6;
        e.loss = 9;
        check(audio.play(11, impact_sound(e)));
        check(audio.last_plays == 1);
        check(!audio.play(11, impact_sound(e)));
        e.effect = 13;
        check(!audio.play(12, impact_sound(e)));
        e.action = 8;
        check(audio.event(13, e));
        check(audio.last_plays == 1);
        e.action = 13;
        check(audio.event(14, e));
        check(audio.last_plays == 1);
        for (unsigned a : {9u, 11u, 12u, 14u, 15u, 16u}) {
            e.action = nuclear::Word(a);
            check(audio.event(1000 + a, e));
            check(audio.last_plays == 1 && audio.queued_bytes() > 0);
            check(!audio.event(1000 + a, e));
            audio.stop();
        }
        e.action = 2;
        audio.mute();
        check(!audio.event(2, e));
        audio.mute();
        audio.volume(1000);
        check(audio.volume() == 128);
        audio.volume(-1000);
        check(audio.volume() == 0);
        check(!audio.play(3, SoundCue{"missing_asset", 715, 100, 24}));
        e.action = 99;
        check(!audio.event(4, e));
    }
    SDL_Quit();
    SDL_SetHint(SDL_HINT_AUDIODRIVER, "invalid-no-audio-driver");
    {
        SoundManager missing(3546895, {});
        check(!missing.available());
        check(!missing.event(1, e));
    }
    SDL_Quit();
    std::cout << "PASS original signed8 queued playback, once/event, mute, volume, missing file "
                 "and unavailable device\n";
}
