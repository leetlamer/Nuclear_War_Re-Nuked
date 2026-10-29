// Included only by the replay executable, inside the application frame scope.
if (!options.fixture.empty()) {
    if (options.smoke.empty())
        throw std::runtime_error("media fixture requires smoke-test output");
    player.enter_selection();
    for (Word id : {Word(0), Word(4), Word(2), Word(7)}) {
        auto r = portrait_rect(id);
        player.click(r.x + 20, r.y + 15);
    }
    player.click(110, 20);
    State state = player.coordinator()->data().state;
    for (auto &a : state.actors)
        a.action = 0;
    state.actors[options.replay_actor].action = 1;
    state.actors[options.replay_actor].target_actor = options.replay_target;
    state.actors[options.replay_actor].target_city = options.replay_city;
    state.actors[options.replay_actor].magnitude = 5;
    if (options.fixture == "earthquake")
        state.actors[options.replay_actor].action = 86;
    if (options.fixture == "stork")
        state.actors[options.replay_actor].action = 9;
    if (options.fixture == "alien-departure")
        state.actors[options.replay_actor].action = 11;
    if (options.fixture == "alien-delivery")
        state.actors[options.replay_actor].action = 16;
    if (options.fixture == "catapult-cow")
        state.actors[options.replay_actor].action = 13;
    if (options.fixture == "catapult-person")
        state.actors[options.replay_actor].action = 14;
    if (options.fixture == "meltdown")
        state.actors[options.replay_actor].action = 15;
    if (options.fixture == "weight")
        state.actors[options.replay_actor].action = 12;
    if (options.fixture == "transfer")
        state.actors[options.replay_actor].action = 14;
    if (options.fixture == "bomber-attack")
        state.actors[options.replay_actor].action = 5;
    if (options.fixture == "missile-preparation")
        state.actors[options.replay_actor].action = 2;
    if (options.fixture == "defector" || options.fixture == "defector-left" ||
        options.fixture == "defector-right") {
        state.actors[options.replay_actor].action = 8;
        if (options.fixture != "defector")
            state.actors[0].city_x[2] = options.fixture == "defector-left" ? 60 : 240;
    }
    if (options.fixture == "propaganda")
        state.actors[options.replay_actor].action = 3;
    if (options.fixture == "production")
        state.actors[options.replay_actor].action = 6;
    if (options.fixture == "defense") {
        state.actors[options.replay_actor].action = 7;
        state.actors[options.replay_actor].magnitude = 0;
    }
    if (options.fixture == "bomber")
        state.actors[options.replay_actor].action = 4;
    if (options.fixture == "failed-defense") {
        state.actors[options.replay_actor].action = 5;
        state.actors[0].action = 7;
        state.actors[0].magnitude = 0;
    }
    if (options.fixture == "bomber-interception")
        state.actors[options.replay_actor].action = 5;
    if (options.fixture == "interception" || options.fixture == "bomber-interception") {
        state.actors[0].action = 7;
        state.actors[0].magnitude = options.fixture == "bomber-interception" ? 1 : 0;
    }
    scene = ActionPresentation::fixture(replay_media(state, 1), player.coordinator()->data().names,
                                        options.replay_actor);
    if (options.fixture == "speech-action" || options.fixture == "speech" ||
        options.fixture == "portrait") {
        ActionEvent event;
        event.actor = 0;
        event.region = EventRegion::speech;
        event.identity = state.identity[0];
        event.speech = reaction_text(event.identity, 1);
        scene = ActionPresentation::replay_event(event, state);
    }
    if (options.fixture == "human-production" || options.fixture == "100mt-production" ||
        options.fixture == "100mt-missile" || options.fixture == "ai-production" ||
        options.fixture == "typewriter") {
        Rng private_rng(1);
        MessageSequenceRng context{true};
        ActionEvent event;
        event.actor = options.fixture == "ai-production" ? 0 : 4;
        event.name = player.coordinator()->data().names[event.actor];
        event.action = 6;
        event.caption = event.name + " produces weapons.";
        if (event.actor == 4) {
            auto result = produce_human(state, context, private_rng);
            event.production = result.inventory;
            if (options.fixture == "100mt-production" || options.fixture == "100mt-missile") {
                event.production = Production{};
                if (options.fixture == "100mt-production")
                    event.production.warheads[3] = 2;
                else
                    event.production.missiles[3] = 2;
            }
            event.production_lines = production_summary_messages(event.production);
            event.production_heading = result.messages.front();
            event.region = EventRegion::human_console;
        } else {
            event.production_lines = production_summary_messages(produce(state, 0, private_rng));
            event.media.supported = true;
            event.media.action = 6;
            event.media.variant = 1;
            event.caption = original_caption(event.media, event.name, "");
        }
        scene = ActionPresentation::replay_event(event, state);
    }
    if (options.fixture == "doors") {
        ActionEvent e;
        e.caption = "Door transition replay";
        scene = ActionPresentation::replay_event(e, state);
        scene.transitions(true);
        scene.begin_transition();
    }
    if (options.fixture == "selector") {
        player.command({HumanCommandKind::actor_view, 0});
        player.command({HumanCommandKind::city_target, 0});
        scene = ActionPresentation{};
    }
    stage = 12;
}

