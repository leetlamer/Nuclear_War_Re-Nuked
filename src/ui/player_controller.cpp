#include "ui/player_controller.hpp"
namespace nuclear::player {
namespace {
std::string category(Word a) {
    switch (a) {
    case 0:
        return "No action";
    case 1:
        return "Missile launch";
    case 2:
        return "Missile preparation";
    case 3:
        return "Propaganda / event";
    case 4:
        return "Bomber preparation";
    case 5:
        return "Bomber attack";
    case 6:
        return "Production";
    case 7:
        return "Defense";
    case 88:
        return "Inactive";
    default:
        return "Event " + std::to_string(a);
    }
}
std::string inventory(const Actor &a) {
    unsigned total = 0;
    for (auto v : a.missiles)
        total += v;
    for (auto v : a.warheads)
        total += v;
    for (auto v : a.bombers)
        total += v;
    for (auto v : a.defenses)
        total += v;
    return std::to_string(total);
}
} // namespace

Rect portrait_rect(std::size_t i) {
    if (i >= 10)
        throw Unsupported("portrait outside0..9");
    auto r = player_assets::portraits[i];
    return {r.x, r.y, r.w, r.h};
}
bool selection_header(int x, int y) { return y < 47 || (y < 88 && x > 83 && x < 236); }
Completion complete_selection(std::array<Word, 10> &flags, Rng &rng) {
    unsigned count = 0;
    for (auto flag : flags) {
        if (flag > 1)
            throw Unsupported("selection flag not0/1");
        count += flag;
    }
    if (count > 4)
        throw Unsupported("selection count exceeds4");
    auto f = flags;
    auto random = rng;
    Completion result;
    while (count < 4) {
        auto id = random.next() % 10;
        ++result.fill_draws;
        if (!f[id]) {
            f[id] = 1;
            ++count;
        }
    }
    for (auto &id : result.identity) {
        Word index = Word(random.next() % 10);
        ++result.assignment_draws;
        while (!f[index])
            index = Word((index + 1) % 10);
        id = index;
        f[index] = 0;
    }
    flags = f;
    rng = random;
    return result;
}
bool Selection::toggle(Word id) {
    if (id >= 10)
        return false;
    if (flags[id]) {
        flags[id] = 0;
        --count;
        return true;
    }
    if (count >= 4)
        return false;
    flags[id] = 1;
    ++count;
    // 115952: synchronous selected-flag branch, not a guessed animation tick.
    for (Word other = 0; other < 10; ++other)
        if (other != id)
            timers[other] = add_word(timers[other], Word(rng.next() % 12 + 1));
    return true;
}
Word city_frame(Word p) {
    if (signed_word(p) >= 60)
        return 151;
    if (signed_word(p) >= 30)
        return 224;
    if (signed_word(p) >= 20)
        return 223;
    if (signed_word(p) >= 10)
        return 222;
    if (signed_word(p) >= 5)
        return 221;
    return signed_word(p) >= 1 ? 220 : 150;
}
bool Player::complete_selection_screen() {
    auto next = selection_;
    auto result = complete_selection(next.flags, next.rng);
    next.count = 4;
    session::Data data(next.rng.seed(), true);
    for (std::size_t i = 0; i < 4; ++i) {
        auto id = result.identity[i];
        data.state.identity[i] = id;
        data.state.actors[i].profile = Word(player_assets::profiles[id]);
        data.names[i] = player_assets::names[id];
    }
    data.state.actors[4].profile = 0;
    data.names[4] = "Your Country";
    data.welcome_flag = 1; // original setup writes132620=1
    // Fresh-session explicit zero globals; not reset on each human entry/round.
    data.human.selection.selected_class = 0;
    data.human.selection.selected_capacity = 0;
    auto c = std::make_unique<session::Coordinator>(data);
    session::Adapters adapters{setup_, schedule_};
    if (!c->advance(adapters) || !c->advance(adapters)) {
        diagnostic_ = c->diagnostic()->message;
        return false;
    }
    selection_ = next;
    coordinator_ = std::move(c);
    screen_ = Screen::main;
    committed_quantities_ = coordinator_->data().state;
    messages_.clear();
    const auto &event = coordinator_->events().back();
    if (event.human_entry) {
        messages_ = event.human_entry->prior.messages;
        const auto &tail = event.human_entry->tail.messages;
        messages_.insert(messages_.end(), tail.begin(), tail.end());
    }
    diagnostic_.clear();
    return true;
}
Player Player::developer_session(session::Data initial) {
    struct FixtureSetup : session::Boundaries {
        Word setup(session::Data &, session::CallbackSchedule &) override { return 0; }
    } boundary;
    Player p;
    p.coordinator_ = std::make_unique<session::Coordinator>(std::move(initial));
    session::Adapters adapters{boundary, p.schedule_};
    if (!p.coordinator_->advance(adapters))
        throw Unsupported("fixture setup failed");
    p.screen_ = Screen::main;
    p.committed_quantities_ = p.coordinator_->data().state;
    return p;
}
bool Player::command(HumanCommand cmd) {
    accepted_action_ = false;
    if (!coordinator_ || coordinator_->phase() != session::Phase::human_selection)
        return false;
    session::Adapters a{setup_, schedule_};
    bool delivery_accepted = false;
    Word delivery_action = cmd.kind == HumanCommandKind::missile   ? 2
                           : cmd.kind == HumanCommandKind::bomber  ? 4
                           : cmd.kind == HumanCommandKind::defense ? 7
                                                                   : 0;
    if (delivery_action) {
        auto copy = coordinator_->data().state;
        auto fields = coordinator_->data().human.selection;
        try {
            delivery_accepted =
                select_human_delivery_fields(copy, fields, delivery_action, cmd.argument);
        } catch (const Unsupported &) {
        }
    }
    bool payload_accepted = false;
    if (cmd.kind == HumanCommandKind::warhead && cmd.argument <= 4) {
        const auto &d = coordinator_->data();
        auto fields = d.human.selection;
        payload_accepted =
            select_human_warhead_fields(d.state, fields, cmd.argument, d.human.target_marker) ==
            HumanWarheadCheck::accepted;
    }
    if (!coordinator_->select(cmd, a)) {
        diagnostic_ = coordinator_->diagnostic()->message;
        return false;
    }
    const auto &e = coordinator_->events().back();
    if (e.human_result && cmd.kind != HumanCommandKind::classification)
        messages_ = e.human_result->messages;
    // Presentation cue only: accepted payload selection, before banner commitment.
    accepted_action_ = delivery_accepted || payload_accepted ||
                       cmd.kind == HumanCommandKind::production ||
                       cmd.kind == HumanCommandKind::propaganda;
    if (payload_accepted)
        ++payload_visual_serial_;
    if (cmd.kind == HumanCommandKind::classification && cmd.argument < 4 &&
        populated_cities(coordinator_->data().state.actors[cmd.argument])) {
        ++bleep_serial_;
        constexpr unsigned periods[]{450, 600, 700, 1000, 1300};
        int value =
            int(std::int8_t(coordinator_->data().state.actors[4].classifications[cmd.argument]));
        bleep_period_ = periods[std::size_t(value + 2)];
    }
    if (cmd.kind != HumanCommandKind::classification)
        selection_visual_action_ = delivery_accepted ? delivery_action : 0;
    if (delivery_accepted)
        ++selection_visual_serial_;
    diagnostic_.clear();
    return true;
}
bool Player::confirm_banner() {
    accepted_action_ = false;
    if (!coordinator_ || coordinator_->phase() != session::Phase::human_selection)
        return false;
    session::Adapters a{setup_, schedule_};
    if (!coordinator_->request_banner_confirmation() || !coordinator_->advance(a)) {
        diagnostic_ = coordinator_->diagnostic()->message;
        return false;
    }
    if (coordinator_->events().back().human_result)
        messages_ = coordinator_->events().back().human_result->messages;
    if (coordinator_->phase() == session::Phase::ai_planning) {
        accepted_action_ = true;
        activity_.clear();
        turn_log_.push_back("Human action committed");
    }
    diagnostic_.clear();
    return true;
}
bool Player::advance_turn(Word time) {
    if (resolution_failed_ || !coordinator_ ||
        coordinator_->phase() == session::Phase::human_selection ||
        coordinator_->phase() == session::Phase::exited)
        return false;
    auto from = coordinator_->phase();
    auto before = coordinator_->data().state;
    auto seed_before = coordinator_->data().rng.seed();
    session::Adapters adapters{setup_, schedule_};
    if (!coordinator_->advance(adapters, from == session::Phase::resolver_handoff
                                             ? std::optional<Word>(time)
                                             : std::nullopt)) {
        resolution_failed_ = true;
        diagnostic_ = coordinator_->diagnostic()->message;
        activity_.push_back("Unsupported: " + diagnostic_);
        turn_log_.push_back("Unsupported at " + std::string(session::phase_name(from)) + ": " +
                            diagnostic_);
        return false;
    }
    if (coordinator_->phase() == session::Phase::human_selection ||
        coordinator_->phase() == session::Phase::exited ||
        from == session::Phase::survivor_counting)
        committed_quantities_ = coordinator_->data().state;
    turn_log_.push_back(session::phase_name(from));
    if (from == session::Phase::ai_production) {
        turn_log_.push_back("AI production complete");
        const auto &event = coordinator_->events().back();
        if (event.ai_production)
            for (std::size_t i = 0; i < 4; ++i)
                if (event.ai_production->produced[i])
                    for (const auto &line :
                         production_summary_messages(event.ai_production->production[i]))
                        turn_log_.push_back(coordinator_->data().names[i] + ": " + line);
    }
    if (from == session::Phase::survivor_counting && !human_alive()) {
        ++round_;
        messages_.clear();
        selection_visual_action_ = 0;
        turn_log_.push_back("AI-only round complete");
    }
    if (from == session::Phase::living_human_entry)
        turn_log_.push_back("Human entry / deferred production");
    if (coordinator_->phase() == session::Phase::exited)
        turn_log_.push_back("Game ended (no score persistence)");
    if (from == session::Phase::ai_planning) {
        turn_log_.push_back("Planner RNG " + std::to_string(seed_before) + " -> " +
                            std::to_string(coordinator_->data().rng.seed()));
        for (std::size_t i = 0; i < 4; ++i) {
            const auto &a = coordinator_->data().state.actors[i];
            activity_.push_back(coordinator_->data().names[i] + ": " + category(a.action));
            activity_.push_back(
                "Target: " +
                (a.target_actor < 5 ? coordinator_->data().names[a.target_actor] : "invalid") +
                " city " + std::to_string(a.target_city));
            turn_log_.push_back(
                "slot=" + std::to_string(i) + " identity=" + std::to_string(before.identity[i]) +
                " alive=" + std::to_string(populated_cities(a) != 0) +
                " profile=" + std::to_string(a.profile) + " action=" + std::to_string(a.action) +
                " target=" + std::to_string(a.target_actor) + ":" + std::to_string(a.target_city) +
                " magnitude=" + std::to_string(a.magnitude) +
                " inventory=" + inventory(before.actors[i]) + "->" + inventory(a));
        }
    }
    if (from == session::Phase::resolver_handoff) {
        for (std::size_t i = 0; i < 4; ++i) {
            const auto &a = before.actors[i];
            turn_log_.push_back(coordinator_->data().names[i] + ": " + category(a.action) +
                                (populated_cities(a) == 0 ? " dead: no resolver dispatch"
                                 : a.action == 0 || a.action == 88
                                     ? " inactive: no resolver dispatch"
                                     : " resolved bounded pass"));
        }
        for (std::size_t i = 0; i < 4; ++i) {
            auto action = before.actors[i].action;
            std::string outcome = populated_cities(before.actors[i]) == 0 ? "dead: not dispatched"
                                  : action == 0 || action == 88           ? "inactive"
                                  : action == 2 || action == 4            ? "weapon prepared"
                                  : action == 6 ? "production queued for resolver exit"
                                  : action == 7 ? "defense active"
                                                : "effect resolved (prototype)";
            activity_.push_back(coordinator_->data().names[i] + ": " + outcome);
        }
    }
    if (from == session::Phase::ai_production || from == session::Phase::living_human_entry)
        for (std::size_t i = 0; i < 5; ++i) {
            auto old = inventory(before.actors[i]),
                 now = inventory(coordinator_->data().state.actors[i]);
            if (old != now) {
                auto line = coordinator_->data().names[i] + " inventory " + old + " -> " + now;
                activity_.push_back(line);
                turn_log_.push_back(line);
            }
        }

    if (from == session::Phase::resolver_handoff)
        for (std::size_t i = 0; i < 5; ++i)
            for (std::size_t c = 0; c < 5; ++c) {
                auto now = coordinator_->data().state.actors[i].population[c];
                if (now != before.actors[i].population[c]) {
                    auto line = coordinator_->data().names[i] + " city " + std::to_string(c) +
                                ": " + std::to_string(before.actors[i].population[c]) + " -> " +
                                std::to_string(now);
                    turn_log_.push_back(line);
                    activity_.push_back(line);
                }
            }
    if (coordinator_->phase() == session::Phase::human_selection) {
        ++round_;
        turn_log_.push_back("Next turn ready");
        const auto &e = coordinator_->events().back();
        if (e.human_entry) {
            messages_ = e.human_entry->prior.messages;
            auto &tail = e.human_entry->tail.messages;
            messages_.insert(messages_.end(), tail.begin(), tail.end());
        }
    }
    diagnostic_.clear();
    return true;
}

Rect Player::city_rect(std::size_t actor, std::size_t city) const {
    if (!coordinator_ || actor >= 5 || city >= 5)
        throw Unsupported("city view outside model");
    auto &a = coordinator_->data().state.actors[actor];
    return {int(a.city_x[city]) - player_assets::origin_x[actor] + 100,
            int(a.city_y[city]) - player_assets::origin_y[actor] + 57, 16, 16};
}
bool Player::click(int x, int y) {
    accepted_action_ = false;
    if (x < 0 || y < 0 || x >= 320 || y >= 200)
        return false;
    if (screen_ == Screen::title) {
        enter_selection();
        return true;
    }
    if (screen_ == Screen::selection) {
        if (selection_header(x, y))
            return complete_selection_screen();
        for (Word i = 0; i < 10; ++i)
            if (portrait_rect(i).contains(x, y)) {
                bool changed = selection_.toggle(i);
                if (changed) {
                    ++bleep_serial_;
                    bleep_period_ = selection_.flags[i] ? 1200 : 1700;
                }
                return changed;
            }
        return false;
    }
    if (!coordinator_)
        return false;
    for (auto g : player_assets::gadgets) {
        if (g.id < 97 || g.id > 122 || !Rect{g.x, g.y, g.w, g.h}.contains(x, y))
            continue;
        if (g.id == 113)
            return confirm_banner();
        if (g.id >= 97 && g.id <= 100)
            return command({HumanCommandKind::classification, Word(g.id - 97)});
        if (g.id == 115)
            return command({HumanCommandKind::production, 0});
        if (g.id == 112)
            return command({HumanCommandKind::propaganda, 0});
        if (g.id == 109)
            return command(
                {HumanCommandKind::missile, g.x == 0 ? Word(0) : Word((g.y - 55) / 9 + 1)});
        if (g.id == 119)
            return command(
                {HumanCommandKind::warhead, g.x == 0 ? Word(0) : Word((g.y - 104) / 9 + 1)});
        if (g.id == 122)
            return command({HumanCommandKind::bomber, g.w > 29    ? Word(0)
                                                      : g.y == 58 ? Word(1)
                                                                  : Word(2)});
        if (g.id == 108)
            return command({HumanCommandKind::defense, g.w > 29     ? Word(0)
                                                       : g.y == 106 ? Word(1)
                                                                    : Word(2)});
    }
    const Rect portraits[]{{0, 0, 68, 42}, {0, 149, 68, 42}, {251, 0, 69, 42}, {251, 149, 69, 42}};
    for (Word actor = 0; actor < 4; ++actor)
        if (portraits[actor].contains(x, y))
            return command({HumanCommandKind::actor_view, actor});
    auto view = coordinator_->data().human.logical_view;
    if (Rect{100, 58, 124, 80}.contains(x, y))
        for (Word city = 0; city < 5; ++city)
            if (city_rect(view, city).contains(x, y))
                return command({HumanCommandKind::city_target, city});
    return false;
}
} // namespace nuclear::player
