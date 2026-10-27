// Included only by the replay executable, inside the application frame scope.
if (options.restart_test) {
    if (new_game.active() && ending.editing())
        throw std::runtime_error("restart interrupted name entry");
    auto mouse = [&](int x, int y) {
        SDL_Event input{};
        input.type = SDL_MOUSEBUTTONDOWN;
        input.button.button = SDL_BUTTON_LEFT;
        input.button.x = x;
        input.button.y = y;
        SDL_PushEvent(&input);
        input.type = SDL_MOUSEBUTTONUP;
        SDL_PushEvent(&input);
    };
    if (ending.active() && !ending.hall() && !restart_early_click) {
        mouse(110, 20);
        restart_early_click = true;
    }
    if (ending.active() && ending.hall() && !new_game.active()) {
        if (ending.editing()) {
            if (!restart_name_click) {
                mouse(110, 20);
                restart_name_click = true;
            } else {
                if (new_game.active())
                    throw std::runtime_error("restart interrupted score entry");
                SDL_Event input{};
                input.type = SDL_TEXTINPUT;
                std::snprintf(input.text.text, sizeof(input.text.text), "Tester");
                SDL_PushEvent(&input);
                input = {};
                input.type = SDL_KEYDOWN;
                input.key.keysym.sym = SDLK_RETURN;
                SDL_PushEvent(&input);
            }
        } else if (ending.restart_ready() && !restart_ending_click) {
            if (std::filesystem::exists(score_path))
                restart_scores = bytes(score_path);
            mouse(110, 20);
            restart_ending_click = true;
            display.capture(options.smoke + "/restart_hall_" + std::to_string(restart_round) +
                            ".bmp");
        }
    }
    if (restart_fresh_check && !new_game.active() && player.screen() == Screen::selection) {
        if (restart_pick == 0) {
            Player expected(options.seed);
            expected.enter_selection();
            if (player.coordinator() || player.selection().flags != expected.selection().flags ||
                player.selection().rng.seed() != expected.selection().rng.seed() ||
                scene.active() || display.action_shutters.active() ||
                display.target_shutters.active() || !display.console_history().history().empty() ||
                playback.captured() || sound.queued_bytes() || ending.active())
                throw std::runtime_error("stale new-game state");
            if (!restart_scores.empty() && bytes(score_path) != restart_scores)
                throw std::runtime_error("restart lost persisted scores");
            display.capture(options.smoke + "/restart_selection_" + std::to_string(restart_round) +
                            ".bmp");
        }
        if (restart_pick < 4) {
            const Word ids[]{0, 4, 2, 7};
            auto r = portrait_rect(ids[restart_pick]);
            mouse(r.x + 20, r.y + 15);
        } else
            mouse(110, 20);
        ++restart_pick;
    }
    if (restart_fresh_check && player.screen() == Screen::main && !new_game.active()) {
        Player expected(options.seed);
        expected.enter_selection();
        for (Word id : {Word(0), Word(4), Word(2), Word(7)}) {
            auto r = portrait_rect(id);
            expected.click(r.x + 20, r.y + 15);
        }
        expected.click(110, 20);
        if (!same_state(expected.coordinator()->data().state, player.coordinator()->data().state) ||
            expected.coordinator()->data().rng.seed() != player.coordinator()->data().rng.seed())
            throw std::runtime_error("new game differs from fresh initialization");
        std::ofstream log(options.smoke + "/restart_trace.txt", std::ios::app);
        log << "restart=" << restart_round
            << " fresh_state_rng_match=1 scores_preserved=1 console_reset=1 "
               "shutters_reset=1\n";
        if (restart_round >= 2) {
            display.capture(options.smoke + "/result.bmp");
            std::cout << "PASS two high-score restarts;three sessions initialized;reference "
                         "state/RNG;persisted scores;new-game-only fades\n";
            running = false;
        } else {
            auto initial = endgame_fixture(true, 0);
            initial.state.actors[4].population[0] = 500;
            player = Player::developer_session(initial);
            scene = ActionPresentation{};
            scene.transitions(true);
            display.reset_session(options.selection_door_seconds);
            restart_fresh_check = false;
        }
    }
    if (new_game.active()) {
        display.capture(options.smoke + "/fade_" + std::to_string(restart_round) + "_" +
                        std::to_string(int(new_game.phase())) + "_" +
                        std::to_string(int(new_game.alpha() * 100)) + ".bmp");
        std::ofstream log(options.smoke + "/fade_trace.txt", std::ios::app);
        log << "restart=" << restart_round << " phase=" << int(new_game.phase())
            << " alpha=" << new_game.alpha() << " screen=" << int(player.screen()) << "\n";
    }
}
if (options.console_speech_test && scene.speech_visible() && !display.console_typing &&
    !display.console_history().history().empty()) {
    if (sound.ticker_plays == 0 || sound.queued_bytes() != 0 ||
        !same_state(console_speech_before->state, player.coordinator()->data().state) ||
        console_speech_before->rng.seed() != player.coordinator()->data().rng.seed())
        throw std::runtime_error("speech suppressed typing or corrupted audio/state");
    display.capture(options.smoke + "/result.bmp");
    std::cout << "PASS actual SDL console typed to completion under AI speech; ticker_plays="
              << sound.ticker_plays << " idle_queue0;state RNG unchanged\n";
    running = false;
}
if (options.propaganda_auto_test && options.fixture == "propaganda" && !scene.active()) {
    display.capture(options.smoke + "/result.bmp");
    std::cout << "PASS normal propaganda result auto-completes with no input\n";
    running = false;
}
if (options.title_selection_test) {
    static int step = 0;
    auto push_portrait = [&](Word id) {
        auto r = portrait_rect(id);
        SDL_Event input{};
        input.type = SDL_MOUSEBUTTONDOWN;
        input.button.button = SDL_BUTTON_LEFT;
        input.button.x = r.x + 20;
        input.button.y = r.y + 15;
        SDL_PushEvent(&input);
    };
    if (step < 3 && player.screen() == Screen::selection) {
        if (display.target_shutters.active())
            throw std::runtime_error("title owns target shutters");
        display.capture(options.smoke + "/title_selection_" + std::to_string(step) + ".bmp");
        if (step == 0)
            push_portrait(0);
        else if (step == 1) {
            if (player.selection().flags[0] != 1)
                throw std::runtime_error("title highlight delayed");
            push_portrait(0);
        } else {
            if (player.selection().flags[0] != 0)
                throw std::runtime_error("title deselection delayed");
            for (Word id : {Word(0), Word(4), Word(2), Word(7)})
                push_portrait(id);
            SDL_Event input{};
            input.type = SDL_MOUSEBUTTONDOWN;
            input.button.button = SDL_BUTTON_LEFT;
            input.button.x = 110;
            input.button.y = 20;
            if (options.seed == 42) {
                input = {};
                input.type = SDL_KEYDOWN;
                input.key.keysym.sym = SDLK_RETURN;
            }
            SDL_PushEvent(&input);
        }
        ++step;
    }
    if (step == 3 && player.screen() == Screen::main) {
        Player expected(options.seed);
        expected.enter_selection();
        for (Word id : {Word(0), Word(0), Word(0), Word(4), Word(2), Word(7)}) {
            auto r = portrait_rect(id);
            if (!expected.click(r.x + 20, r.y + 15))
                throw std::runtime_error("reference title input failed");
        }
        if (!expected.click(110, 20) ||
            !same_state(expected.coordinator()->data().state, player.coordinator()->data().state) ||
            expected.coordinator()->data().rng.seed() != player.coordinator()->data().rng.seed() ||
            player.bleep_serial() != 6 || display.target_shutters.active())
            throw std::runtime_error("title selection changed state RNG or input count");
        display.capture(options.smoke + "/title_startup_main.bmp");
        std::cout << "PASS actual SDL title select/deselect/rapidselection/confirm;6accepted "
                     "toggles;reference state RNG identical;no title shutter owner\n";
        running = false;
    }
}
if (!options.playback_replay.empty() && options.fixture.rfind("revenge-", 0) != 0) {
    static int step = 0;
    static bool saw_fast = false, saw_normal = false, saw_result = false;
    static unsigned result_frames = 0;
    static std::uint64_t result_wall = 0;
    static bool dismissed = false;
    auto now = SDL_GetTicks64();
    if (step == 0 && scene.input_eligible() && scene.elapsed() >= options.playback_click_at) {
        SDL_Event input{};
        input.type = SDL_MOUSEBUTTONDOWN;
        input.button.button = SDL_BUTTON_LEFT;
        input.button.x = 85;
        input.button.y = 15;
        SDL_PushEvent(&input);
        if (options.playback_replay == "click" || options.playback_replay == "complete") {
            input.type = SDL_MOUSEBUTTONUP;
            SDL_PushEvent(&input);
        }
        step = 1;
    }
    if (options.playback_replay != "click" && options.playback_replay != "complete" && step == 1 &&
        playback.mode(now) == PlaybackMode::fast_forward) {
        saw_fast = true;
        if (scene.elapsed() >= .8) {
            SDL_Event input{};
            if (options.playback_replay == "focus") {
                input.type = SDL_WINDOWEVENT;
                input.window.event = SDL_WINDOWEVENT_FOCUS_LOST;
                SDL_PushEvent(&input);
            }
            input = {};
            input.type = SDL_MOUSEBUTTONUP;
            input.button.button = SDL_BUTTON_LEFT;
            SDL_PushEvent(&input);
            step = 2;
        }
    }
    if (step == 2 && !playback.captured() && playback.mode(now) == PlaybackMode::normal)
        saw_normal = true;
    if (scene.active() && scene.instant_result())
        ++result_frames;
    if (scene.instant_result() && !saw_result) {
        result_wall = now;
        display.capture(options.smoke + "/instant_result.bmp");
        std::ofstream trace(options.smoke + "/instant_reveal.txt");
        trace << "elapsed=" << scene.elapsed() << " visual=" << scene.visual_elapsed()
              << " index=" << scene.index() << " active=" << scene.active() << "\n";
        saw_result = true;
    }
    if (scene.active() && scene.instant_result() && scene.event().action == 3 &&
        scene.elapsed() >= 2.3) {
        static bool captured_motion = false;
        if (!captured_motion) {
            display.capture(options.smoke + "/result_motion.bmp");
            captured_motion = true;
        }
    }
    if (options.playback_replay == "complete" && options.fixture == "propaganda" && saw_result &&
        !dismissed && now >= result_wall + options.propaganda_dismiss_ms) {
        SDL_Event input{};
        input.type = SDL_MOUSEBUTTONDOWN;
        input.button.button = SDL_BUTTON_LEFT;
        SDL_PushEvent(&input);
        input.type = SDL_MOUSEBUTTONUP;
        SDL_PushEvent(&input);
        dismissed = true;
        std::ofstream trace(options.smoke + "/propaganda_dismiss.txt");
        trace << "fresh_click_ms=" << now - result_wall
              << " requested=" << options.propaganda_dismiss_ms << "\n";
    }
    if (options.playback_replay == "complete" && step && !scene.active()) {
        if (!saw_result ||
            result_frames <
                ((options.fixture == "propaganda" || options.fixture == "production" ||
                  options.fixture == "defense" || options.fixture == "missile-preparation" ||
                  options.fixture == "bomber")
                     ? 1u
                     : 18u) ||
            !same_state(playback_before->state, player.coordinator()->data().state) ||
            playback_before->rng.seed() != player.coordinator()->data().rng.seed() ||
            playback_before->human.selection.pending_action !=
                player.coordinator()->data().human.selection.pending_action)
            throw std::runtime_error("single-click completion failed");
        display.capture(options.smoke + "/result.bmp");
        std::cout << "PASS actual SDL single-click automatic completion fixture=" << options.fixture
                  << " result_frames=" << result_frames
                  << " no second click;live state RNG inventory unchanged\n";
        running = false;
    }
    if (options.playback_replay != "complete" && step && scene.active() &&
        scene.animation_complete()) {
        if (scene.index() != 0 ||
            !same_state(playback_before->state, player.coordinator()->data().state) ||
            playback_before->rng.seed() != player.coordinator()->data().rng.seed() ||
            playback_before->human.selection.pending_action !=
                player.coordinator()->data().human.selection.pending_action)
            throw std::runtime_error("playback replay mutated simulation or advanced action");
        if (options.playback_replay == "click" && !saw_result)
            throw std::runtime_error("missing instant result");
        if (options.playback_replay != "click" && (!saw_fast || !saw_normal || saw_result))
            throw std::runtime_error("hold/focus gesture failed");
        display.capture(options.smoke + "/result.bmp");
        std::cout << "PASS actual SDL playback " << options.playback_replay
                  << " fixture=" << options.fixture << " gain1.5=" << saw_fast
                  << " released=" << saw_normal << " result=" << saw_result
                  << " index0 retained;live state RNG inventory and pending command "
                     "unchanged\n";
        running = false;
    }
}
if (!options.smoke.empty() && scene.active() && !scene.transition_active()) {
    static std::set<std::string> saved;
    const double marks[]{.5, 1.4, 2.4, 2.9, 3.2, 4.5, 4.7};
    for (int k = 0; k < 7; ++k)
        if (scene.elapsed() >= marks[k]) {
            auto name = "round_" +
                        std::to_string(options.session_fixture.empty() ? completed + 1
                                                                       : unsigned(scene.serial())) +
                        "_event_" + std::to_string(scene.index()) + "_phase_" + std::to_string(k);
            if (saved.insert(name).second)
                display.capture(options.smoke + "/" + name + ".bmp");
        }
}
if (!options.propaganda_auto_test && !options.console_speech_test &&
    options.playback_replay.empty() && !options.fixture.empty() && options.fixture != "doors" &&
    options.fixture != "revenge-ai" && options.fixture != "revenge-human" &&
    options.fixture != "revenge-bomber" && options.fixture != "round-shutters" &&
    options.fixture != "round-shutters-eliminated" && !scene.automatic_production() &&
    scene.active() && scene.animation_complete()) {
    display.capture(options.smoke + "/result.bmp");
    std::cout << "PASS graphical fixture " << options.fixture
              << " effect=" << scene.event().media.effect << " loss=" << scene.event().media.loss
              << "\n";
    running = false;
}
if (options.fixture == "human-production" || options.fixture == "100mt-production" ||
    options.fixture == "100mt-missile" || options.fixture == "typewriter") {
    static bool started = false;
    static std::set<std::string> prefixes;
    if (scene.automatic_production()) {
        started = true;
        auto text = scene.visible_text();
        if (prefixes.insert(text).second) {
            std::ofstream trace(options.smoke + "/production_stream.jsonl", std::ios::app);
            std::string escaped;
            for (char ch : text) {
                if (ch == '\n')
                    escaped += "\\n";
                else if (ch == '\"' || ch == '\\') {
                    escaped += '\\';
                    escaped += ch;
                } else
                    escaped += ch;
            }
            trace << "{\"text\":\"" << escaped
                  << "\",\"head_y\":" << typewriter_head(text, !scene.text_complete()).y << "}\n";
            if (scene.text_complete())
                display.capture(options.smoke + "/production_complete.bmp");
        }
    } else if (started) {
        display.capture(options.smoke + "/result.bmp");
        std::cout << "PASS automatic production stream completed with no "
                     "clicks;carriage fixed174;original inventory and RNG unchanged\n";
        running = false;
    }
}
if (options.fixture == "speech-action") {
    static bool sent = false;
    if (!sent && display.visual_seconds >= .5) {
        display.capture(options.smoke + "/speech_before.bmp");
        SDL_Event input{};
        input.type = SDL_MOUSEBUTTONDOWN;
        input.button.button = SDL_BUTTON_LEFT;
        input.button.x = 85;
        input.button.y = 15;
        SDL_PushEvent(&input);
        sent = true;
    } else if (sent && !scene.active()) {
        if (!player.accepted_action() ||
            player.coordinator()->data().human.selection.pending_action != 6 ||
            player.coordinator()->phase() != session::Phase::human_selection)
            throw std::runtime_error("single speech action click failed");
        display.capture(options.smoke + "/result.bmp");
        std::cout << "PASS actual SDL same production click dismisses speech and "
                     "selects action exactly once;no banner commit\n";
        running = false;
    }
}
// Developer-only real-round regression: payload/map generations must not
// resurrect completed selection shutters after human-entry doors reopen.
if (options.fixture == "round-shutters" || options.fixture == "round-shutters-eliminated") {
    static unsigned step = 0, rounds = 0, quiet = 0;
    static std::uint64_t begun_round = 0;
    auto mouse = [&](int x, int y) {
        SDL_Event input{};
        input.type = SDL_MOUSEBUTTONDOWN;
        input.button.button = SDL_BUTTON_LEFT;
        input.button.x = x;
        input.button.y = y;
        SDL_PushEvent(&input);
        input.type = SDL_MOUSEBUTTONUP;
        SDL_PushEvent(&input);
    };
    bool idle =
        !display.action_shutters.active() && !display.target_shutters.active() && !queued_selection;
    if (step == 0 && idle && !scene.selection_blocked() && SDL_GetTicks64() >= main_input_after) {
        begun_round = player.round();
        mouse(20, 58);
        step = 1;
    } else if (step == 1 && idle) {
        if (display.action_shutters.visible() != 2)
            throw std::runtime_error("round fixture lost missile selection");
        if (rounds == 0) {
            mouse(60, 105);
            step = 2;
        } else {
            mouse(20, 15);
            step = 3;
        }
    } else if (step == 2 && idle) {
        if (display.action_shutters.visible() != 100)
            throw std::runtime_error("round fixture payload not accepted");
        mouse(20, 15);
        step = 3;
    } else if (step == 3 && idle) {
        if (display.action_shutters.visible() != 0)
            throw std::runtime_error("round fixture lost map return");
        mouse(80, 20);
        step = 4;
    } else if (step == 4 && idle && !scene.active()) {
        if (player.coordinator()->data().human.selection.pending_action != 6)
            throw std::runtime_error("round fixture build not selected");
        mouse(110, 20);
        step = 5;
    } else if (step == 5) {
        if (scene.active() && scene.rendered() && !scene.transition_active() &&
            !scene.human_console() && !scene.accepts_action_click() && scene.animation_complete())
            scene.click(SDL_GetTicks64() + 1000);
        if (player.round() > begun_round && !scene.selection_blocked()) {
            quiet = 0;
            step = 6;
        }
    } else if (step == 6) {
        if (display.action_shutters.active())
            throw std::runtime_error("phantom selection shutter replay after round entry");
        if (++quiet == 10) {
            mouse(20, 15);
            step = 7;
        }
    } else if (step == 7 && idle) {
        if (display.target_shutters.visible() != 0)
            throw std::runtime_error("postround AI selection lost");
        auto &d = player.coordinator()->data();
        if (!player.command({HumanCommandKind::city_target, 0}) ||
            !selector_visible(d, player.coordinator()->phase(), 0, scene.selection_blocked()))
            throw std::runtime_error("postround city cursor unavailable");
        std::ofstream log(options.smoke + "/reentry_trace.txt", std::ios::app);
        log << "round=" << player.round()
            << " phantom=0 ai_city_selection=1 action_serial=" << player.selection_visual_serial()
            << " payload_serial=" << player.payload_visual_serial() << "\n";
        if (++rounds == 3) {
            display.capture(options.smoke + "/result.bmp");
            std::cout << "PASS three actual SDL rounds with missile/payload/map history;no "
                         "phantom reentry;AI/city cursors restored;eliminated_fixture="
                      << (options.fixture == "round-shutters-eliminated") << "\n";
            running = false;
        } else
            step = 0;
    }
}
if (options.fixture.rfind("revenge-", 0) == 0) {
    static std::set<std::size_t> seen, gestures;
    static bool held = false;
    static std::uint64_t held_at = 0;
    static unsigned rendered_attacks = 0;
    static bool death_seen = false;
    if (scene.active() && seen.insert(scene.index()).second) {
        if (scene.event().system_notice)
            death_seen = true;
        else if (scene.event().revenge) {
            if (!death_seen)
                throw std::runtime_error("revenge before death announcement");
            ++rendered_attacks;
        }
        std::ofstream out(options.smoke + "/revenge_presentation.txt", std::ios::app);
        out << "index=" << scene.index() << " death=" << scene.event().system_notice
            << " revenge=" << scene.event().revenge << " action=" << scene.event().action
            << " attacker=" << scene.event().actor << " target=" << scene.event().target
            << " city=" << scene.event().city << "\n";
    }
    auto mouse = [&](Uint32 type) {
        SDL_Event input{};
        input.type = type;
        input.button.button = SDL_BUTTON_LEFT;
        input.button.x = 85;
        input.button.y = 15;
        SDL_PushEvent(&input);
    };
    if (scene.active() && scene.event().revenge && scene.playback_eligible() &&
        !gestures.count(scene.index())) {
        if (options.playback_replay == "click") {
            gestures.insert(scene.index());
            mouse(SDL_MOUSEBUTTONDOWN);
            mouse(SDL_MOUSEBUTTONUP);
        } else if (options.playback_replay == "hold") {
            gestures.insert(scene.index());
            mouse(SDL_MOUSEBUTTONDOWN);
            held = true;
            held_at = SDL_GetTicks64();
        }
    }
    if (held && SDL_GetTicks64() - held_at >= 120) {
        mouse(SDL_MOUSEBUTTONUP);
        held = false;
    }
    if (!scene.active()) {
        if (!death_seen || rendered_attacks == 0 ||
            !same_state(player.coordinator()->data().state, scene.snapshot()))
            throw std::runtime_error("revenge presentation lost events or changed state");
        display.capture(options.smoke + "/result.bmp");
        std::cout << "PASS automatic user-defined revenge " << options.fixture
                  << " attacks=" << rendered_attacks << " mode=" << options.playback_replay
                  << " seed=" << player.coordinator()->data().rng.seed()
                  << " no acknowledgement;all immutable events shown\n";
        running = false;
    }
}
if (options.fixture == "rapid-shutters") {
    static unsigned step = 0;
    auto mouse = [&](int x, int y) {
        SDL_Event e{};
        e.type = SDL_MOUSEBUTTONDOWN;
        e.button.button = SDL_BUTTON_LEFT;
        e.button.x = x;
        e.button.y = y;
        SDL_PushEvent(&e);
        e.type = SDL_MOUSEBUTTONUP;
        SDL_PushEvent(&e);
    };
    if (step == 0 && display.visual_seconds >= .2) {
        mouse(20, 58);
        step = 1;
    } else if (step == 1 && display.action_shutters.phase() == 1) {
        mouse(270, 58);
        mouse(270, 106);
        step = 2;
    } else if (step == 2 && !display.action_shutters.active() && !queued_selection &&
               display.action_shutters.visible() == 7) {
        auto expected = Player::developer_session(*action_selection_start);
        for (int i = 0;
             i < 10 && expected.coordinator()->phase() != session::Phase::human_selection; ++i)
            expected.advance_turn(123);
        expected.command({HumanCommandKind::actor_view, 0});
        expected.command({HumanCommandKind::city_target, 0});
        expected.click(20, 58);
        expected.click(270, 106);
        if (!same_state(player.coordinator()->data().state, expected.coordinator()->data().state) ||
            player.coordinator()->data().rng.seed() != expected.coordinator()->data().rng.seed())
            throw std::runtime_error("coalesced selection state/RNG mismatch");
        display.capture(options.smoke + "/result.bmp");
        std::cout << "PASS rapid selection coalesces latest request;two shutter "
                     "cycles;reference state/RNG match\n";
        running = false;
    }
}
if (options.fixture == "reverse-shutters") {
    static unsigned step = 0;
    static bool pending = false;
    const Word actions[]{2, 0, 4, 0, 7, 0};
    if (!display.action_shutters.active() && display.visual_seconds >= .2) {
        if (pending) {
            if (display.action_shutters.visible() != actions[step])
                throw std::runtime_error("reverse covered content mismatch");
            ++step;
            pending = false;
        }
        if (step == 6) {
            display.capture(options.smoke + "/result.bmp");
            std::cout << "PASS actual SDL AI->Missile->AI->Bomber->AI->Defence->AI;covered "
                         "reverse content;12original door cues\n";
            running = false;
        } else {
            SDL_Event input{};
            input.type = SDL_MOUSEBUTTONDOWN;
            input.button.button = SDL_BUTTON_LEFT;
            input.button.x = step % 2 ? 20 : step == 0 ? 20 : 270;
            input.button.y = step % 2 ? 15 : step == 4 ? 106 : 58;
            SDL_PushEvent(&input);
            input.type = SDL_MOUSEBUTTONUP;
            SDL_PushEvent(&input);
            pending = true;
        }
    }
}
if (options.fixture == "action-shutters") {
    static unsigned step = 0;
    static bool pending = false;
    static std::optional<session::Data> before;
    if (!before)
        before = player.coordinator()->data();
    if (!display.action_shutters.active() && display.visual_seconds >= .2) {
        if (pending) {
            const Word actions[]{2, 100, 4, 100, 7, 2};
            if ((actions[step] != 100 && player.selection_visual_action() != actions[step]) ||
                display.action_shutters.visible() != actions[step])
                throw std::runtime_error("action shutter selection mismatch");
            ++step;
            pending = false;
        }
        if (step == 6) {
            auto expected = Player::developer_session(*action_selection_start);
            for (int i = 0;
                 i < 10 && expected.coordinator()->phase() != session::Phase::human_selection; ++i)
                expected.advance_turn(123);
            expected.command({HumanCommandKind::actor_view, 0});
            expected.command({HumanCommandKind::city_target, 0});
            for (auto p : std::array<std::pair<int, int>, 6>{
                     {{20, 58}, {60, 105}, {270, 58}, {60, 105}, {270, 106}, {20, 58}}})
                expected.click(p.first, p.second);
            if (player.coordinator()->data().rng.seed() !=
                    expected.coordinator()->data().rng.seed() ||
                !same_state(player.coordinator()->data().state,
                            expected.coordinator()->data().state))
                throw std::runtime_error("action shutters differ from normal input state/RNG");
            display.capture(options.smoke + "/result.bmp");
            std::cout << "PASS actual SDL action shutters2->100->4->100->7->2;covered "
                         "switch;matches reference selection state/RNG\n";
            running = false;
        } else {
            SDL_Event input{};
            input.type = SDL_MOUSEBUTTONDOWN;
            input.button.button = SDL_BUTTON_LEFT;
            input.button.x = step == 0 || step == 5 ? 20 : step == 1 || step == 3 ? 60 : 270;
            input.button.y = step == 1 || step == 3 ? 105 : step == 4 ? 106 : 58;
            SDL_PushEvent(&input);
            input.type = SDL_MOUSEBUTTONUP;
            SDL_PushEvent(&input);
            pending = true;
        }
    }
}
if (options.fixture == "target-switch") {
    static unsigned step = 0;
    static double after = .2;
    if (!display.target_shutters.active() && display.visual_seconds >= after) {
        display.capture(options.smoke + "/switch_" + std::to_string(step) + ".bmp");
        std::ofstream out(options.smoke + "/switch_trace.txt", std::ios::app);
        out << "step=" << step << " visible=" << display.target_shutters.visible()
            << " logical=" << player.coordinator()->data().human.logical_view << "\n";
        if (step == 4) {
            std::cout << "PASS actual main target switches4->0->3->0->4;shutters and "
                         "marker context\n";
            running = false;
        } else {
            SDL_Event input{};
            if (step < 3) {
                unsigned slot = step == 1 ? 3 : 0;
                input.type = SDL_MOUSEBUTTONDOWN;
                input.button.button = SDL_BUTTON_LEFT;
                input.button.x = slot < 2 ? 20 : 270;
                input.button.y = slot % 2 ? 165 : 15;
            } else {
                input.type = SDL_KEYDOWN;
                input.key.keysym.sym = SDLK_h;
            }
            SDL_PushEvent(&input);
        }
        ++step;
        after = display.visual_seconds + .3;
    }
}
if (options.fixture == "mood-controls") {
    static int step = 0;
    if (step < 6 && display.visual_seconds >= .3 + step * .6) {
        display.capture(options.smoke + "/control_" + std::to_string(step) + ".bmp");
        auto before = player.coordinator()->data();
        if (step < 5) {
            auto expected = before.state;
            cycle_human_classification(expected, 0);
            if (!player.click(45, 5) || !same_state(expected, player.coordinator()->data().state) ||
                before.rng.seed() != player.coordinator()->data().rng.seed())
                throw std::runtime_error("mood control changed extra state or RNG");
        } else {
            display.capture(options.smoke + "/result.bmp");
            std::cout << "PASS original mood control cycle;only declaration byte;no "
                         "RNG/commit\n";
            running = false;
        }
        ++step;
    }
}
if (options.fixture.rfind("mood-", 0) == 0 && options.fixture != "mood-controls" &&
    !scene.active()) {
    static std::set<int> seen;
    for (int k = 0; k < 8; ++k)
        if (display.visual_seconds >= .1 + k * .4 && seen.insert(k).second)
            display.capture(options.smoke + "/mood_" + std::to_string(k) + ".bmp");
    if (display.visual_seconds >= 3.5) {
        display.capture(options.smoke + "/result.bmp");
        std::cout << "PASS read-only portrait mood replay " << options.fixture << "\n";
        running = false;
    }
}
if (options.fixture == "inspect-missile" || options.fixture == "inspect-bomber" ||
    options.fixture == "inspect-defense") {
    if (display.visual_seconds >= 1.3) {
        display.capture(options.smoke + "/result.bmp");
        std::cout << "PASS read-only inspection preview " << options.fixture
                  << " actor=" << options.replay_target << "\n";
        running = false;
    }
}
if (options.fixture == "silo-selection" || options.fixture == "bomber-selection" ||
    options.fixture == "defense-selection") {
    static std::set<int> seen;
    for (int k = 0; k < 4; ++k)
        if (display.visual_seconds >= .5 + k * .7 && seen.insert(k).second)
            display.capture(options.smoke + "/selection_" + std::to_string(k) + ".bmp");
    if (display.visual_seconds >= 3.5) {
        display.capture(options.smoke + "/result.bmp");
        std::cout << "PASS original selection console " << options.fixture << "\n";
        running = false;
    }
}
if (options.fixture == "finger") {
    if (display.visual_seconds >= 1.3) {
        display.capture(options.smoke + "/result.bmp");
        std::cout << "PASS finger presentation-only replay\n";
        running = false;
    } else if (display.visual_seconds >= .5 && display.visual_seconds < .6)
        display.capture(options.smoke + "/finger_press.bmp");
}
if (options.fixture == "selector") {
    static std::set<int> sources;
    auto frame = city_cursor_frame(display.visual_seconds - display.selector_started_,
                                   display.cursor_period);
    if (sources.insert(frame).second)
        display.capture(options.smoke + "/cursor_frame_" + std::to_string(frame) + ".bmp");
    static std::set<int> frames;
    for (int k = 0; k < 3; ++k)
        if (display.visual_seconds >= .1 + k * .2 && frames.insert(k).second)
            display.capture(options.smoke + "/selector_" + std::to_string(k) + ".bmp");
    if (display.visual_seconds >= 1.2) {
        std::cout << "PASS selector replay\n";
        running = false;
    }
}
if (options.fixture == "doors") {
    if (scene.active() && !scene.transition_active() && scene.animation_complete()) {
        scene.presented();
        scene.next();
    }
    if (!scene.active()) {
        display.capture(options.smoke + "/result.bmp");
        std::cout << "PASS doors replay: closed/open terminal frames\n";
        running = false;
    }
}
if (!options.smoke.empty() && stage >= 11 && options.fixture.empty() &&
    options.session_fixture.empty()) {
    if (scene.active() && scene.rendered() && scene.animation_complete() &&
        SDL_GetTicks64() - last > 190) {
        display.capture(options.smoke + "/round_" +
                        std::to_string(options.session_fixture.empty() ? completed + 1
                                                                       : unsigned(scene.serial())) +
                        "_event_" + std::to_string(scene.index()) + "_message_" +
                        std::to_string(capture_step++) + ".bmp");
        std::ofstream trace(options.smoke + "/scene_trace.txt", std::ios::app);
        trace << "round=" << completed + 1
              << " scene=ActionPresentation rendered event=" << scene.index()
              << " region=" << int(scene.event().region) << " actor=" << scene.event().actor
              << " action=" << scene.event().action << " effect=" << scene.event().media.effect
              << " loss=" << scene.event().media.loss
              << " supported=" << scene.event().media.supported
              << " caption=" << scene.event().caption << " visible=" << scene.visible_text()
              << " frames=" << presented_frames
              << " seed=" << player.coordinator()->data().rng.seed() << "\n";
        captured = scene.index();
        last = SDL_GetTicks64();
        SDL_Event next{};
        if (scene.index() % 3 == 0) {
            next.type = SDL_MOUSEBUTTONDOWN;
            next.button.button = SDL_BUTTON_LEFT;
            next.button.x = 120;
            next.button.y = 90;
        } else {
            next.type = SDL_KEYDOWN;
            next.key.keysym.sym = scene.index() % 3 == 1 ? SDLK_SPACE : SDLK_RETURN;
        }
        SDL_PushEvent(&next);
        if (next.type == SDL_MOUSEBUTTONDOWN) {
            next.type = SDL_MOUSEBUTTONUP;
            SDL_PushEvent(&next);
        }
    } else if (!scene.active() && captured < 99 && SDL_GetTicks64() >= main_input_after) {
        display.capture(options.smoke + "/round_" + std::to_string(completed + 1) + "_main.bmp");
        ++completed;
        captured = 99;
        std::ofstream trace(options.smoke + "/scene_trace.txt", std::ios::app);
        trace << "round=" << completed << " transition=ActionPresentation->Main\n";
        if (completed == 4) {
            if (player.round() != 5 ||
                player.coordinator()->phase() != session::Phase::human_selection)
                throw std::runtime_error("scene smoke did not return to main");
            std::cout << "PASS four rounds: actor events and routed console/speech "
                         "rendered; main input restored\n";
            running = false;
        } else {
            SDL_Event select{};
            select.type = SDL_KEYDOWN;
            select.key.keysym.sym = SDLK_s;
            SDL_PushEvent(&select);
            select.key.keysym.sym = SDLK_RETURN;
            SDL_PushEvent(&select);
        }
    }
}
if ((options.session_fixture == "human-victory" ||
     (options.restart_test && player.human_alive() && !restart_fresh_check)) &&
    scene.active() && scene.rendered() && scene.animation_complete())
    scene.click(SDL_GetTicks64() + 1000);
