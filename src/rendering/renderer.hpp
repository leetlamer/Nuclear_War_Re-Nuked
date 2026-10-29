#pragma once
#include "audio/audio_manager.hpp"
#include "platform/city_diagnostics.hpp"
#include "platform/mood_diagnostics.hpp"
#include "platform/resources.hpp"
#include "rendering/animations/action_sprites.hpp"
#include "rendering/animations/earthquake.hpp"
#include "rendering/animations/special_events.hpp"
#include "ui/action_selection_shutters.hpp"
#include "ui/animation_playback.hpp"
#include "ui/endgame.hpp"
#include "ui/interface_layout.hpp"
#include "ui/new_game_transition.hpp"
#include "ui/player_controller.hpp"
#include "ui/selection_chrome.hpp"
#include "ui/target_selection.hpp"
#include "ui/turn_presentation.hpp"
#include <SDL.h>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
using namespace nuclear;
using namespace nuclear::player;
namespace {
std::vector<unsigned char> bytes(const std::string &p) {
    std::ifstream f(p, std::ios::binary);
    if (!f)
        throw std::runtime_error("cannot read " + p);
    return {std::istreambuf_iterator<char>(f), {}};
}
class Renderer {
    // Context outlives textures, renderer and window; reverse member destruction is intentional.
    struct VideoContext {
        VideoContext() {
            SDL_SetMainReady();
            if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0)
                throw std::runtime_error(SDL_GetError());
        }
        ~VideoContext() { SDL_Quit(); }
    } video_;
    std::unique_ptr<SDL_Window, decltype(&SDL_DestroyWindow)> owned_window_{nullptr,
                                                                            SDL_DestroyWindow};
    std::unique_ptr<SDL_Renderer, decltype(&SDL_DestroyRenderer)> owned_renderer_{
        nullptr, SDL_DestroyRenderer};
    std::vector<std::unique_ptr<SDL_Texture, decltype(&SDL_DestroyTexture)>> owned_textures_;

    MoodClocks mood_clocks_;
    std::optional<State> mood_preview_;
    bool preview_speaking_{};
    std::optional<State> inspection_;
    std::size_t inspection_actor_{};
    SDL_Texture *earth_{}, *earth_small_{}, *earth_large_{}, *winner_art_{}, *space_{}, *ending_{}, *start_{}, *dip_{}, *heads_{}, *world_{},
        *tiles_{}, *font_{}, *missiles_{}, *controls_{}, *large_{}, *large_opaque_{};
    ScrollingTextFlow main_text_;
    ConsoleTranscript console_;
    const session::Coordinator *console_owner_{};
    std::uint64_t main_generation_{};
    std::string main_message_;
    std::uint64_t main_tick_{};
    std::uint64_t chrome_action_{}, chrome_payload_{}, chrome_serial_{};
    Word chrome_content_{};
    std::uint64_t payload_seen_{}, selection_seen_{};
    double selection_start_{};
    double finger_start_{-100};

