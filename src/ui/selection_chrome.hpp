#pragma once
#include "rendering/action_media.hpp"
#include <cmath>
namespace nuclear::player {
inline Rect selection_face_rect(Word id){auto r=portrait_rect(id);return {r.x+13,r.y+5,32,32};}
// Presentation-only selection showcase; host timing, never native callback/RNG.
inline Word smiling_selection_frame(Word identity,double seconds){
 constexpr Word frames[]{2,1,0,0,1,2};
 return frames[std::size_t(std::max(0.,seconds)*6+identity)%6];
}
inline bool missile_exhaust(double normalized_time,double cutoff=.46){return normalized_time>=0&&normalized_time<cutoff;}
inline bool defence_animation_active(const MediaEvidence& e,double t){return e.supported&&((e.action==7&&t>=0&&t<2)||((e.action==1||e.action==5)&&e.effect==13&&t>=2.8&&t<4));}
}
