#pragma once
#include "ui/player_controller.hpp"
#include "core/game_text.hpp"
#include <algorithm>
#include <cmath>
#include <set>
namespace nuclear::player {
struct MediaEvidence {bool supported{};Word action{},variant{},effect{},loss{},target{},city{},magnitude{},population_before{};bool amount_known{true};std::int32_t population_delta{};bool population_known{};Word result_variant{},result_amount{};bool result_recorded{};};
inline MediaEvidence recorded_media(const session::ResolvedTurnEvent& r){MediaEvidence e;e.supported=!r.death;e.action=r.action;e.variant=r.variant;e.effect=r.effect;e.loss=r.loss;e.target=r.target;e.city=r.city;e.magnitude=r.magnitude;e.population_before=r.population_before;e.result_recorded=r.result_recorded;e.result_variant=r.result_variant;e.result_amount=r.result_amount;e.population_known=r.population_known;e.population_delta=r.population_delta;return e;}
inline MediaEvidence committed_transfer_visual(const Actor& a){MediaEvidence e;if(a.action==8&&a.target_actor<5&&a.target_city<5){e.supported=true;e.action=8;e.effect=15;e.target=a.target_actor;e.city=a.target_city;e.amount_known=false;}return e;}
struct MediaReplay {State state;std::uint32_t seed{};std::array<MediaEvidence,5> actions{};std::array<State,5> before{},after{};};
// Read-only diagnostic replay in PRIVATE State/Rng copies. Never publishes writes.
inline MediaReplay replay_media(State state,Word time){Rng rng(time);MediaReplay report;
 for(std::size_t i=0;i<5;++i){auto& a=state.actors[i];auto& e=report.actions[i];report.before[i]=state;report.after[i]=state;
  if(!populated_cities(a)||a.action==0||a.action==88)continue;
  auto start=start_resolver_action(state,i,0,rng);if(!start.initialized)continue;
  e={true,a.action,start.handler.message_variant,start.handler.effect,0,a.target_actor,a.target_city,a.magnitude,0};
  if(e.target<5&&e.city<5)e.population_before=state.actors[e.target].population[e.city];
  resolver::Context ctx{a.target_actor,a.target_city,a.magnitude,0};
  if(a.action==1||a.action==5){resolver::ImpactState local;local.context=ctx;local.selector=e.effect;local.magnitude_was_25=start.prelude.magnitude_was_25;
   auto impact=resolver::primary_impact(state,i,0x5208,local,rng);e.effect=impact.local.selector;e.loss=impact.local.context.magnitude;
   resolver::DamageAcknowledgment ack(i,impact.local);ack.write_at_gate(state,0,true);ack.observe_render_request(state,0);
  }else if(e.effect!=255){auto result=resolver::effect_marker(state,i,e.effect,0x400,0,ctx,rng);e.result_variant=result.message_variant.value_or(0);e.result_amount=result.context.magnitude;e.result_recorded=true;if((e.action==3&&e.effect==0x0d)||(e.action==8&&e.effect==0x0f))e.loss=result.context.magnitude;
   // Resolve presentation coordinates from the actual effect latches, not stale Actor targets.
   e.target=(e.action==15?Word(i):result.context.target_actor);e.city=result.context.target_city;
   if(e.target<5){e.population_delta=0;for(std::size_t c=0;c<5;++c)e.population_delta+=signed_word(state.actors[e.target].population[c])-signed_word(report.before[i].actors[e.target].population[c]);e.population_known=true;if(e.city<5)e.population_before=report.before[i].actors[e.target].population[e.city];}
  }
  report.after[i]=state;
 }
 report.state=state;report.seed=rng.seed();return report;
}
inline bool same_state(const State& a,const State& b){for(std::size_t i=0;i<5;++i)if(encode_actor(a.actors[i])!=encode_actor(b.actors[i]))return false;
 return a.prepared_capacity==b.prepared_capacity&&a.selected_opponent==b.selected_opponent&&a.identity==b.identity&&a.remaining_total==b.remaining_total&&a.pass_processed==b.pass_processed&&a.human_prepared_capacity==b.human_prepared_capacity;
}
inline std::string original_caption(const MediaEvidence& e,const std::string& actor,const std::string& target){
 if(!e.supported)return actor+": action details unavailable.";
 try{return nuclear::text::action_announcement(e.action,e.variant,actor,target);}catch(const std::exception&){return actor+": original message unavailable.";}
}
inline std::string combat_caption(const MediaEvidence& e){
 const std::string fallback="Population loss: "+std::to_string(e.loss)+" million.";
 if(!e.supported||(e.action!=1&&e.action!=5))return "Outcome unavailable.";
 if(e.effect==13)return std::string(nuclear::text::lookup(e.action==1?0x10598a:0x10596a).text);
 // Caption selection is bounded to nonoverflowing signed-word contexts.
 if(signed_word(e.population_before)<0||e.population_before>10922||e.magnitude>25)return fallback;
 const bool destroyed=e.loss==e.population_before;std::uint32_t address=0;
 switch(e.effect){case 2:address=e.action==1?0x1056bd:0x105697;break;case 3:address=0x1056e8;break;case 4:address=e.loss==0?0x105761:destroyed?0x105732:0x105704;break;
 case 5:address=destroyed?0x1057a8:0x10577e;break;case 6:case 7:address=destroyed?0x1057f2:0x1057ce;break;case 8:address=destroyed?0x105838:0x105812;break;
 case 9:address=destroyed?0x105882:0x10585a;break;case 10:address=destroyed?0x1058c9:0x1058a5;break;case 11:address=destroyed?0x105906:0x1058e9;break;case 12:address=destroyed?0x105946:0x10591f;break;default:return fallback;}
 auto text=nuclear::text::lookup(address).text;
 return text.find("%d")!=std::string_view::npos?nuclear::text::format(address,{std::int32_t(signed_word(e.loss))}):std::string(text);
}
enum class ImpactVisual {none,interception,explosion};
inline ImpactVisual impact_visual(const MediaEvidence& e){if(!e.supported||(e.action!=1&&e.action!=5))return ImpactVisual::none;if(e.effect==13)return ImpactVisual::interception;return e.loss>0?ImpactVisual::explosion:ImpactVisual::none;}
struct VisualPose {double x{},y{},scale{},angle{};};
inline VisualPose flight_pose(double t,double sx,double sy,double tx,double ty){t=std::clamp(t,0.0,1.0);double apex=std::min(sy,ty)-12,dy,y;
 if(t<=.5){double u=2*t;y=apex+(sy-apex)*(1-u)*(1-u);dy=-4*(sy-apex)*(1-u);}else{double u=2*t-1;y=apex+(ty-apex)*u*u;dy=4*(ty-apex)*u;}
 return {sx+(tx-sx)*t,y,.4+1.2*std::sin(3.141592653589793*t),std::atan2(tx-sx,-dy)*180/3.141592653589793};
}
inline std::string payload_caption(const MediaEvidence& e){if(!e.supported)return {};if(e.action==1||e.action==5){for(Word i=0;i<4;++i)if(e.magnitude==class_effect(i))return nuclear::text::format(0x10951f,{std::int32_t(class_capacity(i))});return "Payload class unavailable.";}if(e.action==7)return e.magnitude==0?"L.N.D.S.":e.magnitude==1?"MegaCannon":"Defense type unavailable.";return {};}
struct SoundCue {const char* sample;unsigned period,length,volume;unsigned repeats{1};double playback_rate{1.};bool smooth_edges{};};
inline std::optional<SoundCue> sound_cue(const MediaEvidence& e){if(!e.supported)return std::nullopt;
 switch(e.action){case 1:return SoundCue{"exp",200,4998,18};case 2:return SoundCue{"missile",715,6467,12};case 3:return SoundCue{"prop",999,5399,20};case 4:return SoundCue{"engine",600,7818,14};case 5:return SoundCue{"engine",420,7818,16};case 9:return SoundCue{"ooo",124,8820,24};case 11:return SoundCue{"engine",600,7818,14};case 12:return SoundCue{"16ton",715,5405,64};case 15:return SoundCue{"ooo",1299,8820,24};case 16:return SoundCue{"prop",300,5399,20};case 13:return SoundCue{"moo",715,11861,30};case 7:return SoundCue{"door.snd",124,8820,12};case 14:return SoundCue{"ooo",300,8820,24};case 8:return SoundCue{"feet",700,5060,9};case 86:return SoundCue{"exp",1300,4998,12,2};case 6:return SoundCue{"build",130,20031,22,2};default:return std::nullopt;}}
inline std::optional<SoundCue> impact_sound(const MediaEvidence& e){if(impact_visual(e)!=ImpactVisual::explosion)return std::nullopt;return SoundCue{"exp",650,4998,std::min(64u,12u+unsigned(e.loss))};}
class SoundGate {std::set<std::uint64_t> seen_;public:bool claim(std::uint64_t token){return seen_.insert(token).second;}};
}
