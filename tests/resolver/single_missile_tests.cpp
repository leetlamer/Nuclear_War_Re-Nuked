#include "nuclear/single_missile.hpp"
#include <cstdlib>
#include <functional>
#include <iostream>
using namespace nuclear;
using namespace nuclear::resolver;
namespace {
unsigned checks=0,groups=0;
void check(bool v,int line){++checks;if(!v){std::cerr<<"failure line "<<line<<'\n';std::exit(1);}}
#define CHECK(x) check(bool(x),__LINE__)
void group(const char* n,const std::function<void()>& f){f();++groups;std::cout<<"PASS "<<n<<'\n';}
void unsupported(const std::function<void()>& f){bool caught=false;try{f();}catch(const Unsupported&){caught=true;}CHECK(caught);}
void same(const State& a,const State& b){
 for(std::size_t i=0;i<5;++i)CHECK(encode_actor(a.actors[i])==encode_actor(b.actors[i]));
 CHECK(a.prepared_capacity==b.prepared_capacity);CHECK(a.selected_opponent==b.selected_opponent);
 CHECK(a.identity==b.identity);CHECK(a.remaining_total==b.remaining_total);
 CHECK(a.pass_processed==b.pass_processed);CHECK(a.human_prepared_capacity==b.human_prepared_capacity);
}
State fixture(){State s;for(auto& a:s.actors)a.profile=1;
 s.actors[4].population[0]=30;s.actors[0].population[2]=20;
 s.actors[0].city_x[2]=125;s.actors[0].city_y[2]=22;
 s.actors[4].target_actor=0;s.actors[4].target_city=2;
 s.actors[4].missiles[1]=1;s.actors[4].warheads[1]=1;return s;
}
void commit(State& s){
 human_commit(s,2,1,20);HumanSelection selection;selection.selected_class=1;selection.selected_capacity=20;
 finish_human_attack_fields(s,selection);CHECK(selection.pending_action==1);
 human_commit(s,selection.pending_action,1,20);
}
Word written(const MissilePrefix& p,std::uint32_t address){for(auto w:p.writes)if(w.old_address==address)return w.value;throw Unsupported("write absent");}
}
int main(){
 group("known short-path committed prefix stops before trajectory math",[]{
  auto s=fixture();Rng rng(1);commit(s);CHECK(s.actors[4].missiles[1]==0&&s.actors[4].warheads[1]==0);
  CHECK(s.human_prepared_capacity==0&&s.actors[4].action==1&&s.actors[4].magnitude==5);
  auto before=s;const auto p=begin_committed_missile(s,4,rng);same(s,before);
  CHECK(p.launch_x==143&&p.launch_y==101&&p.target_x==133&&p.target_y==6);
  CHECK(p.right==0&&p.long_path==0&&p.start.handler.effect==8&&p.start.handler.message_variant==1);
  CHECK(p.start.draws==3&&p.seed_before==1&&p.seed_after==0x2781e494&&rng.seed()==0x2781e494);
  CHECK(written(p,0x131a00)==0&&written(p,0x132418)==240&&written(p,0x132454)==240);
  CHECK(written(p,0x13247c)==240&&written(p,0x132440)==7&&written(p,0x132468)==7);
  CHECK(written(p,0x1323dc)==4057&&written(p,0x1324e0)==4057&&written(p,0x13242c)==4057);
  CHECK(p.writes.size()==21&&p.stop_old_pc==0x11780a&&p.stop_new_pc==0x23680a);
  unsupported([&]{written(p,0x131a0c);});unsupported([&]{written(p,0x132588);});
  for(auto e:{MissileEvent::trajectory_step,MissileEvent::arrival,MissileEvent::render_acknowledged,MissileEvent::clear_slots,MissileEvent::finish_action}){
   unsupported([&]{advance_missile(p,e);});CHECK(rng.seed()==0x2781e494);same(s,before);
  }
 });
 group("prefix coordinate widths and all launch/target pairs",[]{
  constexpr Word xs[]{26,80,125,20,82,4,68,4,73,106,209,241,249,290,302,244,299,225,223,297,130,115,130,167,164};
  constexpr Word ys[]{36,20,22,64,77,136,120,172,167,170,54,37,80,72,98,117,122,137,172,167,82,107,125,109,130};
  constexpr Word launch[]{57,44,260,270,143};
  for(std::size_t actor=0;actor<5;++actor)for(std::size_t target=0;target<5;++target)for(std::size_t city=0;city<5;++city){
   auto s=fixture();s.actors[actor].population[0]=1;s.actors[actor].action=1;
   s.actors[actor].target_actor=Word(target);s.actors[actor].target_city=Word(city);
   s.actors[target].city_x[city]=xs[target*5+city];s.actors[target].city_y[city]=ys[target*5+city];
   Rng rng(1);auto p=begin_committed_missile(s,actor,rng);
   auto dx=int(xs[target*5+city])+8-int(launch[actor]);
   CHECK(p.launch_x==launch[actor]&&p.target_x==Word(xs[target*5+city]+8));
   CHECK(p.right==Word(dx>0)&&p.long_path==Word(std::abs(dx)>50));
   CHECK(p.target_y==Word(ys[target*5+city]-16)&&p.seed_after==0x2781e494);
  }
  auto s=fixture();commit(s);s.actors[0].city_x[2]=0xfffc;s.actors[0].city_y[2]=3;Rng rng(1);
  auto p=begin_committed_missile(s,4,rng);CHECK(p.target_x==4&&p.target_y==0xfff3&&p.long_path==1);
 });
 group("prefix threshold50 versus51 and signed wrapped distance",[]{
  for(auto delta:{-51,-50,0,50,51}){
   auto s=fixture();commit(s);s.actors[0].city_x[2]=Word(143+delta-8);Rng rng(1);
   auto p=begin_committed_missile(s,4,rng);CHECK(p.long_path==Word(std::abs(delta)>50)&&p.right==Word(delta>0));
  }
  auto s=fixture();commit(s);s.actors[0].city_x[2]=Word(0x8000-8);Rng rng(1);
  auto p=begin_committed_missile(s,4,rng);CHECK(p.right==0&&p.long_path==0); // SUB.W wrapped negative, signed comparison
 });
 group("invalid indices, dead actor and uncommitted action reject without draws",[]{
  for(unsigned which=0;which<4;++which){auto s=fixture();commit(s);
   if(which==0)s.actors[4].target_actor=5;
   if(which==1)s.actors[4].target_city=5;
   if(which==2)s.actors[4].population={};
   if(which==3)s.actors[4].action=2;
   auto before=s;Rng rng(1);unsupported([&]{begin_committed_missile(s,4,rng);});same(s,before);CHECK(rng.seed()==1);
  }
 });
 group("reached impact/ack fixture is conditional, not missile trajectory",[]{
  auto s=fixture();commit(s);Rng rng(1);const auto prefix=begin_committed_missile(s,4,rng);
  // Independent supplied boundary fixture: NO claim prefix can reach5208 yet.
  ImpactState local;local.context={0,2,5,0};local.selector=prefix.start.handler.effect;
  auto impact=primary_impact(s,4,0x5208,local,rng);
  CHECK(impact.local.pending==15&&impact.local.context.magnitude==9&&s.actors[0].population[2]==20);
  CHECK(rng.seed()==0xc46b9b3d&&s.actors[4].missiles[1]==0&&s.actors[4].warheads[1]==0);
  DamageAcknowledgment pending(4,impact.local);
  auto before=s;auto blocked=pending.write_at_gate(s,195,false);CHECK(!blocked.written);same(s,before);
  auto write=pending.write_at_gate(s,196,false);
  CHECK(write.written&&write.loss==9&&write.render_request==1&&write.city_redraw==1);
  CHECK(s.actors[0].population[2]==11&&pending.local().pending==0&&pending.phase()==DamagePhase::awaiting_render_ack);
  CHECK(s.actors[0].scores==before.actors[0].scores); // relationship not yet applied
  auto after_write=s;unsupported([&]{pending.write_at_gate(s,196,false);});same(s,after_write);
  CHECK(!pending.observe_render_request(s,1));same(s,after_write);
  auto expected=s;apply_relationship_event(expected,4,0,0,9);
  CHECK(pending.observe_render_request(s,0));same(s,expected);CHECK(pending.phase()==DamagePhase::relationship_complete);
  unsupported([&]{pending.observe_render_request(s,0);});same(s,expected);CHECK(rng.seed()==0xc46b9b3d);
  CHECK(s.actors[0].population[2]==11); // no second population subtraction
  unsupported([&]{advance_missile(prefix,MissileEvent::clear_slots);});
  unsupported([&]{advance_missile(prefix,MissileEvent::finish_action);});
 });
 group("intercepted fixture still draws once but commits zero loss",[]{
  for(Word defense:{Word(0),Word(1)}){
   auto s=fixture();commit(s);s.actors[0].action=7;s.actors[0].magnitude=defense;s.actors[0].defenses={0,0};Rng rng(1);
   auto p=begin_committed_missile(s,4,rng);ImpactState local;local.context={0,2,5,0};local.selector=p.start.handler.effect;
   auto impact=primary_impact(s,4,0x5208,local,rng);CHECK(impact.local.selector==13&&impact.local.pending==15&&s.actors[4].magnitude==0);
   DamageAcknowledgment pending(4,impact.local);auto w=pending.write_at_gate(s,0,true);
   CHECK(w.written&&w.loss==0&&s.actors[0].population[2]==20);
   CHECK(!pending.observe_render_request(s,0xffff)&&pending.observe_render_request(s,0));
   CHECK(rng.seed()==0xc46b9b3d&&(s.actors[0].defenses==std::array<Word,2>{0,0}));
  }
 });
 group("pending gates defer invalid unused indices; final gate uses current magnitude",[]{
  auto s=fixture();ImpactState local;local.context={99,99,99,0};local.pending=15;DamageAcknowledgment gate(99,local);auto before=s;
  CHECK(!gate.write_at_gate(s,0,false).written);same(s,before);
  unsupported([&]{gate.write_at_gate(s,0,true);});same(s,before);
  local.context={0,2,999,0};s.actors[4].magnitude=30;DamageAcknowledgment valid(4,local);
  auto w=valid.write_at_gate(s,0,true);CHECK(w.loss==20&&valid.local().context.magnitude==20&&s.actors[4].magnitude==20&&s.actors[0].population[2]==0);
  CHECK(valid.observe_render_request(s,0));
 });
 group("ack cannot precede population write; failed relationship does not repeat damage",[]{
  auto s=fixture();ImpactState local;local.context={0,2,999,0};local.pending=15;s.actors[4].magnitude=5;
  DamageAcknowledgment gate(4,local);unsupported([&]{gate.observe_render_request(s,0);});
  CHECK(gate.write_at_gate(s,0,true).loss==5&&s.actors[0].population[2]==15);
  s.actors[0].profile=99;auto before=s;
  unsupported([&]{gate.observe_render_request(s,0);});same(s,before);CHECK(gate.phase()==DamagePhase::awaiting_render_ack);
  s.actors[0].profile=1;CHECK(gate.observe_render_request(s,0));CHECK(s.actors[0].population[2]==15);
 });
 std::cout<<groups<<" groups, "<<checks<<" checks passed\n";
}
