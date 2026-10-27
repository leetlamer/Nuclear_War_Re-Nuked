// Reference build: add the original player/include directories and define NWR_REFERENCE.
#ifdef NWR_REFERENCE
#include "developer_sessions.hpp"
#include "model.hpp"
#else
#include "tests/support/session_fixtures.hpp"
#include "ui/player_controller.hpp"
#endif
#include <iomanip>
#include <iostream>
using namespace nuclear;
using namespace nuclear::player;
void dump(const Player &p) {
    const auto &d = p.coordinator()->data();
    const auto &s = d.state;
    std::cout << p.round() << ' ' << session::phase_name(p.coordinator()->phase()) << ' '
              << d.rng.seed() << ' ';
    for (const auto &a : s.actors)
        for (auto b : encode_actor(a))
            std::cout << std::hex << std::setw(2) << std::setfill('0') << unsigned(b);
    std::cout << std::dec << ' ';
    for (auto x : s.identity)
        std::cout << x << ',';
    for (auto x : s.selected_opponent)
        std::cout << x << ',';
    for (auto x : s.remaining_total)
        std::cout << x << ',';
    for (auto x : s.pass_processed)
        std::cout << x << ',';
    for (auto x : s.prepared_capacity)
        std::cout << x << ',';
    std::cout << s.human_prepared_capacity << ' ';
    for (unsigned i = 0; i < 5; ++i) {
        std::cout << d.revenge.announced[i] << ':' << d.revenge.attacks[i] << ':'
                  << d.revenge.airborne[i].active << ':' << d.revenge.airborne[i].capacity << ':'
                  << d.revenge.airborne[i].reserved_payload << ';';
    }
    std::cout << '\n';
}
void run(Player &p, unsigned limit) {
    for (unsigned tick = 0; tick < 2000 && !p.ended() && p.round() < limit; ++tick) {
        dump(p);
        if (p.coordinator()->phase() == session::Phase::human_selection) {
            p.command({HumanCommandKind::production, 0});
            if (!p.confirm_banner())
                throw Unsupported("trace commit");
        } else if (!p.advance_turn(Word(p.round() * 37 + 123)))
            throw Unsupported(p.diagnostic());
    }
    dump(p);
}
int main() {
    for (unsigned seed : {1u, 2u, 29u, 42u, 77u, 123u, 65535u, 123456u}) {
        Player p(seed);
        p.enter_selection();
        for (Word id : {Word(0), Word(4), Word(2), Word(7)}) {
            auto r = portrait_rect(id);
            p.click(r.x + 20, r.y + 15);
        }
        p.click(110, 20);
        run(p, 30);
    }
    for (bool human : {false, true}) {
        auto p = Player::developer_session(endgame_fixture(human, 3));
        run(p, 30);
    }
    return 0;
}
