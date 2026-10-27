#include "nuclear/ending.hpp"

namespace nuclear {
bool ending_requests_exit(Word result) noexcept { return result == 2; }
bool high_score_qualifies(Word score, const HighScoreTable &table) noexcept {
    return signed_word(score) > signed_word(table[9].score);
}
std::optional<std::size_t> insert_high_score_numbers(HighScoreTable &table,
                                                     HighScoreNumbers candidate) noexcept {
    if (!high_score_qualifies(candidate.score, table))
        return std::nullopt;
    std::size_t rank = 0;
    while (rank < 9 && signed_word(candidate.score) <= signed_word(table[rank].score))
        ++rank;
    for (std::size_t row = 9; row > rank; --row)
        table[row] = table[row - 1];
    table[rank] = candidate;
    return rank;
}
} // namespace nuclear
