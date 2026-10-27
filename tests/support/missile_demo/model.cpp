#include "tests/support/missile_demo/model.hpp"
#include <algorithm>
#include <cmath>
namespace nuclear::missile_prototype {
const char* phase_name(Phase p){switch(p){case Phase::selection:return "SELECT";case Phase::prepared:return "PREPARED - TURN 1";case Phase::launch_ready:return "READY - TURN 2";case Phase::flight:return "FLIGHT";case Phase::effect:return "IMPACT EFFECT";case Phase::awaiting_ack:return "DISPLAY ACK";case Phase::complete:return "COMPLETE";}return "INVALID";}
Pose visual_pose(double t,double sx,double sy,double tx,double ty){
 t=std::clamp(t,0.0,1.0);const double apex=std::min(sy,ty)-12;
 const double x=sx+(tx-sx)*t;
 double y,dy;
 if(t<=0.5){const double u=2*t;y=apex+(sy-apex)*(1-u)*(1-u);dy=-4*(sy-apex)*(1-u);}
 else{const double u=2*t-1;y=apex+(ty-apex)*u*u;dy=4*(ty-apex)*u;}
 return {x,y,0.4+1.2*std::sin(3.141592653589793*t),std::atan2(tx-sx,-dy)*180/3.141592653589793};
}
Demo::Demo(std::uint32_t seed,Timeline timeline):rng_(seed),timeline_(timeline){
 if(!std::isfinite(timeline.flight_seconds)||!std::isfinite(timeline.effect_seconds)||timeline.flight_seconds<=0||timeline.effect_seconds<=0)throw Unsupported("timeline durations must be finite and positive");
 // PROTOTYPE_ASSUMPTION: isolated human4 vs target0 scenario, fixed populations/
 // inventories. Not original setup, opponent selection, AI planning or full round.
 for(auto& a:state_.actors)a.profile=1;
 state_.actors[4].population={30,20,20,15,10};state_.actors[0].population={20,25,30,15,10};
 state_.actors[0].city_x={26,80,125,20,82};state_.actors[0].city_y={36,20,22,64,77};
 state_.actors[4].missiles={2,2,2,2};state_.actors[4].warheads={2,2,2,2};
 message_="Choose missile, warhead and target. P confirms turn 1.";
}
bool Demo::choose_missile(Word v){if(phase_!=Phase::selection||v>=4)return false;missile_=v;return true;}
bool Demo::choose_warhead(Word v){if(phase_!=Phase::selection||v>=4)return false;warhead_=v;return true;}
bool Demo::choose_city(Word v){if(phase_!=Phase::selection||v>=5)return false;city_=v;return true;}
bool Demo::choose_defense(Word v){if(phase_!=Phase::selection||v>2)return false;defense_=v;return true;}
bool Demo::prepare(){
 if(phase_!=Phase::selection){message_="Preparation is already committed.";return false;}
 const auto& a=state_.actors[4];
 if(class_capacity(warhead_)>class_capacity(missile_)){message_="Warhead exceeds missile capacity. Choose a smaller warhead.";return false;}
 if(a.missiles[missile_]==0||a.warheads[warhead_]==0||signed_word(state_.actors[0].population[city_])<=0){message_="Missile, warhead or populated target unavailable.";return false;}
 auto staged=state_;staged.actors[4].target_actor=0;staged.actors[4].target_city=city_;
 human_commit(staged,2,missile_,class_capacity(missile_));
 HumanSelection choice;choice.selected_class=warhead_;choice.selected_capacity=class_capacity(warhead_);
 finish_human_attack_fields(staged,choice);human_commit(staged,choice.pending_action,warhead_,choice.selected_capacity);
 // Scenario defense choice, then VERIFIED planner consumes its inventory once.
 if(defense_<2){staged.actors[0].defenses[defense_]=1;staged.selected_opponent[0]=4;if(!plan_defense(staged,0))throw Unsupported("fixture defense commitment failed");}
 state_=staged;phase_=Phase::prepared;message_="Turn 1 committed. Inventory consumed. N advances to turn 2.";return true;
}
bool Demo::advance_turn(){if(phase_!=Phase::prepared){message_="Prepare first. Turn advances once.";return false;}phase_=Phase::launch_ready;message_="Turn 2. Prepared missile awaits L to launch.";return true;}
bool Demo::launch(){
 if(phase_!=Phase::launch_ready){message_="Launch requires advancing to turn 2.";return false;}
 prefix_=resolver::begin_committed_missile(state_,4,rng_);phase_=Phase::flight;elapsed_=0;
 message_="Prototype visual flight. No native timing or trajectory is claimed.";return true;
}
double Demo::progress()const{return std::clamp(elapsed_/timeline_.flight_seconds,0.0,1.0);}
double Demo::effect_progress()const{return std::clamp(elapsed_/timeline_.effect_seconds,0.0,1.0);}
void Demo::tick(double seconds){
 if(!std::isfinite(seconds)||seconds<0)throw Unsupported("invalid visual delta time");
 if(phase_!=Phase::flight&&phase_!=Phase::effect)return;
 elapsed_+=seconds;
 if(phase_==Phase::flight&&elapsed_>=timeline_.flight_seconds){
  // PROTOTYPE_ASSUMPTION: visual endpoint supplies reached primary token5208.
  // We never call/alter advance_missile or claim to complete original FFP.
  resolver::ImpactState local;local.selector=prefix_->start.handler.effect;
  local.context={state_.actors[4].target_actor,state_.actors[4].target_city,state_.actors[4].magnitude,0};
  local.magnitude_was_25=prefix_->start.prelude.magnitude_was_25;
  auto r=resolver::primary_impact(state_,4,0x5208,local,rng_);
  damage_=std::make_unique<resolver::DamageAcknowledgment>(4,r.local);
  loss_=r.local.context.magnitude;intercepted_=r.local.selector==13;++impacts_;
  phase_=Phase::effect;elapsed_=0;message_=intercepted_?"INTERCEPTED. Zero loss; damage still awaits display sequence.":"Impact. Damage remains pending until effect completes.";
 }else if(phase_==Phase::effect&&elapsed_>=timeline_.effect_seconds){
  // PROTOTYPE_ASSUMPTION: effect completion supplies final commit gate.
  auto write=damage_->write_at_gate(state_,0,true);if(!write.written)throw Unsupported("missing prototype pending damage");
  ++commits_;phase_=Phase::awaiting_ack;message_="Population committed once. Awaiting prototype display acknowledgment.";
 }
}
void Demo::after_present(){if(phase_!=Phase::awaiting_ack)return;
 // Explicitly synthetic observation: completed SDL present stands for request0.
 if(damage_->observe_render_request(state_,0)){++acknowledgments_;phase_=Phase::complete;message_="Sequence complete. R resets this isolated demonstration.";}
}
}