if (!options.restart_test && !options.session_fixture.empty()) {
    static std::set<std::string> seen;
    bool alive = player.human_alive();
    if (!alive &&
        (scene.human_console() ||
         (!display.console_visible.empty() && !(scene.active() && scene.event().system_notice)) ||
         player.coordinator()->phase() == session::Phase::human_selection))
        throw std::runtime_error("human UI after death");
    std::string key =
        std::to_string(player.round()) + ":" + session::phase_name(player.coordinator()->phase()) +
        ":" +
        (scene.active() ? std::to_string(scene.serial()) + ":" + std::to_string(scene.index())
                        : "none");
    if (seen.insert(key).second) {
        std::ofstream trace(options.smoke + "/ai_only_trace.txt", std::ios::app);
        trace << key << " human_alive=" << alive << " console=" << display.console_visible.size()
              << " system_notice=" << (scene.active() && scene.event().system_notice)
              << " survivors=" << living_actors(player.coordinator()->data().state)
              << " seed=" << player.coordinator()->data().rng.seed() << "\n";
    }
    if (player.ended() && !scene.active() && ending.hall()) {
        if (ending.editing()) {
            ending.input("Tester");
            ending.finish_name();
        }
        display.capture(options.smoke + "/ending.bmp");
        std::uint32_t expected = options.session_fixture == "human-death" ? 4091754757u
                                 : options.session_fixture == "one-ai"    ? 727447730u
                                                                          : 218270086u;
        if ((options.session_fixture == "human-death" || options.session_fixture == "one-ai" ||
             options.session_fixture == "ai-only") &&
            player.coordinator()->data().rng.seed() != expected)
            throw std::runtime_error("AI-only RNG divergence");
        std::cout << "PASS AI-only actual player loop " << options.session_fixture
                  << " final_seed=" << player.coordinator()->data().rng.seed()
                  << " no post-death human console/input\n";
        running = false;
    }
}
