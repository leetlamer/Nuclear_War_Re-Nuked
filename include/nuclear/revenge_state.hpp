#pragma once
#include "nuclear/engine.hpp"
#include <vector>
namespace nuclear::session {
// USER-DEFINED reconstruction, deliberately separate from original Actor encoding.
struct AirborneBomber {
    bool active{};
    Word capacity{}, reserved_payload{};
    bool death_eligible{};
};
struct ResolvedTurnEvent {
    bool death{}, revenge{};
    std::size_t actor{};
    Word action{}, target{}, city{}, magnitude{}, variant{}, effect{}, loss{}, population_before{};
    bool result_recorded{}, population_known{};
    Word result_variant{}, result_amount{};
    std::int32_t population_delta{};
    State before{}, after{};
    std::uint32_t seed_before{}, seed_after{};
};
struct RevengeState {
    std::array<bool, 5> announced{};
    std::array<unsigned, 5> attacks{};
    std::array<AirborneBomber, 5> airborne{};
    std::array<std::vector<AirborneBomber>, 5> additional_airborne{};
    std::vector<ResolvedTurnEvent> events; // current resolver only, immutable after commit
    bool contains_death{};
};
} // namespace nuclear::session
