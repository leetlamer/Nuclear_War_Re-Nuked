#include "core/game_text.hpp"
#include "core/game_text_data.hpp"
using nuclear::text::Argument;
#include "tests/fixtures/format_fixtures.hpp"
#include <iostream>
#include <stdexcept>
using namespace nuclear::text;
unsigned checks = 0;
void check(bool p) {
    ++checks;
    if (!p)
        throw std::runtime_error("text assertion " + std::to_string(checks));
}
template <class F> void rejects(F f) {
    bool caught = false;
    try {
        f();
    } catch (const std::exception &) {
        caught = true;
    }
    check(caught);
}
int main() {
    try {
        for (const auto &row : generated::rows)
            check(lookup(row.address).text == row.text);
        for (const auto &p : generated::pointers) {
            if (p.address)
                check(table(p.table, p.index).address == p.address);
            else
                rejects([&] { (void)table(p.table, p.index); });
        }
        for (const auto &fixture : format_fixtures) {
            const auto actual = format(fixture.address, fixture.args);
            if (actual != fixture.expected)
                std::cerr << std::hex << fixture.address << " expected[" << fixture.expected
                          << "] actual[" << actual << "]\n";
            check(actual == fixture.expected);
        }
        check(lookup(0x10937e).text == "for propoganada.");
        check(lookup(0x10980b).text == "for propaganada.");
        check(table("leader_names", 0).text == "Ron Raygun");
        check(table("leader_names", 1).text == "PM Satcher");
        check(table("leader_names", 9).text == "Ghanji");
        check(lookup(0x100912).text == "Design  : Eric Hyman       ");
        check(format(0x108f38, {signed16(0xffff)}) == "-1 million dead.");
        check(format(0x10d3e4, {std::int32_t(3), std::int32_t(20), char('s')}) ==
              "03  20MT Missiles");
        check(attack_announcement(1, 1, "Ron Raygun", "Infidel") ==
              "Ron Raygun Launches a missile at Infidel.");
        check(action_announcement(3, 1, "Ron Raygun", "Infidel") ==
              "Ron Raygun conducts propoganda towards Infidel.");
        check(action_announcement(2, 4, "Ron Raygun", "") ==
              "Ron Raygun prepares a missile strike.");
        check(action_announcement(6, 2, "Ron Raygun", "") == "Ron Raygun produces weapons.");
        check(lookup(0x113ae5).text == "#    Name   % Left  Cities  Score");
        rejects([] { (void)lookup(0x131048); });
        rejects([] { (void)table("reaction_identity_speech", 29); });
        rejects([] { (void)table("leader_names", 10); });
        rejects([] { (void)format(0x108f38, {std::string("wrong type")}); });
        rejects([] { (void)format(0x108f38, {}); });
        rejects([] { (void)format(0x1092de, {std::int32_t(1)}); });
        rejects([] { (void)attack_announcement(1, 4, "A", "B"); });
        rejects([] { (void)action_announcement(15, 4, "A", "B"); });
        std::cout << "Original text tests: " << checks << " checks passed; "
                  << format_fixtures.size() << " templates compared with independent fixtures\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << "\n";
        return 1;
    }
}
