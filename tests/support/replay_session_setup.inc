// Included only by the replay executable, inside the application frame scope.
if (!options.session_fixture.empty()) {
    if (options.smoke.empty() || !options.fixture.empty())
        throw std::runtime_error("session fixture requires smoke output and no media fixture");
    auto data = endgame_fixture(
        options.session_fixture == "human-death" || options.session_fixture == "human-victory",
        options.session_fixture == "human-victory" || options.session_fixture == "all-dead" ? 0
        : options.session_fixture == "one-ai" || options.session_fixture == "winner-ai"     ? 1
                                                                                            : 3);
    if (options.session_fixture == "human-victory")
        data.state.actors[4].population[0] = 500;
    if (options.session_fixture == "winner-ai")
        data.state.actors[0].population[0] = 500;
    player = Player::developer_session(data);
}
