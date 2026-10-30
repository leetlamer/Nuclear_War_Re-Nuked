#include "rendering/renderer.hpp"

// Compare the actual stationary city pixels during launch and after resolution.
// A second ground copy of the airborne city must not survive the background redraw.
int main() {
    Renderer display;
    Player player;
    player.enter_selection();
    auto portrait = portrait_rect(0);
    player.click(portrait.x + 20, portrait.y + 15);
    player.click(110, 20);
    auto before = player.coordinator()->data().state;
    auto after = before;
    after.actors[4].population[0] = 0;
    ActionEvent event;
    event.region = EventRegion::action;
    event.actor = event.target = 4;
    event.city = 0;
    event.action = 11;
    event.media.supported = true;
    event.media.population_before = before.actors[4].population[0];
    event.before = before;
    event.after = after;
    auto ground_pixels = [&](double seconds) {
        auto scene = ActionPresentation::replay_event(event, before);
        scene.tick(seconds);
        display.render(player, false, &scene);
        int width, height;
        SDL_GetRendererOutputSize(display.renderer, &width, &height);
        std::vector<unsigned char> pixels(width * height * 4), ground;
        if (SDL_RenderReadPixels(display.renderer, nullptr, SDL_PIXELFORMAT_RGBA32,
                                 pixels.data(), width * 4) != 0)
            throw std::runtime_error(SDL_GetError());
        for (int y = 81 * height / 200; y < 97 * height / 200; ++y)
            for (int x = 130 * width / 320; x < 146 * width / 320; ++x)
                for (int channel = 0; channel < 3; ++channel)
                    ground.push_back(pixels[(y * width + x) * 4 + channel]);
        return ground;
    };
    const auto airborne_ground = ground_pixels(1.);
    const auto resolved_ground = ground_pixels(3.2);
    std::size_t differences = 0;
    for (std::size_t pixel = 0; pixel < airborne_ground.size(); ++pixel)
        differences += airborne_ground[pixel] != resolved_ground[pixel];
    std::cout << "city ground RGB differences during flight versus resolved=" << differences << '\n';
    return differences ? 1 : 0;
}