if (options.fixture == "revenge-ai" || options.fixture == "revenge-human" ||
    options.fixture == "revenge-bomber") {
    auto resolved =
        revenge_fixture(options.fixture == "revenge-human", options.fixture == "revenge-bomber");
    player = Player::developer_session(resolved);
    scene = ActionPresentation::resolved_fixture(resolved);
}
if (options.fixture == "silo-selection" || options.fixture == "bomber-selection" ||
    options.fixture == "defense-selection") {
    scene = ActionPresentation{};
    if (options.fixture == "defense-selection") {
        if (!player.command({HumanCommandKind::defense, 0}) ||
            player.selection_visual_action() != 7)
            throw std::runtime_error("defence preview not connected to accepted selection");
    } else
        display.selection_replay = options.fixture == "silo-selection" ? 2 : 4;
}
if (options.fixture == "action-shutters" || options.fixture == "reverse-shutters" ||
    options.fixture == "round-shutters" || options.fixture == "round-shutters-eliminated" ||
    options.fixture == "rapid-shutters") {
    auto initial = player.coordinator()->data();
    initial.state.actors[4].missiles.fill(2);
    initial.state.actors[4].bombers.fill(2);
    initial.state.actors[4].defenses.fill(2);
    initial.state.actors[4].warheads.fill(2);
    initial.state.actors[4].action = 2;
    initial.state.human_prepared_capacity = 100;
    initial.human.target_marker = 0;
    initial.state.actors[4].target_actor = 0;
    initial.state.actors[4].target_city = 0;
    if (options.fixture == "round-shutters-eliminated")
        initial.state.actors[1].population.fill(0);
    action_selection_start = initial;
    player = Player::developer_session(initial);
    for (int i = 0; i < 10 && player.coordinator()->phase() != session::Phase::human_selection; ++i)
        if (!player.advance_turn(123))
            throw std::runtime_error("action shutter fixture setup failed");
    player.command({HumanCommandKind::actor_view, 0});
    player.command({HumanCommandKind::city_target, 0});
}
if (options.fixture == "target-switch" || options.fixture == "action-shutters" ||
    options.fixture == "reverse-shutters" || options.fixture == "round-shutters" ||
    options.fixture == "round-shutters-eliminated" || options.fixture == "rapid-shutters")
    scene = ActionPresentation{};
if (options.fixture == "finger") {
    scene = ActionPresentation{};
    display.finger_replay = true;
}
if (options.fixture == "inspect-missile" || options.fixture == "inspect-bomber" ||
    options.fixture == "inspect-defense") {
    State snapshot = player.coordinator()->data().state;
    snapshot.actors[options.replay_target].action = options.fixture == "inspect-missile"  ? 2
                                                    : options.fixture == "inspect-bomber" ? 4
                                                                                          : 7;
    display.inspection_preview(snapshot, options.replay_target);
    scene = ActionPresentation{};
}
if (options.fixture == "alien-laser") {
    ActionEvent e;
    e.actor = options.replay_actor;
    e.target = options.replay_target;
    e.city = options.replay_city;
    e.caption = "Developer laser preview. Original trigger unresolved.";
    e.outcome = "Presentation only; no population or inventory effects.";
    e.laser_preview = true;
    scene = ActionPresentation::replay_event(e, player.coordinator()->data().state);
}

if (options.fixture.rfind("mood-", 0) == 0) {
    scene = ActionPresentation{};
    if (options.fixture != "mood-controls") {
        State preview = player.coordinator()->data().state;
        for (std::size_t actor = 0; actor < 4; ++actor) {
            preview.identity[actor] = Word(actor);
            preview.actors[actor].profile = Word(player_assets::profiles[actor]);
            preview.actors[actor].scores[4] = options.fixture == "mood-happy"   ? Word(-20000)
                                              : options.fixture == "mood-angry" ? Word(20000)
                                                                                : 0;
            preview.actors[4].classifications[actor] = 0;
        }
        if (options.fixture == "mood-mixed") {
            const int scores[]{-20000, -12000, 12000, 20000};
            for (int i = 0; i < 4; ++i)
                preview.actors[i].scores[4] = Word(scores[i]);
        }
        if (options.fixture == "mood-profile4") {
            preview.identity[0] = 7;
            preview.identity[1] = 5;
            preview.actors[0].profile = preview.actors[1].profile = 4;
            preview.actors[0].scores[4] = 20000;
            preview.actors[1].scores[4] = Word(-20000);
        }
        if (options.fixture == "mood-eliminated")
            preview.actors[2].population = {};
        if (options.fixture == "mood-speaking") {
            ActionEvent e;
            e.actor = 0;
            e.identity = 0;
            e.region = EventRegion::speech;
            e.speech = reaction_text(0, 1);
            scene = ActionPresentation::replay_event(e, preview);
        }
        display.mood_preview(preview);
    }
    stage = 12;
}
if (!options.session_fixture.empty())
    stage = 12;
if (options.console_speech_test && (options.smoke.empty() || options.fixture != "speech"))
    throw std::runtime_error("console speech test requires speech fixture and smoke output");
std::optional<session::Data> console_speech_before;
if (options.console_speech_test)
    console_speech_before = player.coordinator()->data();
if (options.title_selection_test) {
    if (options.smoke.empty() || !options.fixture.empty() || !options.session_fixture.empty())
        throw std::runtime_error("title selection test requires only smoke output");
    stage = 12;
}
if (!options.playback_replay.empty() && (options.smoke.empty() || options.fixture.empty()))
    throw std::runtime_error("playback replay requires media fixture and smoke output");
std::optional<session::Data> playback_before;
if (!options.playback_replay.empty())
    playback_before = player.coordinator()->data();
