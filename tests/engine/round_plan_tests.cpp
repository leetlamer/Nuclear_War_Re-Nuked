#include "nuclear/round_plan.hpp"
#include <cstdlib>
#include <iostream>
using namespace nuclear;
namespace {
unsigned checks=0;
void check(bool value,int line) { ++checks; if (!value) { std::cerr<<line<<'\n'; std::exit(1); } }
#define CHECK(x) check(bool(x),__LINE__)
State fixture() { State s; for (auto& a:s.actors) a.population[0]=1; return s; }
void unchanged(const State& a,const State& b) {
    for (std::size_t i=0;i<5;++i) CHECK(encode_actor(a.actors[i])==encode_actor(b.actors[i]));
    CHECK(a.prepared_capacity==b.prepared_capacity); CHECK(a.selected_opponent==b.selected_opponent);
    CHECK(a.identity==b.identity); CHECK(a.remaining_total==b.remaining_total);
    CHECK(a.pass_processed==b.pass_processed); CHECK(a.human_prepared_capacity==b.human_prepared_capacity);
}
}
int main() {
    for (Word control:{Word(0),Word(1),Word(2),Word(0x8000),Word(0xffff)}) {
        State s=fixture(); Rng rng(1); const auto result=prepare_round_plan(s,control,rng);
        CHECK(result.human_control==control); CHECK(result.should_resolve==(control==0));
        CHECK(rng.seed()==0xaf1cf0fb);
        CHECK(s.selected_opponent==(std::array<Word,5>{3,4,0,1,0}));
        CHECK(s.actors[0].action==3 && s.actors[0].magnitude==6);
        CHECK(s.actors[1].action==3 && s.actors[1].magnitude==10);
        CHECK(s.actors[2].action==6 && s.actors[3].action==3);
        for (std::size_t i=0;i<4;++i) for(std::size_t j=0;j<5;++j) {
            CHECK(s.actors[i].scores[j]==(i==j?0:Word(-500)));
            CHECK(s.actors[i].classifications[j]==0xff);
        }
        for (auto score:s.actors[4].scores) CHECK(score==0);
    }
    std::cout<<"PASS instruction-derived planning and attitude golden sequence for zero and nonzero controls\n";
    // Actor3 fails after earlier staged actors have consumed RNG/planned actions.
    for (bool attitude_failure:{false,true}) {
        State s=fixture();
        if (attitude_failure) s.actors[4].classifications[0]=127;
        else s.actors[3].profile=5;
        const State before=s; Rng rng(1); bool caught=false;
        try { (void)prepare_round_plan(s,2,rng); } catch(const Unsupported&) { caught=true; }
        CHECK(caught); CHECK(rng.seed()==1); unchanged(s,before);
    }
    std::cout<<"PASS transactional rollback from planning and subsequent attitude failure\n";
    // No human input is generated when called with an already-carried control.
    State s=fixture(); s.actors[4].population={}; s.actors[4].action=5;
    const auto human_before=encode_actor(s.actors[4]); Rng rng(1);
    const auto result=prepare_round_plan(s,0xffff,rng);
    CHECK(!result.should_resolve && result.human_control==0xffff);
    CHECK(encode_actor(s.actors[4])==human_before);
    CHECK(rng.seed()!=1);
    std::cout<<"PASS absent human population preserves supplied control and human actor\n";
    std::cout<<"3 groups, "<<checks<<" checks passed\n";
}
