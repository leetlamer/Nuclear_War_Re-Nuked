#pragma once
#include "nuclear/single_missile.hpp"
#include <deque>
namespace nuclear::player {
// Shared normal combat path. Caller has already committed the payload/roll.
inline void resolve_attack(session::Data& d,session::ResolvedTurnEvent& e,bool magnitude25){
 resolver::ImpactState local;local.context={e.target,e.city,e.magnitude,0};local.selector=e.effect;local.magnitude_was_25=magnitude25;
 auto impact=resolver::primary_impact(d.state,e.actor,0x5208,local,d.rng);
 e.effect=impact.local.selector;
 resolver::DamageAcknowledgment ack(e.actor,impact.local);auto write=ack.write_at_gate(d.state,0,true);ack.observe_render_request(d.state,0);e.loss=write.loss;
}
inline Word bomber_capacity_left(const State& s,std::size_t i){return i==4?s.human_prepared_capacity:s.prepared_capacity[i];}
struct RevengeWeapon {Word action{},delivery{},head{},payload{},capacity_cost{};bool reserved{};std::size_t flight{};};
inline std::optional<RevengeWeapon> revenge_weapon(const session::Data& d,std::size_t i){
 const auto& a=d.state.actors[i];
 // Only aircraft captured at death, still active, and not already delivered.
 for(std::size_t slot=0;slot<=d.revenge.additional_airborne[i].size();++slot){
  const auto& b=slot?d.revenge.additional_airborne[i][slot-1]:d.revenge.airborne[i];
  if(!b.active||!b.death_eligible)continue;
  if(b.reserved_payload)return RevengeWeapon{5,0,0,b.reserved_payload,0,true,slot};
  for(Word h=0;h<4;++h)if(a.warheads[h]&&signed_word(b.capacity)>=int(class_capacity(h)))return RevengeWeapon{5,0,h,class_effect(h),class_capacity(h),false,slot};
 }
 // Deterministic ascending missile class, then ascending compatible warhead.
 for(Word m=0;m<4;++m)if(a.missiles[m])for(Word h=0;h<=m;++h)if(a.warheads[h])return RevengeWeapon{1,m,h,class_effect(h),0,false,0};
 return std::nullopt;
}
// FIFO, bounded to5deaths/20attempts. No presentation callback executes this.
class RevengeQueue {
 session::Data& d_;std::deque<std::size_t> pending_;
public:
 explicit RevengeQueue(session::Data& d):d_(d){}
 void detect(const State& before){for(std::size_t i=0;i<5;++i)if(populated_cities(before.actors[i])&&!populated_cities(d_.state.actors[i])&&!d_.revenge.announced[i]){
  d_.revenge.airborne[i].death_eligible=d_.revenge.airborne[i].active;
  for(auto& b:d_.revenge.additional_airborne[i])b.death_eligible=b.active;
  d_.revenge.announced[i]=true;d_.revenge.contains_death=true;pending_.push_back(i);
  session::ResolvedTurnEvent notice;notice.death=true;notice.actor=i;notice.before=notice.after=d_.state;notice.seed_before=notice.seed_after=d_.rng.seed();d_.revenge.events.push_back(std::move(notice));
 }}
 void drain(){while(!pending_.empty()){
  auto i=pending_.front();pending_.pop_front();std::array<bool,5> targeted{};
  while(d_.revenge.attacks[i]<4){
   auto weapon=revenge_weapon(d_,i);if(!weapon)break;
   std::vector<Word> players;for(Word t=0;t<5;++t)if(t!=i&&populated_cities(d_.state.actors[t])&&!targeted[t])players.push_back(t);
   if(players.empty()){targeted.fill(false);for(Word t=0;t<5;++t)if(t!=i&&populated_cities(d_.state.actors[t]))players.push_back(t);}
   if(players.empty())break;
   session::ResolvedTurnEvent e;e.revenge=true;e.actor=i;e.action=weapon->action;e.magnitude=weapon->payload;e.before=d_.state;e.seed_before=d_.rng.seed();
   e.target=players[d_.rng.next()%players.size()];std::vector<Word> cities;for(Word c=0;c<5;++c)if(signed_word(d_.state.actors[e.target].population[c])>0)cities.push_back(c);
   e.city=cities[d_.rng.next()%cities.size()];targeted[e.target]=true;e.population_before=d_.state.actors[e.target].population[e.city];
   auto& a=d_.state.actors[i];auto& b=weapon->flight?d_.revenge.additional_airborne[i][weapon->flight-1]:d_.revenge.airborne[i];
   if(e.action==1){--a.missiles[weapon->delivery];--a.warheads[weapon->head];}
   else {if(weapon->reserved)b.reserved_payload=0;else {--a.warheads[weapon->head];b.capacity=sub_word(b.capacity,weapon->capacity_cost);}d_.state.prepared_capacity[i]=b.capacity;}
   a.action=e.action;a.target_actor=e.target;a.target_city=e.city;a.magnitude=e.magnitude;
   ++d_.revenge.attacks[i];auto roll=begin_attack_roll(e.action,d_.rng);e.variant=roll.message_variant;e.effect=roll.effect;
   resolve_attack(d_,e,e.magnitude==25);
   if(e.action==5){b.active=false;b.death_eligible=false;b.capacity=d_.state.prepared_capacity[i];if(i==4)d_.state.human_prepared_capacity=b.capacity;}
   e.after=d_.state;e.seed_after=d_.rng.seed();d_.revenge.events.push_back(e);detect(e.before);
  }
 }}
};
}
