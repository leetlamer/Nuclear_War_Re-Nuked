#include "platform/launch_options.hpp"
#include "rendering/renderer.hpp"
#ifdef NWR_WITH_REPLAY
#include "tests/support/session_fixtures.hpp"
#endif
int main(int argc, char **argv) {
    try {
        LaunchOptions options;
        if (!options.parse(argc, argv))
            return 0;
#ifndef NWR_WITH_REPLAY
        if (!options.smoke.empty() || !options.fixture.empty() || !options.session_fixture.empty())
            throw std::runtime_error("Use nuclearwar_replay for deterministic test fixtures");
#endif
        if (options.smoke_prepare && options.smoke.empty())
            throw std::runtime_error("smoke-prepare requires smoke-test output");
        std::optional<session::Data> action_selection_start;
        Player player(options.seed);

#ifdef NWR_WITH_REPLAY
#include "tests/support/replay_session_setup.inc"
#endif
        Renderer display;
        display.target_shutters.duration(options.selection_door_seconds);
        display.action_shutters.duration(options.selection_door_seconds);
        std::string score_path;
        if (!options.smoke.empty())
            score_path = options.smoke + "/HighScores";
        else {
            char *pref = SDL_GetPrefPath("NuclearWar", "NuclearWar");
            if (pref) {
                score_path = std::string(pref) + "HighScores";
                SDL_free(pref);
            }
        }
        EndgamePresentation ending(score_path);
        ending.jump_seconds(options.victory_jump_seconds);
        VictorySoundSequence victory_sound;
        bool extinction_sound_played = false;
        display.endgame = &ending;
        SDL_StartTextInput();
        SoundManager sound(options.audio_clock, options.rates);
        std::cerr << "Audio clock=" << std::uint64_t(sound.effective_clock()) << "\n"
                  << sound.status << "\n";
        ActionPresentation scene;
        scene.transitions(true);
        display.cursor_period = options.cursor_period;
        display.exhaust_cutoff = options.exhaust_cutoff;
        display.stork_speed = options.stork_speed;
        display.debug_hitboxes = options.debug_hitboxes;
        display.explosion_size = options.explosion_size;
        display.preparation_seconds = options.preparation_seconds;
        display.finger_seconds = options.finger_seconds;
        scene.typing_speed(options.typing_cps);
        scene.bomber_speed(options.bomber_speed);
        bool running = true, show_log = false;
        unsigned stage = 0, completed = 0;
        std::size_t captured = 99;
        unsigned presented_frames = 0, capture_step = 0;
        std::uint64_t last = 0, start = SDL_GetTicks64(), last_frame = start, main_input_after = 0;
        if (!options.smoke.empty())
            std::filesystem::create_directories(options.smoke);
        if (options.city_trace.empty() && !options.smoke.empty())
            options.city_trace = options.smoke + "/city_trace.jsonl";
        if (options.mood_trace.empty() && !options.smoke.empty())
            options.mood_trace = options.smoke + "/mood_trace.jsonl";
        std::unique_ptr<MoodAudit> mood_audit;
        if (!options.mood_trace.empty())
            mood_audit = std::make_unique<MoodAudit>(options.mood_trace);
        display.mood_audit = mood_audit.get();
        std::unique_ptr<CityAudit> audit;
        if (!options.city_trace.empty())
            audit = std::make_unique<CityAudit>(options.city_trace);

#ifdef NWR_WITH_REPLAY
#include "tests/support/replay_scene_setup.inc"
#endif
        TurnPlayback playback;
        playback.threshold(options.hold_ms);
        NewGameTransition new_game;
        new_game.duration(options.new_game_fade_seconds);
        if (options.smoke.empty())
            new_game.initial_fade_in();
        unsigned restart_round = 0, restart_pick = 0;
        bool restart_fresh_check = false, restart_ending_click = false, restart_early_click = false,
             restart_name_click = false;
        std::vector<unsigned char> restart_scores;
        std::optional<std::pair<int, int>> queued_selection;
        bool auto_selection = false;
        std::uint64_t bleep_seen = player.bleep_serial();
        bool had_action_audio = false, had_target_audio = false, had_selection_audio = false;
        Screen shutter_audio_screen = Screen::title;
        auto player_click = [&](int x, int y) {
            if (player.screen() != Screen::main) {
                display.target_shutters.observe(4, false);
                if (had_target_audio) {
                    sound.stop();
                    had_target_audio = false;
                }
            } else if (display.target_shutters.active() || display.action_shutters.active()) {
                bool selection = false, emoji = false;
                for (auto g : player_assets::gadgets)
                    if (Rect{g.x, g.y, g.w, g.h}.contains(x, y)) {
                        if (g.id >= 97 && g.id <= 100)
                            emoji = true;
                        if (g.id == 109 || g.id == 119 || g.id == 122 || g.id == 108)
                            selection = true;
                    }
                const Rect portraits[]{
                    {0, 0, 68, 42}, {0, 149, 68, 42}, {251, 0, 69, 42}, {251, 149, 69, 42}};
                for (auto r : portraits)
                    selection = selection || r.contains(x, y);
                if (selection && !emoji)
                    queued_selection = std::make_pair(x, y);
                return;
            }
            player.click(x, y);
        };
        while (running) {
            if (!auto_selection) {
                if (player.screen() == Screen::title)
                    player.enter_selection();
                auto_selection = true;
            }
            SDL_Event event;
            while (SDL_PollEvent(&event)) {
                if (event.type == SDL_QUIT)
                    running = false;
                if (new_game.active()) {
                    if (event.type == SDL_WINDOWEVENT &&
                        event.window.event == SDL_WINDOWEVENT_FOCUS_LOST)
                        playback.cancel();
                    continue;
                }
                auto playback_key = PlaybackKey{scene.serial(), scene.index()};
                bool eligible =
                    !show_log && player.screen() == Screen::main && scene.input_eligible();
                if (event.type == SDL_WINDOWEVENT &&
                    event.window.event == SDL_WINDOWEVENT_FOCUS_LOST) {
                    playback.cancel();
                    queued_selection.reset();
                    continue;
                }
                if (event.type == SDL_MOUSEBUTTONUP && event.button.button == SDL_BUTTON_LEFT) {
                    auto released = playback.release(SDL_GetTicks64(), playback_key, eligible);
                    if (released == PlaybackRelease::instant_complete)
                        scene.instant_complete(SDL_GetTicks64());
                    if (released != PlaybackRelease::ignored)
                        continue;
                }
                if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT &&
                    playback.press(SDL_GetTicks64(), playback_key, eligible))
                    continue;

                if (ending.active()) {
                    if (event.type == SDL_MOUSEBUTTONDOWN &&
                        event.button.button == SDL_BUTTON_LEFT) {
                        if (new_game.request(ending.restart_ready())) {
                            playback.cancel();
                            sound.stop();
                        }
                        continue;
                    }
                    if (event.type == SDL_TEXTINPUT)
                        ending.input(event.text.text);
                    if (event.type == SDL_KEYDOWN && !event.key.repeat) {
                        if (!ending.editing() && event.key.keysym.sym == SDLK_m)
                            sound.mute();
                        else if (!ending.editing() && (event.key.keysym.sym == SDLK_EQUALS ||
                                                       event.key.keysym.sym == SDLK_PLUS))
                            sound.volume(8);
                        else if (!ending.editing() && event.key.keysym.sym == SDLK_MINUS)
                            sound.volume(-8);
                        else if (event.key.keysym.sym == SDLK_ESCAPE && ending.ai_victory()) {
                            sound.stop();
                            running = false;
                        } else if (event.key.keysym.sym == SDLK_BACKSPACE)
                            ending.backspace();
                        else if (event.key.keysym.sym == SDLK_RETURN ||
                                 event.key.keysym.sym == SDLK_ESCAPE) {
                            if (ending.editing())
                                ending.finish_name();
                            else if (ending.hall())
                                running = false;
                        }
                    }
                    continue;
                }
                if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
                    if (show_log) {
                    } else if (scene.accepts_action_click() &&
                               player.coordinator()->phase() == session::Phase::human_selection) {
                        bool covered =
                            display.target_shutters.active() || display.action_shutters.active();
                        player_click(event.button.x, event.button.y);
                        if (!covered && player.accepted_action())
                            scene.dismiss_for_action();
                    } else if (scene.active()) {
                        if (scene.click(SDL_GetTicks64()))
                            main_input_after = SDL_GetTicks64() + 180;
                    } else if (SDL_GetTicks64() >= main_input_after)
                        player_click(event.button.x, event.button.y);
                } // SDL logical integer-scaled coordinates
                if (event.type == SDL_KEYDOWN && !event.key.repeat) {
                    auto key = event.key.keysym.sym;
                    if (key == SDLK_TAB)
                        show_log = !show_log;
                    else if (key == SDLK_m)
                        sound.mute();
                    else if (key == SDLK_EQUALS || key == SDLK_PLUS)
                        sound.volume(8);
                    else if (key == SDLK_MINUS)
                        sound.volume(-8);
                    else if (key == SDLK_ESCAPE) {
                        if (scene.active()) {
                            if (scene.skip())
                                main_input_after = SDL_GetTicks64() + 180;
                        } else
                            running = false;
                    } else if (key == SDLK_f && scene.active() && !scene.transition_active())
                        scene.tick(4);
                    else if (show_log) {
                        if (key == SDLK_PAGEUP)
                            display.log_scroll += 10;
                        if (key == SDLK_PAGEDOWN)
                            display.log_scroll =
                                display.log_scroll > 10 ? display.log_scroll - 10 : 0;
                    } else if (scene.active()) {
                        if (key == SDLK_SPACE || key == SDLK_RETURN) {
                            if (scene.click(SDL_GetTicks64()))
                                main_input_after = SDL_GetTicks64() + 180;
                        }
                    } else if (SDL_GetTicks64() < main_input_after ||
                               (player.screen() == Screen::main &&
                                (display.target_shutters.active() ||
                                 display.action_shutters.active()))) {
                    } else if (player.screen() == Screen::title)
                        player.enter_selection();
                    else if (player.screen() == Screen::selection && key == SDLK_RETURN)
                        player_click(110, 20);
                    else if (player.screen() == Screen::main) {
                        if (key == SDLK_h)
                            player.command({HumanCommandKind::actor_view, 4});
                        else if (key == SDLK_q || key == SDLK_RETURN)
                            player.confirm_banner();
                        else if (key == SDLK_s)
                            player.command({HumanCommandKind::production, 0});
                        else if (key == SDLK_p)
                            player.command({HumanCommandKind::propaganda, 0});
                    }
                }
            }
            if (queued_selection) {
                bool allowed =
                    !new_game.active() && player.coordinator() &&
                    target_shutter_owner(player.screen(), player.human_alive(), player.ended(),
                                         player.coordinator()->phase(), scene.selection_blocked());
                if (!allowed)
                    queued_selection.reset();
                else if (!display.target_shutters.active() && !display.action_shutters.active()) {
                    auto click = *queued_selection;
                    queued_selection.reset();
                    player.click(click.first, click.second);
                    if (player.accepted_action())
                        scene.dismiss_for_action();
                    if (!options.smoke.empty()) {
                        std::ofstream log(options.smoke + "/coalesced_selection.txt",
                                          std::ios::app);
                        log << click.first << "," << click.second << "\n";
                    }
                }
            }
            if (!running) {
                playback.cancel();
                sound.stop();
                break;
            }
            auto micros = std::chrono::duration_cast<std::chrono::microseconds>(
                              std::chrono::system_clock::now().time_since_epoch())
                              .count() %
                          1000000;
            if (!new_game.active() && !scene.active()) {
                if (!options.session_fixture.empty() &&
                    (!options.restart_test || !restart_fresh_check) &&
                    player.coordinator()->phase() == session::Phase::human_selection) {
                    if (!player.human_alive())
                        throw std::runtime_error("dead human input requested");
                    player.command({HumanCommandKind::production, 0});
                    player.confirm_banner();
                }
                player.advance_turn(!options.session_fixture.empty() ? Word(player.round() * 37 + 1)
                                    : options.smoke.empty()          ? Word(micros & 65535)
                                                                     : Word(123));
            }
            if (options.fixture.empty() || options.fixture == "round-shutters" ||
                options.fixture == "round-shutters-eliminated")
                scene.observe(player);
            scene.constrain_door_owner(player.screen() == Screen::main && player.human_alive() &&
                                       player.coordinator()->phase() ==
                                           session::Phase::human_selection);
            scene.typing_speed(options.typing_cps);
            scene.bomber_speed(options.bomber_speed);
            scene.stork_speed(options.stork_speed);
            auto frame_time = SDL_GetTicks64();
            double fade_dt = options.smoke.empty() ? (frame_time - last_frame) / 1000. : .10;
            new_game.tick(fade_dt);
            if (new_game.take_reset()) {
                playback = TurnPlayback{};
                playback.threshold(options.hold_ms);
                sound.new_session();
                queued_selection.reset();
                SDL_StopTextInput();
                player = Player(options.seed);
                player.enter_selection();
                scene = ActionPresentation{};
                scene.transitions(true);
                display.reset_session(options.selection_door_seconds);
                if (audit)
                    audit->new_session();
                if (mood_audit)
                    mood_audit->new_session();
                ending = EndgamePresentation(score_path);
                ending.jump_seconds(options.victory_jump_seconds);
                victory_sound = VictorySoundSequence{};
                extinction_sound_played = false;
                had_action_audio = had_target_audio = had_selection_audio = false;
                bleep_seen = player.bleep_serial();
                show_log = false;
                main_input_after = frame_time + 1;
                SDL_FlushEvent(SDL_MOUSEBUTTONDOWN);
                SDL_FlushEvent(SDL_MOUSEBUTTONUP);
                if (options.restart_test) {
                    ++restart_round;
                    restart_pick = 0;
                    restart_fresh_check = true;
                    restart_ending_click = restart_early_click = restart_name_click = false;
                }
                new_game.reset_done();
            }
            if (!show_log) {
                double dt = options.smoke.empty() ? (frame_time - last_frame) / 1000.0 : 0.10;
                bool eligible = player.screen() == Screen::main && scene.input_eligible();
                double accelerated = playback.seconds(dt, last_frame, frame_time,
                                                      {scene.serial(), scene.index()}, eligible);
                scene.playback_tick(accelerated, accelerated > dt);
                if (!options.playback_replay.empty()) {
                    std::ofstream trace(options.smoke + "/playback_frames.jsonl", std::ios::app);
                    trace << "{\"wall\":" << frame_time
                          << ",\"gain\":" << (dt > 0 ? accelerated / dt : 1.)
                          << ",\"elapsed\":" << scene.elapsed() << ",\"index\":" << scene.index()
                          << ",\"instant\":" << scene.instant_result()
                          << ",\"draw\":" << (scene.active() ? scene.draw_elapsed() : 0.)
                          << ",\"active\":" << scene.active() << "}\n";
                }
                display.visual_seconds += dt;
                if (player.coordinator()) {
                    bool can_select = target_shutter_owner(
                        player.screen(), player.human_alive(), player.ended(),
                        player.coordinator()->phase(), scene.selection_blocked());
                    display.observe_selection(player, can_select);
                    display.action_shutters.tick(dt);
                    display.target_shutters.observe(
                        player.coordinator()->data().human.logical_view,
                        target_shutter_owner(player.screen(), player.human_alive(), player.ended(),
                                             player.coordinator()->phase(),
                                             scene.selection_blocked()) &&
                            player.coordinator()->data().human.display_actor != 0xffff &&
                            !display.action_shutters.active());
                    display.target_shutters.tick(dt);
                } else {
                    display.target_shutters.observe(4, false);
                    display.cancel_selection();
                }
                ending.tick(dt);
            } else
                playback.cancel();
            last_frame = frame_time;
            if (!scene.active() && ending.begin(player, false)) {
                SDL_StartTextInput();
            }
            if (scene.take_instant_audio_cleanup())
                sound.stop();

            bool shutter_owner_allowed =
                player.coordinator() &&
                target_shutter_owner(player.screen(), player.human_alive(), player.ended(),
                                     player.coordinator()->phase(), scene.selection_blocked()) &&
                player.coordinator()->data().human.display_actor != 0xffff &&
                !display.action_shutters.active();
            bool selection_owner_allowed =
                player.coordinator() &&
                target_shutter_owner(player.screen(), player.human_alive(), player.ended(),
                                     player.coordinator()->phase(), scene.selection_blocked());
            // Display::observe_selection owns cancellation in its chrome-generation domain.
            // Reapplying a model action serial here replays completed payload/map cycles.
            if (had_selection_audio && !display.action_shutters.active()) {
                sound.stop();
                had_selection_audio = false;
            }
            if (!shutter_owner_allowed)
                display.target_shutters.observe(
                    player.coordinator() ? player.coordinator()->data().human.logical_view
                                         : Word(4),
                    false);
            if (had_target_audio &&
                (!shutter_owner_allowed || shutter_audio_screen != player.screen())) {
                sound.stop();
                had_target_audio = false;
            }
            if (shutter_owner_allowed && display.target_shutters.active()) {
                had_target_audio = true;
                shutter_audio_screen = player.screen();
            }
            if (player.bleep_serial() != bleep_seen) {
                bleep_seen = player.bleep_serial();
                sound.bleep(player.bleep_period());
                if (!options.smoke.empty()) {
                    std::ofstream trace(options.smoke + "/bleep_trace.txt", std::ios::app);
                    trace << "serial=" << bleep_seen << " period=" << player.bleep_period() << "\n";
                }
            }
            if (selection_owner_allowed && display.action_shutters.active()) {
                had_selection_audio = true;
                if (display.action_shutters.phase() != 2 &&
                    sound.play(display.action_shutters.audio_token(), door_sound()) &&
                    !options.smoke.empty()) {
                    std::ofstream out(options.smoke + "/action_door_audio.txt", std::ios::app);
                    out << "token=" << display.action_shutters.audio_token()
                        << " phase=" << display.action_shutters.phase()
                        << " action=" << display.action_shutters.visible()
                        << " sample=door.snd period=415 bytes=8820 volume=12\n";
                }
            } else if (shutter_owner_allowed && display.target_shutters.active()) {
                if (display.target_shutters.phase() != 2 &&
                    sound.play(display.target_shutters.audio_token(), door_sound()) &&
                    !options.smoke.empty()) {
                    std::ofstream out(options.smoke + "/target_door_audio.txt", std::ios::app);
                    out << "token=" << display.target_shutters.audio_token()
                        << " phase=" << display.target_shutters.phase()
                        << " sample=door.snd period=415 bytes=8820 volume=12\n";
                }
            } else if (scene.active() && scene.transition_active()) {
                if (sound.play(0x8000000000000000ull | (scene.serial() << 8) |
                                   unsigned(scene.door_phase()),
                               door_sound()) &&
                    !options.smoke.empty()) {
                    std::ofstream out(options.smoke + "/door_audio.txt", std::ios::app);
                    out << "round=" << scene.serial() << " phase=" << unsigned(scene.door_phase())
                        << " sample=door.snd period=415 bytes=8820 volume=12\n";
                }
            } else if (scene.active() && scene.event().region == EventRegion::action) {
                bool loop = defence_animation_active(scene.event().media, scene.visual_elapsed());
                bool replenished = sound.defence_loop((scene.serial() << 32) | scene.index(), loop);
                if (replenished && !options.smoke.empty()) {
                    std::ofstream log(options.smoke + "/defence_loop.txt", std::ios::app);
                    log << "round=" << scene.serial() << " event=" << scene.index()
                        << " elapsed=" << scene.elapsed() << " queued=" << sound.queued_bytes()
                        << "\n";
                }
                bool played =
                    !loop && scene.event().media.action != 7 &&
                    action_sound_ready(scene.event().media, scene.elapsed()) &&
                    sound.event((scene.serial() << 32) | (scene.index() << 2), scene.event().media);
                if (played && !options.smoke.empty()) {
                    auto cue = sound_cue(scene.event().media);
                    std::ofstream log(options.smoke + "/audio_trace.txt", std::ios::app);
                    log << "round=" << scene.serial() << " event=" << scene.index()
                        << " sample=" << cue->sample << " period=" << cue->period
                        << " bytes=" << cue->length << " repeats=" << cue->repeats
                        << " queued_plays=" << sound.last_plays << " clock=" << options.audio_clock
                        << "\n";
                }
                if (scene.visual_elapsed() >= 2.8 && scene.visual_elapsed() < 4.0 &&
                    impact_sound(scene.event().media)) {
                    if (sound.play((scene.serial() << 32) | (scene.index() << 2) | 1,
                                   impact_sound(scene.event().media)) &&
                        !options.smoke.empty()) {
                        std::ofstream log(options.smoke + "/impact_audio.txt", std::ios::app);
                        log << "scene=" << scene.serial() << " event=" << scene.index()
                            << " explosion period=650\n";
                    }
                }
            } else {
                sound.end_defence();
                if (had_action_audio)
                    sound.stop();
            }
            had_action_audio = scene.active() && (scene.transition_active() ||
                                                  scene.event().region == EventRegion::action);
            if (scene.take_propaganda_result_audio()) {
                unsigned before = sound.queued_bytes();
                bool started = false;
                if (!before)
                    started = sound.play((scene.serial() << 32) | (scene.index() << 2) | 2,
                                         sound_cue(scene.event().media));
                if (!options.smoke.empty()) {
                    std::ofstream trace(options.smoke + "/propaganda_result_audio.txt",
                                        std::ios::app);
                    trace << "before=" << before << " after=" << sound.queued_bytes()
                          << " restarted=" << started << " cleanup=0\n";
                }
            }
            if (ending.active() && ending.winner() < 0 &&
                extinction_stage(ending.elapsed()) >= 3 && !extinction_sound_played) {
                extinction_sound_played = true;
                sound.play(0xe000000000000000ull, SoundCue{"exp", 620, 4998, 64});
            }
            if (ending.ai_victory()) {
                unsigned previous_queue = sound.queued_bytes();
                if (auto request = victory_sound.next(ending.elapsed(), ending.jump_seconds(),
                                                      previous_queue != 0)) {
                    bool queued = sound.play(0xd000000000000000ull | *request,
                                             SoundCue{"iwon", 370, 5913, 32});
                    if (!options.smoke.empty()) {
                        std::ofstream log(options.smoke + "/victory_audio.txt", std::ios::app);
                        log << "request=" << *request << " queued=" << queued
                            << " previous_queue=" << previous_queue
                            << " clock=" << options.audio_clock << " elapsed=" << ending.elapsed()
                            << "\n";
                    }
                }
                ending.audio_wait(!victory_sound.complete(sound.queued_bytes() != 0));
            }

            if (!options.restart_test && !options.smoke.empty() && ending.active() &&
                ending.hall() && ending.editing()) {
                ending.input("Tester");
                ending.finish_name();
            }
            if (audit)
                audit->observe(player);
            if (mood_audit)
                mood_audit->observe(player);
            std::optional<State> render_before;
            if (audit && player.coordinator())
                render_before = player.coordinator()->data().state;
            auto render_seed = player.coordinator() ? player.coordinator()->data().rng.seed() : 0;
            display.render(player, show_log, &scene);
            if (audit && player.coordinator()) {
                if (!same_state(*render_before, player.coordinator()->data().state) ||
                    render_seed != player.coordinator()->data().rng.seed())
                    throw std::runtime_error("render mutated gameplay state/RNG");
                const State &shown =
                    scene.show_action_map()
                        ? scene.visual_state()
                        : scene.interface_state(player.coordinator()->data().state);
                audit->render(player, scene, shown);
            }
            sound.pump_production();
            bool console_exposed =
                !show_log && player.screen() == Screen::main &&
                ((scene.active() && scene.event().system_notice) ||
                 (player.human_alive() && !player.ended() &&
                  (!scene.active() || scene.main_text_visible() || scene.human_console()))) &&
                !scene.transition_active();
            if (sound.ticker(display.console_visible, console_exposed && display.console_typing,
                             SDL_GetTicks64()) &&
                !options.smoke.empty()) {
                std::ofstream trace(options.smoke + "/ticker_trace.txt", std::ios::app);
                trace << "speech=" << scene.speech_visible() << " typing=" << display.console_typing
                      << " plays=" << sound.ticker_plays << " clock=" << options.audio_clock
                      << "\n";
            }

#ifdef NWR_WITH_REPLAY
#include "tests/support/replay_console_capture.inc"
#endif
            {
                std::string title = SDL_GetWindowTitle(display.window);
                title +=
                    sound.available()
                        ? (sound.muted()
                               ? " - Muted"
                               : " - Volume " + std::to_string(sound.volume() * 100 / 128) + "%")
                        : " - Audio off (rates/device)";
                SDL_SetWindowTitle(display.window, title.c_str());
            }

#ifdef NWR_WITH_REPLAY
#include "tests/support/replay_capture.inc"
#endif
            if (new_game.active()) {
                SDL_SetRenderDrawBlendMode(display.renderer, SDL_BLENDMODE_BLEND);
                SDL_SetRenderDrawColor(display.renderer, 0, 0, 0,
                                       Uint8(std::lround(255 * new_game.alpha())));
                SDL_Rect cover{0, 0, 320, 200};
                SDL_RenderFillRect(display.renderer, &cover);
                SDL_SetRenderDrawBlendMode(display.renderer, SDL_BLENDMODE_NONE);
            }
            SDL_RenderPresent(display.renderer);
            if (!show_log) {
                scene.presented();
                if (scene.production_100mt_ready() &&
                    sound.production(0x9000000000000000ull | scene.console_source(),
                                     production_100mt_sound()) &&
                    !options.smoke.empty()) {
                    std::ofstream log(options.smoke + "/production_100mt_audio.txt", std::ios::app);
                    log << "source=" << scene.console_source()
                        << " sample=ooo period=715 bytes=8820 volume=24 typed_complete=1 queued="
                        << sound.queued_bytes() << "\n";
                }
                display.target_shutters.presented();
                display.action_shutters.presented();
            }
            if (scene.active())
                ++presented_frames;

#ifdef NWR_WITH_REPLAY
#include "tests/support/replay_step.inc"
#endif
            if (!show_log)
                scene.finish_production();
            SDL_Delay(5);
            if (!options.smoke.empty() && SDL_GetTicks64() - start > 90000)
                throw std::runtime_error("smoke timeout");
        }
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
