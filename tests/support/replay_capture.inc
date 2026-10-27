// Included only by the replay executable, inside the application frame scope.
if (!options.smoke.empty() && SDL_GetTicks64() - last > 100) {
    const char *names[]{"title",           "selection_empty", "selection_1", "selection_2",
                        "selection_3",     "selection_4",     "main",        "target_view",
                        "target_selected", "production",      "banner"};
    if (stage < 11 && !display.target_shutters.active()) {
        display.capture(options.smoke + "/" + names[stage] + ".bmp");
        SDL_Event input{};
        input.type = SDL_MOUSEBUTTONDOWN;
        input.button.button = SDL_BUTTON_LEFT;
        if (stage == 0) {
            input.type = 0;
        } else if (stage >= 1 && stage <= 4) {
            const Word ids[]{0, 4, 2, 7};
            auto r = portrait_rect(ids[stage - 1]);
            input.button.x = r.x + 20;
            input.button.y = r.y + 15;
        } else if (stage == 5) {
            input.button.x = 110;
            input.button.y = 20;
        } else if (stage == 6) {
            input.button.x = 20;
            input.button.y = 15;
        } else if (stage == 7) {
            auto r = player.city_rect(0, 0);
            input.button.x = r.x + 4;
            input.button.y = r.y + 4;
        } else if (stage == 8) {
            input.button.x = options.smoke_prepare ? 60 : 80;
            input.button.y = options.smoke_prepare ? 56 : 20;
        } else if (stage == 9) {
            input.button.x = 110;
            input.button.y = 20;
        } else {
            stage = 11;
        }

        if (stage < 10 && input.type)
            SDL_PushEvent(&input);
        ++stage;
        last = SDL_GetTicks64();
    }
}
if (!options.smoke.empty() && scene.transition_active()) {
    static std::set<std::string> gates;
    std::string key = "door_" + std::to_string(scene.serial()) + "_" +
                      std::to_string(unsigned(scene.door_phase())) + "_" +
                      std::to_string(int(scene.closed_fraction() * 4));
    if (gates.insert(key).second) {
        display.capture(options.smoke + "/" + key + ".bmp");
        std::ofstream out(options.smoke + "/door_trace.txt", std::ios::app);
        out << key << " fraction=" << scene.closed_fraction() << "\n";
    }
}

if (!options.smoke.empty() && display.target_shutters.active()) {
    std::string key = "target_door_" +
                      std::to_string(player.coordinator()->data().human.logical_view) + "_" +
                      std::to_string(display.target_shutters.phase()) + "_" +
                      std::to_string(int(display.target_shutters.fraction() * 10));
    display.capture(options.smoke + "/" + key + ".bmp");
    std::ofstream log(options.smoke + "/target_transition.txt", std::ios::app);
    log << "requested=" << display.target_shutters.requested()
        << " visible=" << display.target_shutters.visible()
        << " phase=" << display.target_shutters.phase()
        << " fraction=" << display.target_shutters.fraction() << "\n";
}
if (!options.smoke.empty() && ending.ai_victory()) {
    static std::set<int> heights;
    auto pose =
        victory_pose(player.coordinator()->data().state.identity[std::size_t(ending.winner())],
                     ending.elapsed(), ending.jump_seconds());
    if (heights.insert(pose.height).second)
        display.capture(options.smoke + "/victory_height_" + std::to_string(pose.height) + ".bmp");
}
if (!options.smoke.empty() && scene.active() && scene.event().region == EventRegion::action) {
    std::ofstream log(options.smoke + "/event_text_trace.txt", std::ios::app);
    log << scene.event().action << "|" << scene.event().media.variant << "|"
        << scene.event().media.result_variant << "|" << scene.elapsed() << "|"
        << scene.visible_text() << "\n";
}
if (!options.smoke.empty() && display.action_shutters.active()) {
    std::ofstream log(options.smoke + "/action_transition.txt", std::ios::app);
    log << "visible=" << display.action_shutters.visible()
        << " requested=" << display.action_shutters.requested()
        << " phase=" << display.action_shutters.phase()
        << " fraction=" << display.action_shutters.fraction() << "\n";
    display.capture(options.smoke + "/action_" + std::to_string(display.action_shutters.phase()) +
                    "_" + std::to_string(display.action_shutters.requested()) + ".bmp");
}