  public:
    const ConsoleTranscript &console_history() const { return console_; }
    MoodAudit *mood_audit{};
    TargetViewShutters target_shutters;
    ActionSelectionShutters action_shutters;
    double cursor_period{.6}, exhaust_cutoff{.46}, ai_arrow_period{1.12};
    EndgamePresentation *endgame{};
    void reset_session(double door_duration) {
        mood_clocks_ = MoodClocks{};
        mood_preview_.reset();
        preview_speaking_ = false;
        inspection_.reset();
        inspection_actor_ = 0;
        main_text_ = ScrollingTextFlow{};
        console_ = ConsoleTranscript{};
        console_owner_ = nullptr;
        main_generation_ = main_tick_ = 0;
        main_message_.clear();
        payload_seen_ = selection_seen_ = chrome_action_ = chrome_payload_ = chrome_serial_ = 0;
        chrome_content_ = 0;
        selection_start_ = 0;
        finger_start_ = -100;
        target_shutters = TargetViewShutters{};
        action_shutters = ActionSelectionShutters{};
        target_shutters.duration(door_duration);
        action_shutters.duration(door_duration);
        selection_replay = 0;
        finger_replay = false;
        selector_key_ = 0xffff;
        selector_started_ = visual_seconds = 0;
        console_visible.clear();
        console_typing = false;
        log_scroll = 0;
    }
    void observe_selection(const Player &p, bool allowed) {
        bool action = p.selection_visual_serial() != chrome_action_,
             payload = p.payload_visual_serial() != chrome_payload_;
        Word desired = chrome_content_;
        if (action || payload) {
            chrome_action_ = p.selection_visual_serial();
            chrome_payload_ = p.payload_visual_serial();
            desired = payload ? Word(100) : p.selection_visual_action();
        }
        if (p.coordinator() && p.coordinator()->data().human.display_actor != 0xffff)
            desired = 0;
        if (action || payload || desired != chrome_content_) {
            chrome_content_ = desired;
            ++chrome_serial_;
        }
        action_shutters.observe(chrome_serial_, chrome_content_, allowed);
    }
    void cancel_selection() { action_shutters.observe(chrome_serial_, chrome_content_, false); }
    void mood_preview(const State &state, bool speaking = false) {
        mood_preview_ = state;
        preview_speaking_ = speaking;
    }
    double stork_speed{DEFAULT_STORK_SPEED};
    Word selection_replay{};
    double finger_seconds{1.2};
    double visual_seconds{}, selector_started_{};
    Word selector_key_{0xffff};
    bool debug_hitboxes{}, finger_replay{};
    std::size_t log_scroll{};
    double preparation_seconds{.4};
    std::string console_visible;
    bool console_typing{};
    int explosion_size{20};
    SDL_Window *window{};
    SDL_Renderer *renderer{};
    void inspection_preview(const State &state, std::size_t actor) {
        inspection_ = state;
        inspection_actor_ = actor;
    }
    Renderer() {
        SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
        window = SDL_CreateWindow("Nuclear War: Re-Nuked", SDL_WINDOWPOS_CENTERED,
                                  SDL_WINDOWPOS_CENTERED, 960, 600,
                                  SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
        if (!window)
            throw std::runtime_error(SDL_GetError());
        owned_window_.reset(window);
        nuclear::assets::set_window_icon(window);
        SDL_SetWindowMinimumSize(window, 320, 200);
        renderer =
            SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
        if (!renderer)
            renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
        if (!renderer)
            throw std::runtime_error(SDL_GetError());
        owned_renderer_.reset(renderer);
        SDL_RenderSetLogicalSize(renderer, 320, 200);
        SDL_RenderSetIntegerScale(renderer, SDL_TRUE);
        winner_art_ = image_texture("victory_body", "masked");
        space_ = image_texture("space_ending");
        earth_ = image_texture("expmap");
        earth_small_ = image_texture("smallexp");
        earth_large_ = image_texture("bigexp");
        ending_ = image_texture("desolate_ending");
        large_ = image_texture("large_sprites", "masked");
        large_opaque_ = image_texture("large_sprites");
        controls_ = image_texture("sprite_tiles");
        missiles_ = image_texture("flight_effects", "masked");
        start_ = image_texture("character_selection");
        dip_ = image_texture("main_window");
        heads_ = image_texture("leader_portraits");
        world_ = image_texture("world_map");
        tiles_ = image_texture("sprite_tiles", "masked");
        font_ = image_texture("bitmap_font");
    }
    ~Renderer() = default;
    Renderer(const Renderer &) = delete;
    Renderer &operator=(const Renderer &) = delete;
    SDL_Texture *texture(const std::vector<unsigned char> &p, int w, int h) {
        auto t =
            SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, w, h);
        if (!t)
            throw std::runtime_error(SDL_GetError());
        owned_textures_.emplace_back(t, SDL_DestroyTexture);
        SDL_UpdateTexture(t, nullptr, p.data(), w * 4);
        SDL_SetTextureBlendMode(t, SDL_BLENDMODE_BLEND);
        return t;
    }
    SDL_Texture *image_texture(std::string_view key, std::string_view variant = {}) {
        auto bitmap = nuclear::assets::image(key, variant);
        return texture(bitmap.rgba, int(bitmap.width), int(bitmap.height));
    }
    void copy(SDL_Texture *t, SDL_Rect s, SDL_Rect d) { SDL_RenderCopy(renderer, t, &s, &d); }
    void text(std::string s, int x, int y, SDL_Color color = {0, 0, 0, 255}) {
        SDL_SetTextureColorMod(font_, color.r, color.g, color.b);
        int origin = x;
        for (unsigned char c : s) {
            if (c == '\n') {
                x = origin;
                y += 8;
                continue;
            }
            auto g = player_assets::glyphs[c >= 32 && c <= 127 ? c - 32 : 96];
            copy(font_, {g.offset, 0, g.width, 8}, {x, y, g.width, 8});
            x += 6;
        }
    }
    void speech_text(const std::string &s, int x, int y) {
        SDL_SetTextureColorMod(font_, 187, 0, 0);
        for (unsigned char c : s) {
            auto g = player_assets::glyphs[c >= 32 && c <= 127 ? c - 32 : 96];
            copy(font_, {g.offset, 0, g.width, 8}, {x, y, std::max(1, int(g.width) * 5 / 6), 7});
            x += 5;
        }
    }
    void action_text(std::string line, int y, SDL_Color color = {255, 221, 17, 255}) {
        line = action_line(line);
        double scale = action_glyph_scale(line.size());
        SDL_SetTextureColorMod(font_, color.r, color.g, color.b);
        for (std::size_t i = 0; i < line.size(); ++i) {
            unsigned char c = line[i];
            auto g = player_assets::glyphs[c >= 32 && c <= 127 ? c - 32 : 96];
            copy(font_, {g.offset, 0, g.width, 8},
                 {2 + int(i * 6 * scale), y, std::max(1, int(g.width * scale)), 8});
        }
    }
    void tile(Word index, Rect d) {
        copy(tiles_, {int(index % 20) * 16, int(index / 20) * 16, 16, 16}, {d.x, d.y, d.w, d.h});
    }
    void render(const Player &player, bool show_log = false,
                const ActionPresentation *scene = nullptr) {
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        if (player.screen() != Screen::main) {
            SDL_RenderCopy(renderer, start_, nullptr, nullptr);
            if (player.screen() == Screen::selection) {
                for (Word id = 0; id < 10; ++id) {
                    auto r = selection_face_rect(id);
                    Word frame = smiling_selection_frame(id, visual_seconds);
                    copy(heads_, {int(id % 2) * 160 + int(frame) * 32, int(id / 2) * 32, 32, 32},
                         {r.x, r.y, 32, 32});
                }

                for (Word id = 0; id < 10; ++id)
                    if (player.selection().flags[id]) {
                        auto r = portrait_rect(id);
                        tile(233, {r.x + 45, r.y + 5, 16, 16});
                    }
                auto title = "Nuclear War - select 1–4 opponents (" +
                             std::to_string(player.selection().count) + "/4); click title to begin";
                SDL_SetWindowTitle(window, title.c_str());
            } else
                SDL_SetWindowTitle(window,
                                   "Nuclear War - press a key or click to select opponents");
            return;
        }
        if (endgame && endgame->active()) {
            SDL_RenderCopy(renderer, endgame->winner() < 0 ? space_ : ending_, nullptr, nullptr);
            if (endgame->winner() < 0 && !endgame->hall()) {
                int stage = extinction_stage(endgame->elapsed());
                if (stage < int(extinction_frames.size()))
                    SDL_RenderCopy(renderer, earth_, nullptr, nullptr);
                // Original copies accumulate on each screen buffer. Reconstruct that
                // deterministic composition each frame rather than retaining GPU contents.
                for (int i = 0; stage < int(extinction_frames.size()) && i <= stage; ++i) {
                    auto frame = extinction_frames[std::size_t(i)];
                    auto r = frame.source, d = frame.destination;
                    copy(frame.large ? earth_large_ : earth_small_,
                         {r.x,r.y,r.w,r.h}, {d.x,d.y,d.w,d.h});
                }
            }
            if (endgame->ai_victory()) {
                auto pose = victory_pose(
                    player.coordinator()->data().state.identity[std::size_t(endgame->winner())],
                    endgame->elapsed(), endgame->jump_seconds());
                auto b = pose.body_source, d = pose.body_destination;
                copy(winner_art_, {b.x, b.y, b.w, b.h}, {d.x, d.y, d.w, d.h});
                b = pose.head_source;
                d = pose.head_destination;
                copy(winner_art_, {b.x, b.y, b.w, b.h}, {d.x, d.y, d.w, d.h});
            }
            if (endgame->hall()) {
                text(" NUCLEAR WAR", 124, 4, {255, 255, 255, 255});
                text("Hall -O- Fame", 124, 14, {255, 255, 255, 255});
                text("#    Name    Left  Cities  Score", 45, 29, {255, 255, 255, 255});
                for (std::size_t i = 0; i < 10; ++i) {
                    const auto &row = endgame->rows()[i];
                    SDL_Color color = endgame->rank() && *endgame->rank() == i
                                          ? SDL_Color{255, 238, 0, 255}
                                          : SDL_Color{255, 255, 255, 255};
                    auto line = hall_field(std::to_string(i + 1), 2, 2) + " " +
                                hall_field(row.name, 10, 10, true) + "  " +
                                hall_numbers(row.numbers);
                    text(line, 45, 39 + int(i) * 10, color);
                }
                if (endgame->winner() >= 0) {
                    auto value = endgame->score();
                    auto label = endgame->winner() == 4
                                     ? std::string(" Your Score:")
                                     : " " +
                                           hall_field(player.coordinator()
                                                          ->data()
                                                          .names[std::size_t(endgame->winner())],
                                                      10, 10, true) +
                                           ":";
                    text(label + "  " +
                             hall_numbers(
                                 {value.score, value.population_total, value.positive_cities}),
                         45, 159, {255, 255, 255, 255});
                }
            }
            console_visible.clear();
            console_typing = false;
            SDL_SetWindowTitle(window, "Nuclear War - Hall -O- Fame");
            return;
        }
        SDL_RenderCopy(renderer, dip_, nullptr, nullptr);
        auto &c = *player.coordinator();
        auto &data = c.data();
        if (console_owner_ != &c) {
            console_owner_ = &c;
            console_ = ConsoleTranscript{};
            main_text_ = ScrollingTextFlow{};
            main_message_.clear();
            main_generation_ = 0;
            main_tick_ = 0;
        }
        const auto committed_display = player.display_state();
        const State &face_state = mood_preview_ ? *mood_preview_
                                  : scene       ? scene->interface_state(committed_display)
                                                : committed_display;
        Word shown_view = inspection_ ? Word(inspection_actor_) : target_shutters.visible();
        Word numeric_view = data.human.display_actor == 0xffff ? Word(4) : shown_view;
        bool allow_marker = ai_selection_active(c.phase(), player.human_alive(), player.ended(),
                                                scene && scene->selection_blocked(),
                                                data.human.display_actor == 0xffff);
        for (std::size_t i = 0; i < 4; ++i) {
            if (!face_state.participating[i])
                continue;
            auto identity = face_state.identity[i];
            auto r = portrait_rect(identity);
            int x = i < 2 ? 13 : 264, y = (i % 2) ? 154 : 5;
            bool speaking = (scene && scene->speech_visible() && scene->event().actor == i) ||
                            (preview_speaking_ && i == 0);
            auto face = mood_clocks_.pose(face_state, i, visual_seconds, speaking);
            face.indicator = mood_indicator(mood_preview_ ? *mood_preview_ : committed_display, i);
            bool alive = face.animation != FaceState::eliminated;
            if (alive)
                copy(heads_, {face.source.x, face.source.y, 32, 32}, {x, y, 32, 32});
            copy(start_, {r.x, r.y + 44, 68, 5}, {i < 2 ? 0 : 251, (i % 2) ? 193 : 44, 68, 5});
            if (face.indicator) {
                auto index = *face.indicator;
                copy(controls_, {int(index % 20) * 16, int(index / 20) * 16, 16, 16},
                     {i < 2 ? 45 : 296, (i % 2) ? 154 : 5, 16, 16});
            }
            if (alive) {
                if (allow_marker && shown_view == i) {
                    tile(ai_marker_frame(visual_seconds, ai_arrow_period), ai_marker_rect(i));
                } else if (auto number = ai_red_number(face_state, i, numeric_view)) {
                    std::string value = (*number < 10 ? " " : "") + std::to_string(*number) + " ";
                    int nx = i < 2 ? 46 : 297, ny = i % 2 ? 174 : 25;
                    text(value, nx, ny, {0, 0, 0, 255});
                    text(value, nx + 1, ny, {221, 0, 0, 255});
                }
            }
            if (mood_audit)
                mood_audit->pose(face, mood_preview_ ? *mood_preview_ : committed_display,
                                 mood_preview_
                                     ? "developer immutable preview"
                                     : (&face_state == &data.state ? "authoritative state"
                                                                   : "immutable scene snapshot"));
        }
        auto view = std::size_t(shown_view);
        if (view >= 5)
            view = 4;
        const auto &display_state =
            inspection_ ? *inspection_
                        : (scene ? scene->interface_state(committed_display) : committed_display);
        SDL_Rect clip{100, 58, 124, 80};
        SDL_RenderSetClipRect(renderer, &clip);
        copy(world_, {player_assets::origin_x[view], player_assets::origin_y[view], 124, 80}, clip);
        const auto &actor = display_state.actors[view];
        for (std::size_t city = 0; city < 5; ++city) {
            auto r = player.city_rect(view, city);
            tile(city_frame(actor.population[city]), r);
            if (view == 4 && actor.population[city]) {
                int x = r.x + 18;
                if (x > 198)
                    x = r.x - 10;
                auto population = std::to_string(signed_word(actor.population[city]));
                text(population, x - 1, r.y + 14, {0, 0, 0, 255});
                text(population, x, r.y + 13, {255, 255, 255, 255});
            }
        }
        if (!target_shutters.active() &&
            selector_visible(data, c.phase(), view, scene && scene->selection_blocked())) {
            Word key = Word(view * 5 + data.state.actors[4].target_city);
            if (key != selector_key_) {
                selector_key_ = key;
                selector_started_ = visual_seconds;
            }
            tile(city_cursor_frame(visual_seconds - selector_started_, cursor_period),
                 city_cursor_rect(player.city_rect(view, data.state.actors[4].target_city)));
        } else
            selector_key_ = 0xffff;
        if (auto deployed = deployed_sprite(display_state, view, visual_seconds);
            deployed && !(view == 4 && actor.action == 2)) {
            auto d = deployed->destination;
            d.x += 100 - player_assets::origin_x[view];
            d.y += (actor.action == 4 ? 58 : 57) - player_assets::origin_y[view];
            if (actor.action == 2)
                tile(171, {d.x - 4, d.y + 15, 16, 16});
            auto source = deployed->source;
            copy(deployed->atlas == Atlas::big ? missiles_ : tiles_,
                 {source.x, source.y, source.w, source.h}, {d.x, d.y, d.w, d.h});
        }
        SDL_RenderSetClipRect(renderer, nullptr);
        const auto &human = display_state.actors[4];
        for (auto indicator : availability(human)) {
            SDL_SetRenderDrawColor(renderer, indicator.available ? 0 : 187,
                                   indicator.available ? 221 : 0, 0, 255);
            SDL_Rect r{indicator.box.x, indicator.box.y, indicator.box.w, indicator.box.h};
            SDL_RenderFillRect(renderer, &r);
        }
        if (prepared_missile(display_state) && view == 4) {
            const int x = 143 - player_assets::origin_x[4] + 100 - 4,
                      y = 101 - player_assets::origin_y[4] + 58 + 8;
            tile(171, {x, y, 16, 16});
            copy(missiles_, {0, 192, 16, 16}, {x + 4, y - 15, 16, 16});
        }
        if (player.selection_visual_serial() != selection_seen_) {
            selection_seen_ = player.selection_visual_serial();
            selection_start_ = visual_seconds;
        }
        Word preview = selection_replay ? selection_replay : action_shutters.visible();
        if (preview && preview != 100 && (!scene || !scene->selection_blocked()) &&
            c.phase() == session::Phase::human_selection) {
            auto r = selection_background(preview);
            if (preview == 7) {
                copy(large_opaque_, {0, 288, 124, 80}, {100, 58, 124, 80});
            } else
                copy(large_opaque_, {r.x, r.y, r.w, r.h}, {100, 58, 124, 80});
            SDL_Rect area{100, 58, 124, 80};
            SDL_RenderSetClipRect(renderer, &area);
            for (auto frame :
                 selection_sprites(preview, selection_replay ? visual_seconds - selection_start_
                                                             : action_shutters.visible_seconds())) {
                auto r = frame.source, d = frame.destination;
                copy(large_, {r.x, r.y, r.w, r.h}, {d.x, d.y, d.w, d.h});
            }
            SDL_RenderSetClipRect(renderer, nullptr);
        }
        if (player.payload_visual_serial() != payload_seen_) {
            payload_seen_ = player.payload_visual_serial();
            finger_start_ = visual_seconds;
        }
        if (finger_replay ||
            ((!scene || !scene->selection_blocked()) &&
             c.phase() == session::Phase::human_selection && action_shutters.visible() == 100)) {
            copy(controls_, {196, 64, 124, 80}, {100, 58, 124, 80});
            if (auto hand =
                    finger_pose(finger_replay ? visual_seconds : action_shutters.visible_seconds(),
                                finger_seconds)) {
                SDL_Rect area{100, 58, 124, 80};
                SDL_RenderSetClipRect(renderer, &area);
                copy(missiles_, {hand->source.x, hand->source.y, hand->source.w, hand->source.h},
                     {hand->destination.x, hand->destination.y, hand->destination.w,
                      hand->destination.h});
                SDL_RenderSetClipRect(renderer, nullptr);
            }
        }

        // Static rendering of exact existing human messages; never run text RNG here.
        std::vector<std::string> lines;
        for (auto s : player.messages()) {
            while (s.size() > 27) {
                auto n = s.rfind(' ', 27);
                if (n == std::string::npos || n == 0)
                    n = 27;
                lines.push_back(s.substr(0, n));
                s.erase(0, n + (n < s.size() && s[n] == ' '));
            }
            lines.push_back(s);
        }
        int first = 0;
        std::string current;
        for (std::size_t i = first; i < lines.size(); ++i) {
            if (!current.empty())
                current += "\n";
            current += lines[i];
        }
        if (current != main_message_) {
            main_message_ = current;
            ++main_generation_;
            main_text_.set(current, 27, 3);
        }
        auto clock = SDL_GetTicks64();
        if (!scene || scene->main_text_visible())
            main_text_.tick(main_tick_ ? (clock - main_tick_) / 1000.0 : 0);
        main_tick_ = clock;
        if (scene && scene->human_console()) {
            console_.observe(1, scene->console_source(), scene->console_prefix());
            console_typing = !scene->text_complete();
        } else {
            console_.observe(0, main_generation_, main_text_.revealed());
            console_typing = !main_text_.complete();
        }
        console_visible = console_.visible();
        auto typed = console_typing_frame(console_visible, console_typing);
        {
            auto head = typed.head;
            SDL_Rect clip_head{78, 158, 162, 36};
            SDL_RenderSetClipRect(renderer, &clip_head);
            copy(tiles_, {304, 176, 16, 16}, {head.x, head.y, 16, 16});
            SDL_RenderSetClipRect(renderer, nullptr);
        }
        int row = 0;
        int first_y = typed.first_y;
        for (auto &line : wrap_text(typed.text, 27))
            text(line, 78, first_y + 8 * row++);
        {
            auto r = typewriter_foreground();
            copy(dip_, {r.x, r.y, r.w, r.h}, {r.x, r.y, r.w, r.h});
        }
        if (!player.human_alive() || player.ended()) {
            SDL_RenderCopy(renderer, world_, nullptr, nullptr);
            for (auto &a : display_state.actors)
                if (display_state.participating[std::size_t(&a - display_state.actors.data())])
                for (int city = 0; city < 5; ++city)
                    tile(city_frame(a.population[city]),
                         {int(a.city_x[city]), int(a.city_y[city]) - 1, 16, 16});
            console_visible.clear();
            console_typing = false;
            if (player.ended()) {
                std::string ending = "No country survives.";
                for (std::size_t actor = 0; actor < 5; ++actor)
                    if (populated_cities(data.state.actors[actor]))
                        ending = data.names[actor] + " is the last surviving country.";
                SDL_SetRenderDrawColor(renderer, 0, 102, 51, 255);
                SDL_Rect bottom{0, 184, 320, 16};
                SDL_RenderFillRect(renderer, &bottom);
                action_text(ending, 184);
            }
        }

        if (scene && scene->show_action_map()) {
            SDL_RenderCopy(renderer, world_, nullptr, nullptr);
            const auto state = scene->population_state();
            const auto &event = scene->event();
            for (std::size_t i = 0; i < 5; ++i)
                if (state.participating[i])
                for (std::size_t city = 0; city < 5; ++city) {
                    const auto &a = state.actors[i];
                    tile(event.action == 86 && i == event.target && city == event.city
                             ? Word(150)
                             : city_frame(a.population[city]),
                         {int(a.city_x[city]), int(a.city_y[city]) - 1, 16, 16});
                }
            // Original portrait and city art; caption placement/outline are prototype UI.

            if (debug_hitboxes && event.media.supported &&
                (event.action == 1 || event.action == 3 || event.action == 5) && event.target < 5 &&
                event.city < 5) {
                const auto &target = state.actors[event.target];
                SDL_SetRenderDrawColor(renderer, 255, 221, 17, 255);
                SDL_Rect box{int(target.city_x[event.city]) - 1, int(target.city_y[event.city]) - 2,
                             18, 18};
                SDL_RenderDrawRect(renderer, &box);
            }
            const double elapsed = scene->draw_elapsed();
            if (event.media.supported && (event.action == 1 || event.action == 5) &&
                event.target < 5 && event.city < 5) {
                const auto &target = state.actors[event.target];
                const double tx = target.city_x[event.city] + 8,
                             ty = int(target.city_y[event.city]) + 7;
                const auto launch = missile_launch_origin(event.actor);
                const double sx = launch.x, sy = launch.y;
                if (elapsed < 2.8) {
                    auto pose = flight_pose(elapsed / 2.8, sx, sy, tx, ty);
                    int w = std::max(1, int(16 * pose.scale)),
                        h = std::max(1, int(32 * pose.scale));
                    SDL_Rect src{0, 192, 16,
                                 missile_exhaust(elapsed / 2.8, exhaust_cutoff) ? 32 : 16},
                        dst{int(pose.x) - w / 2, int(pose.y) - h / 2, w, h};
                    SDL_Point pivot{w / 2, h / 2};
                    if (!missile_exhaust(elapsed / 2.8, exhaust_cutoff))
                        dst.h = std::max(1, int(16 * pose.scale));
                    if (event.action == 1)
                        SDL_RenderCopyEx(renderer, missiles_, &src, &dst, pose.angle, &pivot,
                                         SDL_FLIP_NONE);

                } else if (impact_visual(event.media) == ImpactVisual::explosion &&
                           (elapsed < 4.0 || scene->instant_result())) {
                    int frame = 180 + std::min(22, int((elapsed - 2.8) / 1.2 * 23));
                    tile(Word(frame), centered_effect(int(tx), int(ty), explosion_size));
                }
                if (event.action == 5) {
                    auto aircraft = bomber_sequence(event.actor, elapsed, tx, ty, event.media);
                    if (aircraft)
                        copy(tiles_,
                             {int(aircraft->frame % 20) * 16, int(aircraft->frame / 20) * 16, 16,
                              16},
                             {int(aircraft->x) - 8, int(aircraft->y) - 8, 16, 16});
                }
            }
            if (event.media.supported && event.action == 2) {
                auto origin = action_origin(2, event.actor);
                int x = origin.x, y = origin.y + 8,
                    h = preparation_height(elapsed, preparation_seconds);
                tile(171, {x - 4, y, 16, 16});
                if (h > 0)
                    copy(missiles_, {0, 192, 16, h}, {x, y + 1 - h, 16, h});
            }
            if (event.media.supported && !special_art(event.action)) {
                auto sprite = action_sprite(event.action,
                                            event.action == 7 ? std::min(elapsed, 2.0) : elapsed);
                if (sprite) {
                    auto origin = action_origin(event.action, event.actor);
                    int x = origin.x, y = origin.y;
                    if (event.action == 4) {
                        auto pose = bomber_orbit(event.actor, elapsed);
                        x = int(pose.x) - 8;
                        y = int(pose.y) - 8;
                        sprite->index = pose.frame;
                    }
                    if ((event.action == 8 || event.action == 14) && event.target < 5 &&
                        event.city < 5) {
                        double t = std::min(1.0, elapsed / 2.0);
                        if (event.action == 14) {
                            x -= 4;
                            y -= 4;
                        }
                        int tx = state.actors[event.target].city_x[event.city] +
                                 (event.action == 14 ? 8 : 0),
                            ty = state.actors[event.target].city_y[event.city] -
                                 (event.action == 14 ? 16 : 0);
                        x = int(x + (tx - x) * t);
                        y = int(y + (ty - y) * t);
                    }
                    SDL_Rect source{sprite->index % 20 * 16, sprite->index / 20 * 16, sprite->width,
                                    sprite->height},
                        destination{x, y, sprite->width, sprite->height};
                    bool flip =
                        event.action == 8 && event.target < 5 && event.city < 5 &&
                        defector_flip(origin.x, state.actors[event.target].city_x[event.city]);
                    SDL_RenderCopyEx(renderer, tiles_, &source, &destination, 0, nullptr,
                                     flip ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE);
                }
            }
            if (event.media.supported && special_art(event.action) && event.actor < 5 &&
                event.target < 5 && event.city < 5) {
                const auto &target = state.actors[event.target];
                int tx = target.city_x[event.city], ty = target.city_y[event.city];
                for (auto sprite :
                     special_sprites(event.action, event.actor, elapsed, tx, ty,
                                     event.media.population_before, {.7, 2, 3, stork_speed})) {
                    SDL_Rect source{sprite.source.x, sprite.source.y, sprite.source.w,
                                    sprite.source.h},
                        destination{sprite.destination.x, sprite.destination.y,
                                    sprite.destination.w, sprite.destination.h};
                    SDL_RenderCopyEx(renderer, sprite.atlas == Atlas::small ? tiles_ : missiles_,
                                     &source, &destination, 0, nullptr,
                                     sprite.flip ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE);
                }
            }
            if (event.media.supported && event.action == 86 && event.target < 5 && event.city < 5) {
                const auto &a = state.actors[event.target];
                for (auto sprite :
                     earthquake_sprites(elapsed, a.city_x[event.city], a.city_y[event.city],
                                        event.media.population_before)) {
                    auto r = sprite.source, d = sprite.destination;
                    copy(tiles_, {r.x, r.y, r.w, r.h}, {d.x, d.y, d.w, d.h});
                }
            }
            if (event.laser_preview && event.target < 5 && event.city < 5) {
                const auto &target = state.actors[event.target];
                int tx = target.city_x[event.city] + 8, ty = target.city_y[event.city] + 8;
                auto craft = small_sprite(260 + 2 * (int(elapsed * 8) % 8), 32, 10, tx - 16,
                                          std::max(0, ty - 48), 2);
                auto r = craft.source, d = craft.destination;
                copy(tiles_, {r.x, r.y, r.w, r.h}, {d.x, d.y, d.w, d.h});
                if (elapsed >= .7 && elapsed < 2) {
                    SDL_SetRenderDrawColor(renderer, 255, 85, 85, 255);
                    SDL_RenderDrawLine(renderer, tx, d.y + 10, tx, ty);
                }
                text("DEVELOPER LASER APPROXIMATION", 2, 0, {255, 255, 255, 255});
            }
            SDL_SetRenderDrawColor(renderer, 0, 102, 51, 255);
            SDL_Rect caption{0, 184, 320, 16};
            SDL_RenderFillRect(renderer, &caption);
            if (event.system_notice) {
                int row = 0;
                for (const auto &line : wrap_text(scene->visible_text(), 52))
                    if (row < 2)
                        text(line, 2, 184 + 8 * row++, {255, 221, 17, 255});
                console_visible = scene->visible_text();
                console_typing = !scene->text_complete();
            } else
                action_text(scene->visible_text(), 184);
        }
        if (scene && scene->speech_visible()) {
            auto r = bubble_rect(scene->event().actor, scene->event().speech.size());
            SDL_Rect box{r.x, r.y, r.w, r.h};
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
            SDL_RenderFillRect(renderer, &box);
            box = {r.x + 1, r.y + 1, r.w - 2, r.h - 2};
            SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
            SDL_RenderFillRect(renderer, &box);
            int line = 0;
            for (auto row : wrap_text(scene->visible_text(), 26))
                if (line < 2)
                    speech_text(row, r.x + 6, r.y + 3 + 7 * line++);
        }
        if (scene && scene->door_visible() && player.human_alive() &&
            c.phase() == session::Phase::human_selection) {
            double f = scene->closed_fraction();
            int h = int(f * 40);
            if (f >= 1)
                copy(dip_, {100, 58, 124, 80}, {100, 58, 124, 80});
            else if (h > 0) {
                h = std::min(39, h);
                copy(dip_, {100, 97 - h, 124, h}, {100, 58, 124, h});
                copy(dip_, {100, 98, 124, h}, {100, 138 - h, 124, h});
            }
        }
        if (target_shutters.active() && (!scene || !scene->selection_blocked()) &&
            player.human_alive() && c.phase() == session::Phase::human_selection) {
            double f = target_shutters.fraction();
            int h = std::min(40, int(f * 40));
            if (f >= 1)
                copy(dip_, {100, 58, 124, 80}, {100, 58, 124, 80});
            else if (h > 0) {
                copy(dip_, {100, 97 - h, 124, h}, {100, 58, 124, h});
                copy(dip_, {100, 98, 124, h}, {100, 138 - h, 124, h});
            }
        }
        if (action_shutters.active() &&
            target_shutter_owner(player.screen(), player.human_alive(), player.ended(), c.phase(),
                                 scene && scene->selection_blocked())) {
            double f = action_shutters.fraction();
            int h = std::min(40, int(f * 40));
            if (f >= 1)
                copy(dip_, {100, 58, 124, 80}, {100, 58, 124, 80});
            else if (h > 0) {
                copy(dip_, {100, 97 - h, 124, h}, {100, 58, 124, h});
                copy(dip_, {100, 98, 124, h}, {100, 138 - h, 124, h});
            }
        }
        if (show_log) {
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
            SDL_Rect panel{0, 0, 320, 184};
            SDL_RenderFillRect(renderer, &panel);
            text("PROTOTYPE TURN LOG - TAB CLOSES", 1, 0, {255, 255, 255, 255});
            auto &log = player.turn_log();
            auto tail = log.size() > 20 ? log.size() - 20 : 0;
            auto first = tail > log_scroll ? tail - log_scroll : 0;
            for (std::size_t i = first; i < std::min(log.size(), first + 20); ++i)
                text(log[i].substr(0, 52), 1, 8 + 8 * int(i - first), {255, 255, 255, 255});
            if (!player.diagnostic().empty())
                text("Unsupported: " + player.diagnostic().substr(0, 38), 1, 176,
                     {255, 90, 90, 255});
        }
        std::string title = "Nuclear War - Round " + std::to_string(player.round());
        if (!player.diagnostic().empty())
            title += " - This interaction is not available in this build";
        SDL_SetWindowTitle(window, title.c_str());
    }
    void capture(const std::string &file) {
        int w, h;
        SDL_GetRendererOutputSize(renderer, &w, &h);
        auto s = SDL_CreateRGBSurfaceWithFormat(0, w, h, 32, SDL_PIXELFORMAT_RGBA32);
        if (!s)
            throw std::runtime_error(SDL_GetError());
        if (SDL_RenderReadPixels(renderer, nullptr, s->format->format, s->pixels, s->pitch) != 0 ||
            SDL_SaveBMP(s, file.c_str()) != 0) {
            SDL_FreeSurface(s);
            throw std::runtime_error(SDL_GetError());
        }
        SDL_FreeSurface(s);
    }
};
} // namespace
