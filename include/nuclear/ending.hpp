#pragma once
#include "nuclear/engine.hpp"
#include <optional>

namespace nuclear {
// 10087c..100884: every result except exactly 2 returns to setup at100658.
// This selects flow only; setup/reinitialization and OS cleanup are external.
bool ending_requests_exit(Word ending_result) noexcept;

// Numeric portion of one high-score row. Original names are separate pointers
// copied by a string routine; name entry/persistence are not implemented here.
// Caller must mirror the returned rank/shift in its own name representation.
struct HighScoreNumbers {
    Word score{}, population{}, cities{};
};
using HighScoreTable = std::array<HighScoreNumbers, 10>;
// 11496c..114986: strict SIGNED word comparison with last row.
bool high_score_qualifies(Word score, const HighScoreTable &) noexcept;
// 1141b4..1142ce, with required caller guard11496c. Equality is ranked
// after existing equal scores. No sorting or validation of original table.
// Rejected candidates leave all rows unchanged.
std::optional<std::size_t> insert_high_score_numbers(HighScoreTable &,
                                                     HighScoreNumbers candidate) noexcept;
} // namespace nuclear
