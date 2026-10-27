#include "platform/resources.hpp"
#include "rendering/animations/selection_sprites.hpp"
#include "rendering/animations/special_events.hpp"
#include "ui/interface_layout.hpp"
#include <SDL.h>
#include <fstream>
#include <iostream>
using namespace nuclear::player;
int main() {
    SDL_SetMainReady();
    SDL_Init(0);
    auto atlas = nuclear::assets::image("sprite_tiles", "masked");
    std::vector<unsigned char> rgba(16 * 16 * 4);
    unsigned opaque = 0;
    for (int y = 0; y < 16; ++y)
        for (int x = 0; x < 16; ++x) {
            int p = (192 + y) * 320 + x, o = (y * 16 + x) * 4;
            std::copy_n(atlas.rgba.begin() + p * 4, 4, rgba.begin() + o);
            opaque += rgba[o + 3] != 0;
        }
    auto surface = SDL_CreateRGBSurfaceWithFormat(0, 16, 16, 32, SDL_PIXELFORMAT_RGBA32);
    auto renderer = SDL_CreateSoftwareRenderer(surface);
    auto texture =
        SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, 16, 16);
    SDL_UpdateTexture(texture, nullptr, rgba.data(), 64);
    SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
    auto draw = [&](bool flip) {
        SDL_SetRenderDrawColor(renderer, 255, 0, 255, 255);
        SDL_RenderClear(renderer);
        SDL_RenderCopyEx(renderer, texture, nullptr, nullptr, 0, nullptr,
                         flip ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE);
        std::vector<unsigned char> pixels(1024);
        if (SDL_RenderReadPixels(renderer, nullptr, SDL_PIXELFORMAT_RGBA32, pixels.data(), 64))
            throw std::runtime_error(SDL_GetError());
        return pixels;
    };
    auto right = draw(false), left = draw(true);
    unsigned checks = 0;
    for (int y = 0; y < 16; ++y)
        for (int x = 0; x < 16; ++x)
            for (int c = 0; c < 4; ++c) {
                ++checks;
                if (right[(y * 16 + x) * 4 + c] != left[(y * 16 + 15 - x) * 4 + c])
                    return 1;
            }
    if (!opaque || opaque == 256)
        return 2;
    for (int p = 0; p < 256; ++p)
        if (!rgba[p * 4 + 3]) {
            ++checks;
            if (right[p * 4] != 255 || right[p * 4 + 1] != 0 || right[p * 4 + 2] != 255)
                return 3;
        }
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_FreeSurface(surface);
    // Exercise the actual SDL masked blitter for recovered multi-cell specials/hands.
    std::vector<DrawSprite> sprites{
        small_sprite(72, 32, 16, 0, 0, 1), small_sprite(203, 16, 16, 0, 0),
        small_sprite(280, 32, 32, 0, 0),   small_sprite(250, 16, 16, 0, 0),
        small_sprite(255, 16, 16, 0, 0),   small_sprite(286, 32, 32, 0, 0),
        small_sprite(131, 16, 16, 0, 0),   small_sprite(260, 32, 10, 0, 0, 2)};
    for (auto action : {2, 4})
        for (double seconds : {0., .5, 1.2, 2.5}) {
            auto frames = selection_sprites(nuclear::Word(action), seconds);
            sprites.insert(sprites.end(), frames.begin(), frames.end());
        }
    for (double t : {.24, .6, .96}) {
        auto f = finger_pose(t);
        sprites.push_back({Atlas::big, f->source, f->destination, false});
    }
    sprites.push_back({Atlas::big, {0, 208, 16, 16}, {0, 0, 16, 16}, false});
    for (auto sprite : sprites) {
        auto image = sprite.atlas == Atlas::small
                         ? atlas
                         : nuclear::assets::image(
                               sprite.atlas == Atlas::large ? "large_sprites" : "flight_effects",
                               sprite.atlas == Atlas::large ? "masked" : "masked");
        int w = sprite.source.w, h = sprite.source.h;
        std::vector<unsigned char> rgba2(w * h * 4);
        for (int y = 0; y < h; ++y)
            for (int x = 0; x < w; ++x) {
                int src = (sprite.source.y + y) * 320 + sprite.source.x + x, dst = (y * w + x) * 4;
                std::copy_n(image.rgba.begin() + src * 4, 4, rgba2.begin() + dst);
            }
        auto target = SDL_CreateRGBSurfaceWithFormat(0, w, h, 32, SDL_PIXELFORMAT_RGBA32);
        auto draw = SDL_CreateSoftwareRenderer(target);
        auto tex = SDL_CreateTexture(draw, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, w, h);
        SDL_UpdateTexture(tex, nullptr, rgba2.data(), w * 4);
        SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
        for (bool flip : {false, true}) {
            SDL_SetRenderDrawColor(draw, 255, 0, 255, 255);
            SDL_RenderClear(draw);
            SDL_RenderCopyEx(draw, tex, nullptr, nullptr, 0, nullptr,
                             flip ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE);
            std::vector<unsigned char> actual(w * h * 4);
            if (SDL_RenderReadPixels(draw, nullptr, SDL_PIXELFORMAT_RGBA32, actual.data(), w * 4))
                return 4;
            for (int y = 0; y < h; ++y)
                for (int x = 0; x < w; ++x) {
                    int src = (y * w + x) * 4, dst = (y * w + (flip ? w - 1 - x : x)) * 4;
                    for (int c = 0; c < 3; ++c) {
                        ++checks;
                        unsigned char expected = rgba2[src + 3] ? rgba2[src + c] : c == 1 ? 0 : 255;
                        if (actual[dst + c] != expected)
                            return 5;
                    }
                }
        }
        SDL_DestroyTexture(tex);
        SDL_DestroyRenderer(draw);
        SDL_FreeSurface(target);
    }
    SDL_Quit();
    std::cout << "PASS " << checks
              << " SDL sprite mirror/transparency checks; original masked pixels retained\n";
}
