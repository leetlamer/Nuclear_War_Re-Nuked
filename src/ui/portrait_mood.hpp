#pragma once
#include "ui/interface_layout.hpp"
namespace nuclear::player {
inline const char* expression_name(int bucket){constexpr const char* names[]{"very happy","happy","neutral","angry","very angry"};return bucket>=-2&&bucket<=2?names[bucket+2]:"unsupported";}
 enum class FaceState {eliminated,neutral_static,mood_envelope};
struct MoodTiming {double endpoint_hold{1.0},frame_seconds{.16};};
struct MoodPose {
 std::size_t actor{};Word identity{},profile{},score{};int raw_bucket{},display_bucket{},base_frame{2},frame{2};
 FaceState animation{FaceState::eliminated};bool speaking{};
 Rect source{};std::optional<Word> indicator;
};
inline std::optional<Word> mood_indicator(const State& state,std::size_t actor){
 if(actor>=4||!populated_cities(state.actors[actor]))return std::nullopt;
 int declaration=signed_byte(state.actors[4].classifications[actor]);if(declaration< -2||declaration>2)return std::nullopt;
 return Word(235+declaration); // All proven native callers request human context4.
}
inline int mood_frame(int bucket,double elapsed,MoodTiming timing={}){
 if(bucket< -2||bucket>2||!std::isfinite(elapsed)||!std::isfinite(timing.endpoint_hold)||!std::isfinite(timing.frame_seconds)||timing.endpoint_hold<0||timing.frame_seconds<=0)throw Unsupported("invalid mood presentation input");
 int base=2+bucket,span=std::abs(bucket);if(!span)return 2;
 double cycle=timing.endpoint_hold+2*span*timing.frame_seconds;
 double t=std::fmod(std::max(0.,elapsed),cycle);if(t<timing.endpoint_hold)return base;
 int step=std::min(2*span,1+int((t-timing.endpoint_hold)/timing.frame_seconds)),direction=bucket<0?1:-1;
 return step<=span?base+direction*step:2-direction*(step-span);
}
inline MoodPose portrait_mood(const State& state,std::size_t actor,double elapsed,bool speaking=false,MoodTiming timing={}){
 if(actor>=4)throw Unsupported("portrait actor outside AI slots0..3");
 const auto& a=state.actors[actor];MoodPose p;p.actor=actor;p.identity=state.identity[actor];p.profile=a.profile;p.score=a.scores[4];p.speaking=speaking;p.indicator=mood_indicator(state,actor);
 p.raw_bucket=signed_word(relationship_bucket(p.score));p.display_bucket=p.profile==4?-p.raw_bucket:p.raw_bucket;p.base_frame=2+p.display_bucket;
 if(!populated_cities(a))return p;
 if(p.identity>=10)throw Unsupported("portrait identity outside original ten");
 p.animation=p.display_bucket==0?FaceState::neutral_static:FaceState::mood_envelope;p.frame=mood_frame(p.display_bucket,elapsed,timing);
 p.source={int(p.identity%2)*160+p.frame*32,int(p.identity/2)*32,32,32};return p;
}
// Owns only elapsed presentation time. State remains authoritative and read-only.
class MoodClocks {
 struct Clock {int bucket{99};Word identity{0xffff},profile{0xffff};bool alive{};double start{};};std::array<Clock,4> clocks_{};
public:
 MoodPose pose(const State& state,std::size_t actor,double now,bool speaking=false){auto& c=clocks_.at(actor);auto p=portrait_mood(state,actor,0,speaking);bool alive=p.animation!=FaceState::eliminated;
  if(c.bucket!=p.display_bucket||c.identity!=p.identity||c.profile!=p.profile||c.alive!=alive){c={p.display_bucket,p.identity,p.profile,alive,now};}
  return portrait_mood(state,actor,std::max(0.,now-c.start),speaking);
 }
};
}
