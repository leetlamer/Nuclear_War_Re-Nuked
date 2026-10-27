#ifdef NWR_REFERENCE
#define main reference_application_main
#include NWR_REFERENCE_FRONTEND
#undef main
using GameRenderer = Display;
#else
#include "rendering/renderer.hpp"
#include "tests/support/session_fixtures.hpp"
using GameRenderer = Renderer;
#endif
int main(int argc, char **argv) {
    try {
        std::filesystem::create_directories(argv[1]);
        GameRenderer display;
        Player player(1);
        ActionPresentation scene;
        auto capture = [&](const std::string &name) {
            display.render(player, false, &scene);
            display.capture(std::string(argv[1]) + "/" + name + "_pre.bmp");
            SDL_RenderPresent(display.renderer);
            display.capture(std::string(argv[1]) + "/" + name + "_post.bmp");
            scene.presented();
        };
        player.enter_selection();
        capture("selection");
        for (Word id : {Word(0), Word(4), Word(2), Word(7)}) {
            auto r = portrait_rect(id);
            player.click(r.x + 20, r.y + 15);
        }
        capture("four_selected");
        player.click(110, 20);
        capture("main");
        for (Word action : {Word(1), Word(5), Word(7), Word(11), Word(16), Word(86)}) {
            auto state = player.coordinator()->data().state;
            for (auto &a : state.actors)
                a.action = 0;
            state.actors[4].action = action;
            state.actors[4].target_actor = 0;
            state.actors[4].target_city = 0;
            state.actors[4].magnitude = 5;
            scene = ActionPresentation::fixture(replay_media(state, 1),
                                                player.coordinator()->data().names, 4);
            for (int step = 0; step < 4; ++step) {
                scene.tick(.5);
                display.visual_seconds += .5;
                capture("action_" + std::to_string(action) + "_" + std::to_string(step));
            }
        }
        auto data = endgame_fixture(true, 0);
        data.state.actors[4].population[0] = 500;
        player = Player::developer_session(data);
        for (unsigned i = 0; i < 30 && !player.ended(); ++i)
            player.advance_turn(37);
        scene = {};
        EndgamePresentation ending;
        ending.begin(player, false);
        if (ending.editing()) {
            ending.input("Tester");
            ending.finish_name();
        }
        ending.tick(8);
        display.endgame = &ending;
        capture("high_scores");
        display.endgame = nullptr;
        player = Player(1);
        player.enter_selection();
        display.reset_session(SELECTION_SHUTTER_SECONDS);
        capture("restart_selection");
        std::cout << "PASS native pre/post renderer probe\n";
        return argc < 2;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
