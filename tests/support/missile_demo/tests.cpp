#include "tests/support/missile_demo/model.hpp"
#include <cmath>
#include <cstdlib>
#include <iostream>
using namespace nuclear;
using namespace nuclear::missile_prototype;
unsigned checks = 0, groups = 0;
void check(bool b, int line) {
    ++checks;
    if (!b) {
        std::cerr << "failed " << line << '\n';
        std::exit(1);
    }
}
#define CHECK(x) check(bool(x), __LINE__)
template <class F> void group(const char *n, F f) {
    f();
    ++groups;
    std::cout << "PASS " << n << '\n';
}
int main() {
    group("preparation and launch separated by turn advance; consume once", [] {
        Demo d;
        CHECK(!d.launch());
        CHECK(d.prepare());
        CHECK(d.turn() == 1 && d.phase() == Phase::prepared);
        CHECK(d.state().actors[4].missiles[1] == 1 && d.state().actors[4].warheads[1] == 1);
        CHECK(d.seed() == 1);
        CHECK(!d.launch() && !d.prepare());
        CHECK(d.advance_turn() && d.turn() == 2 && d.seed() == 1);
        CHECK(!d.advance_turn());
        CHECK(d.launch() && !d.launch());
        CHECK(d.seed() == 0x2781e494 && d.state().actors[4].missiles[1] == 1 &&
              d.state().actors[4].warheads[1] == 1);
    });
    group("invalid capacity and frozen selection", [] {
        Demo d;
        d.choose_missile(0);
        d.choose_warhead(3);
        CHECK(!d.prepare() && d.seed() == 1 && d.state().actors[4].missiles[0] == 2);
        CHECK(d.choose_warhead(0) && d.prepare());
        CHECK(!d.choose_city(4) && !d.choose_defense(0) && !d.choose_missile(2) &&
              !d.choose_warhead(2));
    });
    group("all20 target/warhead selection combinations and valid target positions", [] {
        for (Word city = 0; city < 5; ++city)
            for (Word w = 0; w < 4; ++w) {
                Demo d;
                CHECK(d.choose_city(city) && d.choose_warhead(w) && d.choose_missile(w) &&
                      d.prepare() && d.advance_turn() && d.launch());
                auto &p = *d.prefix();
                CHECK(p.target_x == add_word(d.state().actors[0].city_x[city], 8) &&
                      p.target_y == sub_word(d.state().actors[0].city_y[city], 16));
                CHECK(p.launch_x == 143 && p.launch_y == 101);
            }
    });
    group("visual pose grows rotates shrinks and reaches exact coordinates without RNG", [] {
        Demo d;
        auto seed = d.seed();
        auto start = visual_pose(0, 143, 101, 133, 6), peak = visual_pose(.5, 143, 101, 133, 6),
             end = visual_pose(1, 143, 101, 133, 6);
        CHECK(start.x == 143 && start.y == 101 && end.x == 133 && end.y == 6);
        CHECK(peak.scale > start.scale && peak.scale > end.scale);
        CHECK(std::abs(start.angle - end.angle) > 90);
        for (unsigned i = 0; i < 10000; ++i) {
            auto p = visual_pose(i / 9999., 143, 101, 133, 6);
            CHECK(std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.angle));
        }
        CHECK(d.seed() == seed);
    });
    group("flight cannot draw RNG or commit early; impact consumes one draw", [] {
        Demo d;
        d.prepare();
        d.advance_turn();
        d.launch();
        auto seed = d.seed();
        for (int i = 0; i < 39; ++i)
            d.tick(.1);
        CHECK(d.phase() == Phase::flight && d.seed() == seed && d.impacts() == 0 &&
              d.state().actors[0].population[2] == 30);
        d.tick(.2);
        CHECK(d.phase() == Phase::effect && d.impacts() == 1 && d.seed() == 0xc46b9b3d &&
              d.state().actors[0].population[2] == 30 && d.loss() == 9);
    });
    group("damage once then acknowledgment once; no duplicated relationship", [] {
        Demo d;
        d.prepare();
        d.advance_turn();
        d.launch();
        d.tick(4);
        auto before = d.state().actors[0].scores;
        d.tick(1.2);
        CHECK(d.phase() == Phase::awaiting_ack && d.commits() == 1 && d.acknowledgments() == 0 &&
              d.state().actors[0].population[2] == 21);
        CHECK(d.state().actors[0].scores == before);
        d.after_present();
        CHECK(d.phase() == Phase::complete && d.acknowledgments() == 1);
        auto after = d.state().actors[0].scores;
        auto seed = d.seed();
        for (int i = 0; i < 20; ++i) {
            d.tick(100);
            d.after_present();
        }
        CHECK(d.impacts() == 1 && d.commits() == 1 && d.acknowledgments() == 1 && d.seed() == seed);
        CHECK(d.state().actors[0].scores == after && d.state().actors[0].population[2] == 21);
    });
    group("verified defense consumption and interception zero loss", [] {
        for (Word defense : {Word(0), Word(1)}) {
            Demo d;
            d.choose_defense(defense);
            d.prepare();
            CHECK(d.state().actors[0].defenses[defense] == 0 && d.state().actors[0].action == 7);
            d.advance_turn();
            d.launch();
            d.tick(4);
            CHECK(d.intercepted() && d.loss() == 0 && d.seed() == 0xc46b9b3d);
            d.tick(1.2);
            d.after_present();
            CHECK(d.state().actors[0].population[2] == 30 && d.commits() == 1 &&
                  d.acknowledgments() == 1);
        }
    });
    group("timelines and update partitioning cannot alter gameplay RNG/results", [] {
        for (auto time : {.01, .2, 2., 10.}) {
            Demo d(1, {time, time / 2});
            d.prepare();
            d.advance_turn();
            d.launch();
            for (unsigned i = 0; i < 1000 && d.phase() != Phase::complete; ++i) {
                d.tick(time / 20);
                d.after_present();
            }
            CHECK(d.phase() == Phase::complete && d.seed() == 0xc46b9b3d && d.loss() == 9 &&
                  d.state().actors[0].population[2] == 21);
        }
    });
    group("multiple seeds and valid inventories finish without duplicate impact", [] {
        for (std::uint32_t seed = 0; seed < 100; ++seed)
            for (Word defense : {Word(0), Word(1), Word(2)}) {
                Demo d(seed, {.01, .01});
                d.choose_defense(defense);
                CHECK(d.prepare() && d.advance_turn() && d.launch());
                d.tick(100);
                d.tick(100);
                d.after_present();
                CHECK(d.phase() == Phase::complete && d.impacts() == 1 && d.commits() == 1 &&
                      d.acknowledgments() == 1);
                CHECK(d.state().actors[4].missiles[1] == 1 && d.state().actors[4].warheads[1] == 1);
                if (defense < 2)
                    CHECK(d.loss() == 0 && d.state().actors[0].population[2] == 30);
            }
    });
    std::cout << groups << " groups, " << checks << " checks passed\n";
}
