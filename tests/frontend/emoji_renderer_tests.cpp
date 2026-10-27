// Exercise the real application renderer without adding a production test mode.
#include "rendering/renderer.hpp"
unsigned checks = 0, clicks = 0, tones = 0;
void verify(bool ok, const char *why) {
    ++checks;
    if (!ok)
        throw std::runtime_error(why);
}
void setup(Player &p) {
    p.enter_selection();
    for (Word id : {Word(0), Word(4), Word(2), Word(7)}) {
        auto r = portrait_rect(id);
        verify(p.click(r.x + 20, r.y + 15), "character pick");
    }
    verify(p.click(110, 20), "start");
}
void frame(Renderer &display, Player &p, ActionPresentation &scene,
           const std::string &capture = {}) {
    display.render(p, false, &scene);
    int w = 0, h = 0;
    SDL_GetRendererOutputSize(display.renderer, &w, &h);
    std::vector<unsigned char> pixels(std::size_t(w * h * 4));
    verify(SDL_RenderReadPixels(display.renderer, nullptr, SDL_PIXELFORMAT_RGBA32, pixels.data(),
                                w * 4) == 0,
           "pre-present readback");
    auto controls = nuclear::assets::image("smalltiles").rgba;
    int scale = std::min(w / 320, h / 200), ox = (w - 320 * scale) / 2, oy = (h - 200 * scale) / 2;
    for (std::size_t a = 0; a < 4; ++a) {
        auto indicator = mood_indicator(p.coordinator()->data().state, a);
        if (!indicator)
            continue;
        auto tile = *indicator;
        int dx = a < 2 ? 45 : 296, dy = a % 2 ? 154 : 5;
        for (int y = 0; y < 16; ++y)
            for (int x = 0; x < 16; ++x) {
                int source = ((int(tile) / 20 * 16 + y) * 320 + int(tile) % 20 * 16 + x) * 4;
                const unsigned char rgb[]{controls[source], controls[source + 1],
                                          controls[source + 2]};
                int offset =
                    ((oy + (dy + y) * scale + scale / 2) * w + ox + (dx + x) * scale + scale / 2) *
                    4;
                for (int c = 0; c < 3; ++c)
                    verify(pixels[std::size_t(offset + c)] == rgb[c],
                           "emoji pixels stale on next frame");
            }
    }
    if (!capture.empty())
        display.capture(capture); // also before RenderPresent
    SDL_RenderPresent(display.renderer);
    scene.presented();
}
void press(Renderer &d, Player &p, ActionPresentation &scene, SoundManager &sound,
           std::size_t actor) {
    auto before = p.coordinator()->data();
    auto serial = p.bleep_serial();
    auto speech = scene.speech_visible();
    auto time = scene.elapsed();
    auto text = scene.visible_text();
    auto generation = scene.serial();
    auto index = scene.index();
    auto frozen = scene.snapshot();
    SDL_Event e{};
    e.type = SDL_MOUSEBUTTONDOWN;
    e.button.button = SDL_BUTTON_LEFT;
    e.button.x = actor < 2 ? 45 : 296;
    e.button.y = actor % 2 ? 154 : 5;
    SDL_PushEvent(&e);
    e.type = SDL_MOUSEBUTTONUP;
    SDL_PushEvent(&e);
    while (SDL_PollEvent(&e))
        if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT) {
            verify(p.click(e.button.x, e.button.y), "accepted emoji click");
            if (p.accepted_action())
                scene.dismiss_for_action();
            ++clicks;
        }
    auto expected = before.state;
    cycle_human_classification(expected, actor);
    verify(same_state(expected, p.coordinator()->data().state), "only declaration changes");
    verify(before.rng.seed() == p.coordinator()->data().rng.seed(), "no extra RNG");
    verify(p.bleep_serial() == serial + 1, "one tone request per accepted click");
    verify(sound.bleep(p.bleep_period()), "original bleep queues");
    ++tones;
    verify(scene.speech_visible() == speech && scene.elapsed() == time &&
               scene.visible_text() == text && scene.serial() == generation &&
               scene.index() == index,
           "speech must not dismiss/restart/reveal on emoji input");
    verify(same_state(scene.snapshot(), frozen), "do not rebuild speech snapshot");
    frame(d, p, scene);
    if (speech) {
        auto elapsed = scene.elapsed();
        scene.tick(.04);
        verify(scene.elapsed() > elapsed && scene.speech_visible(), "speech clock continues");
    }
}
int main(int argc, char **argv) {
    try {
        std::string out = argc > 1 ? argv[1] : "emoji_capture";
        std::filesystem::create_directories(out);
        Renderer d;
        SoundManager sound;
        verify(sound.available(), "audio available");
        {
            Player p;
            setup(p);
            ActionPresentation scene;
            frame(d, p, scene);
            for (std::size_t a = 0; a < 4; ++a)
                for (int c = 0; c < 5; ++c)
                    press(d, p, scene, sound, a);
        }
        for (std::size_t speaker = 0; speaker < 4; ++speaker) {
            Player p;
            setup(p);
            ActionEvent e;
            e.actor = speaker;
            e.region = EventRegion::speech;
            e.speech = "The original AI comment continues.";
            auto scene = ActionPresentation::replay_event(e, p.coordinator()->data().state);
            scene.tick(.04);
            frame(d, p, scene, out + "/speaker_" + std::to_string(speaker) + "_before.bmp");
            auto original_text = scene.visible_text();
            for (std::size_t a = 0; a < 4; ++a)
                for (int c = 0; c < 5; ++c)
                    press(d, p, scene, sound, a);
            verify(scene.visible_text().size() > original_text.size(),
                   "speech text progresses normally");
            press(d, p, scene, sound, speaker);
            frame(d, p, scene, out + "/speaker_" + std::to_string(speaker) + "_changed.bmp");
            verify(scene.speech_visible(), "bubble remains");
            verify(scene.skip(), "test ends bubble separately");
            frame(d, p, scene, out + "/speaker_" + std::to_string(speaker) + "_after.bmp");
            auto declared = p.coordinator()->data().state.actors[4].classifications;
            ActionPresentation rounds;
            rounds.observe(p);
            for (unsigned r = 0; r < 3; ++r) {
                verify(p.command({HumanCommandKind::production, 0}) && p.confirm_banner(),
                       "commit ordinary human round");
                for (unsigned step = 0; step < 30 && !rounds.active(); ++step) {
                    verify(p.advance_turn(123), "actual human/AI phases");
                    rounds.observe(p);
                }
                verify(rounds.active(), "round presentation begins");
                for (unsigned t = 0; t < 10000 && rounds.active(); ++t) {
                    rounds.tick(.2);
                    rounds.presented();
                    if (rounds.active() && rounds.animation_complete())
                        rounds.next();
                }
                verify(!rounds.active(), "round presentation completes");
                verify(p.coordinator()->data().state.actors[4].classifications == declared,
                       "intent persists across turns");
                frame(d, p, rounds);
            }
        }
        verify(clicks == 104 && tones == 104, "exact accepted clicks and tone count");
        std::cout << "PASS " << checks << " real renderer checks; " << clicks << " clicks/" << tones
                  << " original bleep requests; all four opponents and speakers; no speech restart "
                     "or RNG change\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "FAIL " << e.what() << " after " << checks << " checks\n";
        return 1;
    }
}
