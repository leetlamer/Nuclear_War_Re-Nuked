#pragma once
#include "ui/player_controller.hpp"
namespace nuclear::player {
// PRIVATE deterministic fixtures, never used by normal setup or event generation.
inline session::Data endgame_fixture(bool human_alive=false,unsigned live_ai=3){
 session::Data d(1,true);for(auto& a:d.state.actors)a.profile=1;initialize_state(d.state,0,d.rng);
 for(std::size_t i=0;i<5;++i){auto& a=d.state.actors[i];a.population={};a.action=0;a.missiles={};a.warheads={};a.bombers={};a.defenses={};d.names[i]=i==4?"Your Country":player_assets::names[i];if(i<4)d.state.identity[i]=Word(i);
  if(i<live_ai){a.population[0]=5;a.missiles={10,10,10,10};a.warheads={10,10,10,10};a.action=2;d.state.prepared_capacity[i]=100;}a.tally=i<live_ai?5:0;d.state.remaining_total[i]=a.tally;
 }
 d.state.actors[4].profile=0;
 if(human_alive){auto& h=d.state.actors[4];h.population[0]=1;h.tally=1;d.state.remaining_total[4]=1;h.action=6;for(unsigned i=0;i<live_ai;++i)d.state.actors[i].scores[4]=30000;}
 return d;
}
// User-defined revenge fixture: death is caused by normal committed combat.
inline session::Data revenge_fixture(bool human,bool airborne=false){
 auto d=endgame_fixture(true,4);d.rng.reseed_word(1);std::size_t victim=human?4:1;
 for(std::size_t i=0;i<5;++i){auto& a=d.state.actors[i];a.action=88;a.population.fill(1000);a.missiles.fill(0);a.warheads.fill(0);a.bombers.fill(0);a.defenses.fill(0);d.state.prepared_capacity[i]=0;}
 auto& attacker=d.state.actors[0];attacker.action=1;attacker.target_actor=Word(victim);attacker.target_city=0;attacker.magnitude=25;
 auto& dead=d.state.actors[victim];dead.population.fill(0);dead.population[0]=1;dead.missiles[0]=airborne?0:10;dead.warheads[0]=10;
 if(airborne){dead.action=5;dead.magnitude=5;d.state.prepared_capacity[victim]=80;if(victim==4)d.state.human_prepared_capacity=80;}
 TurnResolverAdapter resolver;StaticSchedule callbacks;session::ResolverRequest request;request.microseconds_low16=1;resolver.resolve(d,request,callbacks);return d;
}
}
