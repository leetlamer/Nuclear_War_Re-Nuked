#pragma once
#include "core/config.hpp"
#include "nuclear/engine.hpp"
#include <cmath>
#include <iostream>
#include <map>
#include <string>
using namespace nuclear;
using namespace nuclear::player;
struct LaunchOptions {
    std::uint32_t seed = 1;
    double playback_click_at = .2;
    double cursor_period = .6, exhaust_cutoff = .46, victory_jump_seconds = 1.2;
    std::uint64_t hold_ms = 200, propaganda_dismiss_ms = 100;
    double selection_door_seconds = SELECTION_SHUTTER_SECONDS;
    double new_game_fade_seconds = .4;
    double bomber_speed = 1.;
    double audio_clock = DEFAULT_AUDIO_CLOCK, stork_speed = DEFAULT_STORK_SPEED, typing_cps = 35,
           preparation_seconds = .4, finger_seconds = 1.2;
    int explosion_size = 20;
    std::map<std::string, unsigned> rates;
    std::string playback_replay;
    std::string smoke, fixture, session_fixture, city_trace, mood_trace;
    std::size_t replay_actor = 4;
    Word replay_target = 0, replay_city = 2;
    bool restart_test = false;
    bool propaganda_auto_test = false, console_speech_test = false, title_selection_test = false,
         smoke_prepare = false, debug_hitboxes = false;
    bool parse(int argc, char **argv) {
        for (int i = 1; i < argc; ++i) {
            std::string s = argv[i];
            if (s == "--propaganda-dismiss-ms" && i + 1 < argc) {
                propaganda_dismiss_ms = std::stoul(argv[++i]);
                if (propaganda_dismiss_ms < 50 || propaganda_dismiss_ms > 2000)
                    throw std::runtime_error("dismiss50..2000ms");
            } else if (s == "--new-game-fade-seconds" && i + 1 < argc) {
                new_game_fade_seconds = std::stod(argv[++i]);
                if (!std::isfinite(new_game_fade_seconds) || new_game_fade_seconds < .05 ||
                    new_game_fade_seconds > 2)
                    throw std::runtime_error("fade .05..2 seconds");
            } else if (s == "--restart-test")
                restart_test = true;
            else if (s == "--playback-click-at" && i + 1 < argc) {
                playback_click_at = std::stod(argv[++i]);
                if (!std::isfinite(playback_click_at) || playback_click_at < 0 ||
                    playback_click_at > 60)
                    throw std::runtime_error("playback click time0..60");
            } else if (s == "--city-cursor-period" && i + 1 < argc) {
                cursor_period = std::stod(argv[++i]);
                if (!std::isfinite(cursor_period) || cursor_period < .2 || cursor_period > 2)
                    throw std::runtime_error("city cursor period .2..2");
            } else if (s == "--exhaust-cutoff" && i + 1 < argc) {
                exhaust_cutoff = std::stod(argv[++i]);
                if (!std::isfinite(exhaust_cutoff) || exhaust_cutoff < .1 || exhaust_cutoff >= .5)
                    throw std::runtime_error("exhaust cutoff .1..<.5");
            } else if (s == "--victory-jump-seconds" && i + 1 < argc) {
                victory_jump_seconds = std::stod(argv[++i]);
                if (!std::isfinite(victory_jump_seconds) || victory_jump_seconds < .8 ||
                    victory_jump_seconds > 5)
                    throw std::runtime_error("victory jump .8..5");
            } else if (s == "--propaganda-auto-test")
                propaganda_auto_test = true;
            else if (s == "--console-speech-test")
                console_speech_test = true;
            else if (s == "--title-selection-test")
                title_selection_test = true;
            else if (s == "--playback-replay" && i + 1 < argc) {
                playback_replay = argv[++i];
                if (playback_replay != "click" && playback_replay != "hold" &&
                    playback_replay != "focus" && playback_replay != "complete")
                    throw std::runtime_error("playback replay click/hold/focus/complete");
            } else if (s == "--animation-hold-ms" && i + 1 < argc) {
                hold_ms = std::stoul(argv[++i]);
                if (hold_ms < 20 || hold_ms > 1000)
                    throw std::runtime_error("animation hold threshold20..1000ms");
            } else if (s == "--selection-door-seconds" && i + 1 < argc) {
                selection_door_seconds = std::stod(argv[++i]);
                if (!std::isfinite(selection_door_seconds) || selection_door_seconds < .1 ||
                    selection_door_seconds > 2)
                    throw std::runtime_error("selection door duration .1..2");
            } else if (s == "--bomber-speed" && i + 1 < argc) {
                bomber_speed = std::stod(argv[++i]);
                if (!std::isfinite(bomber_speed) || bomber_speed <= 0 || bomber_speed > 4)
                    throw std::runtime_error("bomber speed0..<4");
            } else if (s == "--seed" && i + 1 < argc)
                seed = std::uint32_t(std::stoul(argv[++i]));
            else if (s == "--stork-speed" && i + 1 < argc) {
                stork_speed = std::stod(argv[++i]);
                if (!std::isfinite(stork_speed) || stork_speed < .05 || stork_speed > 4)
                    throw std::runtime_error("stork speed.05..4");
            } else if (s == "--help") {
                std::cout
                    << "Usage: nuclearwar [--new-game-fade-seconds seconds] "
                       "[--animation-hold-ms milliseconds] [--audio-clock Hz] "
                       "[--selection-door-seconds seconds] [--bomber-speed multiplier] "
                       "[--stork-speed factor] [--city-cursor-period seconds] [--exhaust-cutoff "
                       "fraction] [--victory-jump-seconds seconds]\nDefault audio clock="
                    << std::uint64_t(DEFAULT_AUDIO_CLOCK)
                    << "; per-sample Hz=clock/period. Default stork speed=" << DEFAULT_STORK_SPEED
                    << "; selection shutter leg=.36s; new-game fade leg=.4s; city cursor "
                       "period=.6s; exhaust cutoff=.46; victory jump=1.2s\n";
                return false;
            } else if (s == "--typing-cps" && i + 1 < argc) {
                typing_cps = std::stod(argv[++i]);
                if (!std::isfinite(typing_cps) || typing_cps <= 0 || typing_cps > 1000)
                    throw std::runtime_error("invalid typing speed");
            } else if (s == "--preparation-seconds" && i + 1 < argc) {
                preparation_seconds = std::stod(argv[++i]);
                if (!std::isfinite(preparation_seconds) || preparation_seconds < .1 ||
                    preparation_seconds > 5)
                    throw std::runtime_error("preparation duration outside .1..5 seconds");
            } else if (s == "--finger-seconds" && i + 1 < argc) {
                finger_seconds = std::stod(argv[++i]);
                if (!std::isfinite(finger_seconds) || finger_seconds < .2 || finger_seconds > 5)
                    throw std::runtime_error("finger duration.2..5");
            } else if (s == "--explosion-size" && i + 1 < argc) {
                explosion_size = std::stoi(argv[++i]);
                if (explosion_size < 8 || explosion_size > 96 || explosion_size % 2)
                    throw std::runtime_error("explosion size must be even8..96");
            } else if (s == "--audio-clock" && i + 1 < argc) {
                audio_clock = std::stod(argv[++i]);
                if (!std::isfinite(audio_clock) || audio_clock <= 0 || audio_clock > 1e8)
                    throw std::runtime_error("invalid explicit audio clock");
            } else if (s == "--sample-rate" && i + 1 < argc) {
                std::string arg = argv[++i];
                auto equal = arg.find('=');
                if (equal == std::string::npos)
                    throw std::runtime_error("sample rate expects name=Hz");
                auto hz = std::stoul(arg.substr(equal + 1));
                if (hz < 100 || hz > 192000)
                    throw std::runtime_error("sample rate outside100..192000");
                rates[arg.substr(0, equal)] = unsigned(hz);
            } else if (s == "--mood-trace" && i + 1 < argc)
                mood_trace = argv[++i];
            else if (s == "--city-trace" && i + 1 < argc)
                city_trace = argv[++i];
            else if (s == "--session-fixture" && i + 1 < argc) {
                session_fixture = argv[++i];
                if (session_fixture != "ai-only" && session_fixture != "one-ai" &&
                    session_fixture != "human-death" && session_fixture != "human-victory" &&
                    session_fixture != "winner-ai" && session_fixture != "all-dead")
                    throw std::runtime_error("unknown session fixture");
            } else if (s == "--media-fixture" && i + 1 < argc) {
                fixture = argv[++i];
                if (fixture != "missile" && fixture != "interception" && fixture != "defector" &&
                    fixture != "propaganda" && fixture != "production" && fixture != "defense" &&
                    fixture != "bomber" && fixture != "bomber-attack" &&
                    fixture != "missile-preparation" && fixture != "speech-action" &&
                    fixture != "speech" && fixture != "portrait" && fixture != "100mt-production" &&
                    fixture != "100mt-missile" && fixture != "rapid-shutters" &&
                    fixture != "revenge-ai" && fixture != "revenge-human" &&
                    fixture != "revenge-bomber" && fixture != "round-shutters" &&
                    fixture != "round-shutters-eliminated" && fixture != "reverse-shutters" &&
                    fixture != "action-shutters" && fixture != "human-production" &&
                    fixture != "ai-production" && fixture != "defector-right" &&
                    fixture != "defector-left" && fixture != "bomber-interception" &&
                    fixture != "selector" && fixture != "doors" && fixture != "typewriter" &&
                    fixture != "transfer" && fixture != "failed-defense" && fixture != "finger" &&
                    fixture != "earthquake" && fixture != "stork" && fixture != "alien-laser" &&
                    fixture != "alien-delivery" && fixture != "alien-departure" &&
                    fixture != "catapult-cow" && fixture != "catapult-person" &&
                    fixture != "meltdown" && fixture != "weight" && fixture != "inspect-missile" &&
                    fixture != "inspect-bomber" && fixture != "inspect-defense" &&
                    fixture != "silo-selection" && fixture != "defense-selection" &&
                    fixture != "bomber-selection" && fixture != "mood-neutral" &&
                    fixture != "mood-happy" && fixture != "mood-angry" && fixture != "mood-mixed" &&
                    fixture != "mood-profile4" && fixture != "mood-speaking" &&
                    fixture != "mood-controls" && fixture != "mood-eliminated" &&
                    fixture != "target-switch")
                    throw std::runtime_error("unknown media fixture");
            } else if (s == "--replay-actor" && i + 1 < argc) {
                replay_actor = std::stoul(argv[++i]);
                if (replay_actor >= 5)
                    throw std::runtime_error("replay actor0..4");
            } else if (s == "--replay-target" && i + 1 < argc) {
                std::string v = argv[++i];
                auto sep = v.find(':');
                if (sep == std::string::npos)
                    throw std::runtime_error("replay target actor:city");
                auto a = std::stoul(v.substr(0, sep)), c = std::stoul(v.substr(sep + 1));
                if (a >= 5 || c >= 5)
                    throw std::runtime_error("replay target0..4:0..4");
                replay_target = Word(a);
                replay_city = Word(c);
            } else if (s == "--debug-hitboxes")
                debug_hitboxes = true;
            else if (s == "--smoke-prepare")
                smoke_prepare = true;
            else if (s == "--smoke-test" && i + 1 < argc)
                smoke = argv[++i];
            else
                throw std::runtime_error(
                    "usage: nuclearwar [--seed N] [--audio-clock Hz] [--sample-rate name=Hz] "
                    "[--smoke-test directory]");
        }
        return true;
    }
};
