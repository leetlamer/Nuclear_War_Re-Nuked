#pragma once
#include "ui/player_controller.hpp"
namespace nuclear::player {
struct ExtinctionFrame { bool large; Rect source, destination; double seconds; };
// Native arrays 23c3ca..23c41a: packed, variable-size rectangles.
// One native logical stage is copied to both buffers; never count that as two frames.
inline constexpr std::array<ExtinctionFrame, 10> extinction_frames{{
    {false,{0,0,48,38},{151,43,48,38},.20},
    {false,{48,0,112,64},{113,74,112,64},.20},
    {false,{160,0,80,64},{149,78,80,64},.20},
    {false,{0,64,112,112},{111,42,112,112},.22},
    {false,{112,64,112,112},{103,39,112,112},.22},
    {true,{0,0,160,128},{81,28,160,128},.22},
    {true,{160,0,160,144},{82,16,160,144},.22},
    {true,{0,128,160,160},{81,16,160,160},.22},
    {true,{160,144,160,160},{82,16,160,160},.22},
    {true,{0,288,160,144},{82,16,160,144},.04}
}};
// 50Hz WaitTOF conversion is provisional host timing. Original stage order/rects verified.
inline constexpr double extinction_earth_seconds = .18;
inline double extinction_duration() {
    double total = extinction_earth_seconds;
    for (const auto &frame : extinction_frames) total += frame.seconds;
    return total + .60; // Native clean-space hold before Hall.
}
inline int extinction_stage(double elapsed) {
    elapsed -= extinction_earth_seconds;
    if (elapsed < 0) return -1;
    for (std::size_t i = 0; i < extinction_frames.size(); ++i) {
        if (elapsed < extinction_frames[i].seconds) return int(i);
        elapsed -= extinction_frames[i].seconds;
    }
    return int(extinction_frames.size());
}
} // namespace nuclear::player
