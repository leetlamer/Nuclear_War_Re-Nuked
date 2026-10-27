#pragma once
#include "ui/interface_layout.hpp"
#include "ui/console_display.hpp"
namespace nuclear::player {
// Host durations only; native callback cadence is deliberately not modeled.
inline constexpr double door_seconds=.32;
enum class DoorPhase{none,closing,opening};
inline double door_fraction(DoorPhase phase,double seconds){double t=std::clamp(seconds/door_seconds,0.,1.);return phase==DoorPhase::closing?t:phase==DoorPhase::opening?1-t:0;}
inline Rect typewriter_head(const std::string& visible,bool){auto newline=visible.rfind('\n');std::size_t col=newline==std::string::npos?visible.size():visible.size()-newline-1;return {78+int(std::min<std::size_t>(col,27))*6,174,16,16};}

inline Rect typewriter_foreground(){return {72,184,176,10};}
inline int console_first_y(const std::string& text){return 174-8*(int(std::min<std::size_t>(3,wrap_text(text,27).size()))-1);}
struct ConsoleTypingFrame {std::string text;Rect head;int first_y;bool typing;};
inline ConsoleTypingFrame console_typing_frame(const std::string& text,bool typing){return {text,typewriter_head(text,typing),console_first_y(text),typing};}
inline SoundCue production_100mt_sound(){return {"ooo",715,8820,24,1,1.08,true};}
inline SoundCue door_sound(){return {"door.snd",415,8820,12};}
inline bool action_sound_ready(const MediaEvidence& e,double seconds){return e.action==12?seconds>=2.0:e.action!=14||seconds>=.7;}
}
