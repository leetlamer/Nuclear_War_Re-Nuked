#include "nuclear/ending.hpp"
#include <cstdlib>
#include <iostream>
using namespace nuclear;
namespace {
unsigned checks = 0;
void check(bool b, int line) {
    ++checks;
    if (!b) {
        std::cerr << "Failure line " << line << '\n';
        std::exit(1);
    }
}
#define CHECK(x) check(bool(x), __LINE__)
bool same(const HighScoreNumbers &a, const HighScoreNumbers &b) {
    return a.score == b.score && a.population == b.population && a.cities == b.cities;
}
} // namespace
int main() {
    for (unsigned raw = 0; raw < 65536; ++raw)
        CHECK(ending_requests_exit(Word(raw)) == (raw == 2));
    std::cout << "PASS ending exact result control\n";
    for (Word last : {Word(0), Word(0x7fff), Word(0x8000), Word(0xffff), Word(10)}) {
        HighScoreTable t{};
        t[9].score = last;
        for (unsigned raw = 0; raw < 65536; ++raw)
            CHECK(high_score_qualifies(Word(raw), t) ==
                  (signed_word(Word(raw)) > signed_word(last)));
    }
    std::cout << "PASS qualification exhaustive signed boundaries\n";
    HighScoreTable original{};
    for (unsigned i = 0; i < 10; ++i)
        original[i] = {Word(100 - 10 * i), Word(0x8000 + i), Word(0xfff0 + i)};
    for (unsigned raw = 0; raw < 65536; ++raw) {
        auto t = original;
        const HighScoreNumbers candidate{Word(raw), 0xffff, 0x8000};
        const auto rank = insert_high_score_numbers(t, candidate);
        if (signed_word(Word(raw)) <= 10) {
            CHECK(!rank);
            for (unsigned i = 0; i < 10; ++i)
                CHECK(same(t[i], original[i]));
        } else {
            unsigned expected = 0;
            while (expected < 9 && signed_word(Word(raw)) <= signed_word(original[expected].score))
                ++expected;
            CHECK(rank && *rank == expected);
            for (unsigned i = 0; i < 10; ++i)
                CHECK(same(t[i], i < expected    ? original[i]
                                 : i == expected ? candidate
                                                 : original[i - 1]));
        }
    }
    std::cout << "PASS insertion exhaustive scores ties shift and rejection\n";
    // Original caller checks row9 only; do not silently sort odd tables.
    HighScoreTable odd{};
    for (auto &row : odd)
        row = {0x7fff, 1, 2};
    odd[9].score = 0x8000;
    auto rank = insert_high_score_numbers(odd, {0xffff, 0x8000, 0xffff});
    CHECK(rank && *rank == 9);
    CHECK(odd[9].score == 0xffff);
    CHECK(odd[9].population == 0x8000);
    CHECK(odd[9].cities == 0xffff);
    std::cout << "PASS original caller guard and raw field widths\n";
    std::cout << "4 groups, " << checks << " checks passed\n";
}
