#pragma once
#include "nuclear/engine.hpp"
namespace nuclear {
struct RoundPlanResult { Word human_control{}; bool should_resolve{}; };
// Main100724..100734 only. Caller has already handled human input when the
// human has positive cities (10070a..100720); otherwise pass the carried
// control word unchanged. This helper does not synthesize human commands.
// AI planning and attitudes still run for nonzero human control. Unsupported
// leaves both state and RNG unchanged; no action resolution is performed.
RoundPlanResult prepare_round_plan(State&, Word human_control, Rng&);
}
