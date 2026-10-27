#include "nuclear/engine.hpp"
#include "nuclear/human_commands.hpp"
#include "nuclear/bomber_navigation.hpp"
#include "nuclear/resolver_markers.hpp"
#include "tests/fixtures/tables.hpp"
#include <cstdlib>
#include <iostream>
#include <string>

using namespace nuclear;
namespace {
unsigned checks=0, groups=0;
void require(bool ok,const char* expression,int line) {
    ++checks;
    if (!ok) { std::cerr<<"line "<<line<<": "<<expression<<"\n"; std::exit(1); }
}
#define CHECK(x) require(bool(x),#x,__LINE__)
template<class F> void unsupported(F f) {
    bool caught=false; try { f(); } catch (const Unsupported&) { caught=true; }
    CHECK(caught);
}
template<class F> void group(const char* name,F f) { f(); ++groups; std::cout<<"PASS "<<name<<"\n"; }
}
int main() {
group("quake prelude circular nonzero scan and preserved fields",[] {
    for (std::size_t actor=0;actor<5;++actor) for (unsigned mask=1;mask<32;++mask) {
        State s; auto& a=s.actors[actor]; a.profile=4; a.action=3;
        a.magnitude=99; a.target_actor=99; a.target_city=99;
        for (std::size_t c=0;c<5;++c) a.population[c]=(mask&(1u<<c))?Word(0x8000+c):0;
        State expected=s; Rng r(1), reference(1); auto c=std::size_t(reference.next()%5);
        while (a.population[c]==0) c=(c+1)%5;
        auto& e=expected.actors[actor]; e.action=86; e.target_actor=Word(actor);
        e.target_city=Word(c); e.magnitude=e.population[c];
        CHECK(quake_event_prelude(s,actor,r)==c); CHECK(r.seed()==reference.seed());
        for (std::size_t n=0;n<5;++n) CHECK(encode_actor(s.actors[n])==encode_actor(expected.actors[n]));
    }
});
group("quake prelude unsupported paths preserve state and RNG",[] {
    State s; s.actors[0].action=3; s.actors[0].profile=0xffff;
    auto before=encode_actor(s.actors[0]); Rng r(55);
    unsupported([&] { quake_event_prelude(s,0,r); });
    CHECK(r.seed()==55); CHECK(encode_actor(s.actors[0])==before);
    unsupported([&] { quake_event_prelude(s,5,r); }); CHECK(r.seed()==55);
});
group("quake decision and setup exact RNG sequence",[] {
    State s; s.actors[0].population={10,20,30,40,50}; Rng r(55);
    auto d=decide_action3_substitution(3,r); CHECK(d.selected_action==86); CHECK(d.draws==2);
    Rng reference(r.seed()); auto city=std::size_t(reference.next()%5);
    CHECK(quake_event_prelude(s,0,r)==city); CHECK(r.seed()==reference.seed());
    CHECK(s.actors[0].magnitude==s.actors[0].population[city]);
    CHECK(s.actors[0].action==86); CHECK(s.actors[0].target_actor==0);
});
group("ending last surviving actor uses positive city count",[] {
    for (unsigned mask=0;mask<32;++mask) {
        State s; int expected=-1;
        for (std::size_t a=0;a<5;++a) {
            s.actors[a].population[0]=0xffff;
            if (mask&(1u<<a)) { s.actors[a].population[4]=1; expected=int(a); }
        }
        CHECK(last_surviving_actor(s)==expected);
    }
});
group("end score instruction-derived population and city bonus cases",[] {
    Actor a; auto result=end_game_score(a);
    CHECK(result.population_total==0); CHECK(result.positive_cities==0); CHECK(result.score==0xffe7);
    for (Word n=1;n<=5;++n) {
        a.population[n-1]=10; result=end_game_score(a);
        CHECK(result.positive_cities==n); CHECK(result.population_total==n*10);
        CHECK(result.score==n*10+(n-1)*25);
    }
    a.population={0xffff,0x8000,0,1,0x7fff}; result=end_game_score(a);
    CHECK(result.positive_cities==2); CHECK(result.population_total==0xffff); CHECK(result.score==24);
    a.population.fill(0x7fff); result=end_game_score(a);
    CHECK(result.population_total==0x7ffb); CHECK(result.score==0x805f);
});
group("end score every word population preserves low-word arithmetic",[] {
    Actor a;
    for (unsigned value=0;value<=0xffff;++value) {
        a.population={Word(value),0xffff,0x8000,7,0}; auto result=end_game_score(a);
        unsigned cities=(value>0 && value<0x8000)?2:1;
        CHECK(result.population_total==Word(value+0xffffu+0x8000u+7));
        CHECK(result.positive_cities==cities);
        CHECK(result.score==Word(value+0xffffu+0x8000u+7+25*(cities-1)));
    }
});
group("attack roll golden rejection and message sequences",[] {
    struct C { unsigned seed,effect,missile,bomber,draws,end; };
    const C cases[]={{0,2,2,1,3,0xa70427df},{1,8,1,2,3,0x2781e494},
        {2,9,1,4,3,0xa7ffa149},{5,6,3,1,3,0x2978d768},{6,4,2,3,5,0x93724863}};
    for (auto c:cases) for (Word action:{Word(1),Word(5)}) {
        Rng r(c.seed); auto result=begin_attack_roll(action,r);
        CHECK(result.effect==c.effect); CHECK(result.message_variant==(action==1?c.missile:c.bomber));
        CHECK(result.draws==c.draws); CHECK(r.seed()==c.end);
    }
    Rng r(1); unsupported([&] { begin_attack_roll(2,r); }); CHECK(r.seed()==1);
});
group("combat impact interception matrix and exact one draw",[] {
    for (Word action:{Word(1),Word(5),Word(2)}) for (Word defense:{Word(0),Word(1),Word(2),Word(0xffff)})
    for (Word defending:{Word(0),Word(7)}) {
        State s; auto& a=s.actors[0]; auto& d=s.actors[1];
        a.action=action; a.magnitude=10; d.action=defending; d.magnitude=defense;
        d.population[3]=12; auto before=encode_actor(d); Rng r(1),reference(1);
        Word increment=Word(reference.next()%6+1);
        auto result=prepare_combat_impact(s,0,1,3,8,r);
        CHECK(a.magnitude==10+increment); CHECK(result.loss==12);
        bool intercepted=defending==7 && (defense==1 || (action!=5 && defense==0));
        CHECK(result.effect==(intercepted?13:8)); CHECK(r.seed()==reference.seed());
        CHECK(encode_actor(d)==before);
    }
});
group("combat impact preserves word overflow and signed upper-only clamp",[] {
    for (Word magnitude:{Word(0),Word(0x7fff),Word(0xfffe),Word(0x8000)})
    for (Word population:{Word(0),Word(1),Word(0x7fff),Word(0xffff),Word(0x8000)}) {
        State s; s.actors[0].magnitude=magnitude; s.actors[1].population[0]=population;
        Rng r(1); auto impact=prepare_combat_impact(s,0,1,0,2,r);
        Word sum=add_word(magnitude,3); CHECK(s.actors[0].magnitude==sum);
        CHECK(impact.loss==(signed_word(sum)>signed_word(population)?population:sum));
        CHECK(impact.effect==2);
    }
    State s; s.actors[0].magnitude=0xffff; s.actors[0].action=7; s.actors[0].population[0]=10;
    Rng r(0); auto result=prepare_combat_impact(s,0,0,0,8,r);
    CHECK(s.actors[0].magnitude==0); CHECK(result.effect==13); CHECK(result.loss==0);
});
group("combat impact invalid and inactive entries are transactional",[] {
    for (Word effect:{Word(0),Word(0xffff)}) {
        State s; Rng r(1); auto before=encode_actor(s.actors[0]);
        unsupported([&] { prepare_combat_impact(s,0,1,0,effect,r); });
        CHECK(r.seed()==1); CHECK(encode_actor(s.actors[0])==before);
    }
    State s; Rng r(1);
    unsupported([&] { prepare_combat_impact(s,5,1,0,2,r); });
    unsupported([&] { prepare_combat_impact(s,0,5,0,2,r); });
    unsupported([&] { prepare_combat_impact(s,0,1,5,2,r); }); CHECK(r.seed()==1);
});
group("full bounded action3 prelude composes nine exact routes",[] {
    const unsigned seeds[]={0,2,19,55,79,123,148,249,262};
    for (unsigned seed:seeds) {
        State s; for (auto& a:s.actors) { a.profile=0; a.population={20,20,20,20,20}; }
        s.actors[0].action=3; s.actors[0].target_actor=4; s.actors[0].target_city=4;
        State expected=s; Rng r(seed),reference(seed);
        auto decision=decide_action3_substitution(3,reference); unsigned draws=decision.draws;
        if (decision.selected_action==14) draws+=transfer_event_prelude(expected,0,decision.event_roll,reference).draws;
        else if (decision.selected_action==86) { quake_event_prelude(expected,0,reference); ++draws; }
        else expected.actors[0].action=decision.selected_action;
        auto result=apply_action3_prelude(s,0,r);
        CHECK(result.decision.selected_action==decision.selected_action); CHECK(result.draws==draws);
        CHECK(r.seed()==reference.seed());
        for (unsigned a=0;a<5;++a) CHECK(encode_actor(s.actors[a])==encode_actor(expected.actors[a]));
    }
});
group("action3 prelude skips unchanged actions and failed gate",[] {
    for (Word action:{Word(0),Word(1),Word(3),Word(86)}) {
        State s; s.actors[0].action=action; auto before=encode_actor(s.actors[0]); Rng r(30);
        auto result=apply_action3_prelude(s,0,r);
        CHECK(!result.decision.substituted); CHECK(result.draws==(action==3?1:0));
        CHECK(r.seed()==(action==3?0xb53d60ffu:30u)); CHECK(encode_actor(s.actors[0])==before);
    }
});
group("action3 prelude failure restores selection draws and state",[] {
    for (unsigned seed:{55u,123u}) {
        State s; s.actors[0].action=3; auto before=encode_actor(s.actors[0]); Rng r(seed);
        unsupported([&] { apply_action3_prelude(s,0,r); });
        CHECK(r.seed()==seed); CHECK(encode_actor(s.actors[0])==before);
    }
    State s; Rng r(1); unsupported([&] { apply_action3_prelude(s,5,r); }); CHECK(r.seed()==1);
});
group("pending combat commit clamps source magnitude and applies diplomacy",[] {
    State s; s.actors[0].magnitude=20; s.actors[1].population[2]=5;
    CHECK(combat_population_commit(s,0,1,2)==5);
    CHECK(s.actors[0].magnitude==5); CHECK(s.actors[1].population[2]==0);
    CHECK(s.actors[1].scores[0]==11000); CHECK(s.actors[2].scores[0]==0);
    CHECK(s.actors[3].scores[0]==0); CHECK(s.actors[4].scores[0]==0);
});
group("pending combat commit uses signed upper-only cap and word subtraction",[] {
    for (Word magnitude:{Word(0),Word(1),Word(0x7fff),Word(0x8000),Word(0xffff)})
    for (Word population:{Word(0),Word(1),Word(0x7fff),Word(0x8000),Word(0xffff)}) {
        State s; s.actors[0].magnitude=magnitude; s.actors[1].population[0]=population;
        Word expected=signed_word(magnitude)>signed_word(population)?population:magnitude;
        CHECK(combat_population_commit(s,0,1,0)==expected);
        CHECK(s.actors[0].magnitude==expected); CHECK(s.actors[1].population[0]==sub_word(population,expected));
    }
    State s; s.actors[0].magnitude=0xffff; s.actors[1].population[0]=7;
    combat_population_commit(s,0,1,0); CHECK(s.actors[1].population[0]==8);
    CHECK(s.actors[1].scores[0]==8600);
});
group("pending combat commit aliasing and unsupported input rollback",[] {
    State s; s.actors[0].magnitude=12; s.actors[0].population[1]=4;
    CHECK(combat_population_commit(s,0,0,1)==4); CHECK(s.actors[0].magnitude==4);
    CHECK(s.actors[0].population[1]==0);
    s.actors[1].profile=99; s.actors[0].magnitude=3; s.actors[2].population[0]=10;
    State before=s; unsupported([&] { combat_population_commit(s,0,2,0); });
    for (unsigned a=0;a<5;++a) CHECK(encode_actor(s.actors[a])==encode_actor(before.actors[a]));
    unsupported([&] { combat_population_commit(s,5,1,0); });
    unsupported([&] { combat_population_commit(s,0,5,0); });
    unsupported([&] { combat_population_commit(s,0,1,5); });
});
group("growth handler message then magnitude exact RNG and later commit",[] {
    Actor a; a.action=9; a.target_city=3; a.population[3]=0xfffe; Rng r(1);
    CHECK(growth_handler_initialize(a,r)==3); CHECK(a.magnitude==20);
    CHECK(r.seed()==0x967eb0e7); CHECK(a.population[3]==0xfffe); CHECK(a.action==9);
    growth_commit(a,3); CHECK(a.population[3]==18);
    bool seen[5]{};
    for (unsigned seed=0;seed<100;++seed) {
        Rng r2(seed),reference(seed); Actor b; auto message=reference.next()%4+1;
        auto magnitude=(reference.next()%5+1)*5;
        CHECK(growth_handler_initialize(b,r2)==message); CHECK(b.magnitude==magnitude);
        CHECK(r2.seed()==reference.seed()); seen[magnitude/5-1]=true;
    }
    for (bool present:seen) CHECK(present);
});
group("quake handler clears selected own city only and retains magnitude",[] {
    for (Word city=0;city<5;++city) {
        Actor a; a.population={1,0xffff,0x8000,4,5}; a.target_city=city;
        a.magnitude=a.population[city]; a.action=86; a.target_actor=99; a.profile=4;
        Actor expected=a; expected.population[city]=0; Rng r(1);
        CHECK(quake_handler_initialize(a,r)==3); CHECK(r.seed()==0x41c67ea6);
        CHECK(encode_actor(a)==encode_actor(expected));
    }
    Actor a; a.target_city=5; Rng r(1); auto before=encode_actor(a);
    unsupported([&] { quake_handler_initialize(a,r); });
    CHECK(r.seed()==1); CHECK(encode_actor(a)==before);
});
group("city loss handler first nonzero selection and complete separate commit",[] {
    for (unsigned mask=1;mask<32;++mask) for (Word action:{Word(11),Word(12)}) {
        Actor a; a.action=action; a.target_actor=99; a.profile=3;
        for (unsigned c=0;c<5;++c) a.population[c]=(mask&(1u<<c))?Word(0x8000+c):0;
        Actor expected=a; Word city=0; while (expected.population[city]==0) ++city;
        expected.target_city=city; expected.magnitude=expected.population[city]; Rng r(1);
        CHECK(city_loss_handler_initialize(a,action,r)==(action==11?3:3));
        CHECK(r.seed()==0x41c67ea6); CHECK(encode_actor(a)==encode_actor(expected));
        if (action==11) city_removal_commit(a,r); else weight_drop_commit(a,r);
        CHECK(a.population[city]==0); CHECK(a.magnitude==expected.magnitude);
        CHECK(a.action==(action==11?0x11:0x12)); CHECK(r.seed()==0x967eb0e7);
    }
});
group("city loss handler rejects all-zero and invalid action transactionally",[] {
    Actor a; a.action=11; a.magnitude=99; Rng r(7); auto before=encode_actor(a);
    unsupported([&] { city_loss_handler_initialize(a,11,r); });
    CHECK(r.seed()==7); CHECK(encode_actor(a)==before);
    a.population[4]=1; before=encode_actor(a);
    unsupported([&] { city_loss_handler_initialize(a,13,r); });
    CHECK(r.seed()==7); CHECK(encode_actor(a)==before);
});
group("special handler initialization all supported actions exact seeded traces",[] {
    struct C { Word action,effect,draws,message; };
    const C cases[]={{3,13,1,3},{8,15,0,0},{9,16,2,3},{11,17,1,3},{12,18,1,3},
        {13,19,2,3},{14,20,1,3},{15,21,1,3},{16,69,3,1},{86,86,1,3}};
    const std::uint32_t seeds[]={1,0x41c67ea6,0x967eb0e7,0x2781e494};
    for (auto c:cases) {
        Actor a; a.action=c.action; a.profile=3; a.target_actor=4; a.target_city=3;
        a.population={10,20,30,40,50}; a.magnitude=77; Actor expected=a;
        if (c.action==9 || c.action==13) expected.magnitude=20;
        if (c.action==11 || c.action==12) { expected.magnitude=10; expected.target_city=0; }
        if (c.action==16) expected.magnitude=9;
        if (c.action==86) expected.population[3]=0;
        Rng r(1); auto result=initialize_event_handler(a,r);
        CHECK(result.effect==c.effect); CHECK(result.draws==c.draws);
        CHECK(result.message_variant==c.message); CHECK(r.seed()==seeds[c.draws]);
        CHECK(result.alien_initial_x==(c.action==16?164:0));
        CHECK(encode_actor(a)==encode_actor(expected));
    }
});
group("special handler initialization errors preserve state and RNG",[] {
    for (Word action:{Word(0),Word(1),Word(2),Word(4),Word(5),Word(6),Word(7),Word(10),Word(0xffff)}) {
        Actor a; a.action=action; Rng r(1); auto before=encode_actor(a);
        unsupported([&] { initialize_event_handler(a,r); }); CHECK(r.seed()==1); CHECK(encode_actor(a)==before);
    }
    Actor a; a.action=11; Rng r(1); auto before=encode_actor(a);
    unsupported([&] { initialize_event_handler(a,r); }); CHECK(r.seed()==1); CHECK(encode_actor(a)==before);
});
group("combat evaluation all effect branches and post-clamp magnitude store",[] {
    const Word losses[]={0,0,11,14,13,13,15,18,23,26,39,0};
    for (Word effect=2;effect<=13;++effect) {
        State s; s.actors[0].action=1; s.actors[0].magnitude=10;
        s.actors[1].population[0]=100; s.prepared_capacity[0]=50; Rng r(1);
        auto result=evaluate_combat_impact(s,0,1,0,effect,false,r);
        CHECK(result.effect==effect); CHECK(result.damage.loss==losses[effect-2]);
        CHECK(s.actors[0].magnitude==losses[effect-2]); CHECK(s.actors[0].action==1);
        CHECK(s.prepared_capacity[0]==((effect==2 || effect==13)?0:50));
        CHECK(s.actors[1].population[0]==100); CHECK(r.seed()==0x41c67ea6);
    }
});
group("combat evaluation bomber loss interception and explicit magnitude25 snapshot",[] {
    for (Word effect:{Word(2),Word(13)}) {
        State s; s.actors[0].action=5; s.actors[0].magnitude=10;
        s.actors[1].population[0]=100; s.prepared_capacity[0]=100; Rng r(1);
        evaluate_combat_impact(s,0,1,0,effect,false,r);
        CHECK(s.actors[0].action==(effect==2?88:0)); CHECK(s.actors[0].magnitude==0);
        CHECK(s.prepared_capacity[0]==0);
    }
    State s; s.actors[0].action=1; s.actors[0].magnitude=10; s.actors[1].population[0]=100;
    Rng r(1); auto result=evaluate_combat_impact(s,0,1,0,12,true,r);
    CHECK(result.damage.loss==100); CHECK(s.actors[0].magnitude==100);
    s.actors[0].magnitude=10; s.actors[1].action=7; s.actors[1].magnitude=1;
    result=evaluate_combat_impact(s,0,1,0,12,true,r);
    CHECK(result.effect==13); CHECK(result.damage.loss==0);
});
group("bounded attack sequence draws damage and diplomacy golden fixture",[] {
    State s; s.actors[0].action=1; s.actors[0].magnitude=10;
    s.actors[1].population[0]=100; s.prepared_capacity[0]=50; Rng r(1);
    auto roll=begin_attack_roll(1,r); CHECK(roll.effect==8); CHECK(roll.draws==3);
    auto result=evaluate_combat_impact(s,0,1,0,roll.effect,false,r);
    CHECK(result.damage.loss==14); CHECK(r.seed()==0xc46b9b3d);
    CHECK(combat_population_commit(s,0,1,0)==14);
    CHECK(s.actors[1].population[0]==86); CHECK(s.actors[1].scores[0]==14600);
    CHECK(r.seed()==0xc46b9b3d); CHECK(s.actors[0].magnitude==14);
});
group("combat evaluation rejects unknown effects without mutation",[] {
    for (Word effect:{Word(0),Word(1),Word(14),Word(86),Word(0xffff)}) {
        State s; s.actors[0].magnitude=10; Rng r(1); auto before=encode_actor(s.actors[0]);
        unsupported([&] { evaluate_combat_impact(s,0,1,0,effect,false,r); });
        CHECK(r.seed()==1); CHECK(encode_actor(s.actors[0])==before);
    }
});
group("human entry four draws exact values and order",[] {
    Rng r(1); const std::array<Word,4> expected={40,88,56,75};
    CHECK(human_entry_rolls(r)==expected); CHECK(r.seed()==0xc46b9b3d);
    for (unsigned seed=0;seed<100;++seed) {
        Rng actual(seed),reference(seed); auto values=human_entry_rolls(actual);
        for (auto value:values) CHECK(value==reference.next()%107+1);
        CHECK(actual.seed()==reference.seed());
    }
});
group("status refresh RNG all neutral living actors golden periods",[] {
    State s; for (auto& a:s.actors) a.population[0]=1;
    Rng r(1); auto report=attitude_refresh_rng(s,r);
    const std::array<Word,4> expected={98,180,80,131};
    CHECK(report.random_periods==expected); CHECK(report.draws==8);
    for (auto active:report.randomized) CHECK(active);
    CHECK(r.seed()==0x9cfbae39);
});
group("status refresh signed buckets profile inversion and dead filtering",[] {
    for (Word profile:{Word(0),Word(4)}) for (Word score:{Word(0),Word(9000),Word(9001),Word(18001),
          Word(0xffff),Word(-9000),Word(-9001),Word(-18001),Word(0x8000)}) {
        State s; auto& a=s.actors[0]; a.profile=profile; a.scores[4]=score; a.population[0]=1;
        auto image=encode_actor(a); Rng r(1);
        auto report=attitude_refresh_rng(s,r);
        int bucket=signed_word(relationship_bucket(score)); if (profile==4) bucket=-bucket;
        bool draws=bucket>=0;
        CHECK(report.draws==(draws?2:0)); CHECK(report.randomized[0]==draws);
        CHECK(report.random_periods[0]==(draws?98:0)); CHECK(r.seed()==(draws?0x967eb0e7u:1u));
        CHECK(encode_actor(a)==image);
        a.population[0]=0x8000; Rng dead(1); report=attitude_refresh_rng(s,dead);
        CHECK(report.draws==0); CHECK(dead.seed()==1);
    }
});
group("human delivery auto and manual inventory field transitions",[] {
    State s; auto& human=s.actors[4]; human.action=6; human.magnitude=99;
    human.missiles={1,2,3,4}; human.bombers={2,3}; human.defenses={4,5};
    HumanSelection selection{0,0,77}; auto original=encode_actor(human);
    CHECK(select_human_delivery_fields(s,selection,2,0)); CHECK(selection.pending_action==2);
    CHECK(selection.selected_class==3); CHECK(selection.selected_capacity==100); CHECK(human.magnitude==2);
    CHECK(human.action==6); CHECK(human.missiles[3]==4);
    CHECK(select_human_delivery_fields(s,selection,4,0)); CHECK(selection.pending_action==4);
    CHECK(selection.selected_class==1); CHECK(selection.selected_capacity==100); CHECK(human.magnitude==2);
    CHECK(select_human_delivery_fields(s,selection,7,1)); CHECK(selection.pending_action==7);
    CHECK(selection.selected_class==0); CHECK(selection.selected_capacity==100); CHECK(human.magnitude==0);
    human.magnitude=99; CHECK(encode_actor(human)==original);
    for (Word choice=1;choice<=4;++choice) {
        CHECK(select_human_delivery_fields(s,selection,2,choice));
        CHECK(selection.selected_class==choice-1); CHECK(selection.selected_capacity==class_capacity(Word(choice-1)));
    }
});
group("human binary auto uses signed positive second count including quirk",[] {
    for (Word action:{Word(4),Word(7)}) for (Word first:{Word(0),Word(1),Word(0xffff)})
    for (Word second:{Word(0),Word(1),Word(0xffff),Word(0x8000)}) {
        State s; auto& a=s.actors[4]; a.magnitude=33; a.action=3;
        if (action==4) a.bombers={first,second}; else a.defenses={first,second};
        HumanSelection selection{3,3,77}; auto before=encode_actor(a);
        bool accepted=select_human_delivery_fields(s,selection,action,0);
        CHECK(accepted==(first!=0 || second!=0));
        if (!accepted) { CHECK(selection.pending_action==3); CHECK(encode_actor(a)==before); continue; }
        Word index=Word(signed_word(second)>0); CHECK(selection.selected_class==index);
        CHECK(selection.pending_action==action); CHECK(a.action==3);
        CHECK(selection.selected_capacity==(action==4?Word(index*50+50):77));
        CHECK(a.magnitude==(action==7?index:33));
    }
});
group("human delivery manual negative counts accepted and empty rejected",[] {
    for (Word action:{Word(2),Word(4),Word(7)}) {
        State s; auto& a=s.actors[4]; a.missiles[1]=0xffff; a.bombers[1]=0xffff; a.defenses[1]=0xffff;
        HumanSelection selection{3,0,44}; auto before=encode_actor(a);
        CHECK(!select_human_delivery_fields(s,selection,action,1)); CHECK(selection.pending_action==3);
        CHECK(selection.selected_class==0); CHECK(selection.selected_capacity==44); CHECK(encode_actor(a)==before);
        CHECK(select_human_delivery_fields(s,selection,action,2)); CHECK(selection.selected_class==1);
    }
});
group("human delivery commit remains separate and preparation magnitude retained",[] {
    State s; s.actors[4].missiles[2]=1; s.actors[4].magnitude=9;
    HumanSelection selection{0,0,0}; CHECK(select_human_delivery_fields(s,selection,2,3));
    CHECK(s.actors[4].action==0); CHECK(s.actors[4].missiles[2]==1); CHECK(s.human_prepared_capacity==0);
    human_commit(s,selection.pending_action,selection.selected_class,selection.selected_capacity);
    CHECK(s.actors[4].action==2); CHECK(s.actors[4].missiles[2]==0); CHECK(s.human_prepared_capacity==50);
    CHECK(s.actors[4].magnitude==2);
});
group("human delivery sentinel guards and unsupported paths preserve fields",[] {
    State s; s.actors[4].missiles[0]=1; HumanSelection selection; auto before=encode_actor(s.actors[4]);
    CHECK(!select_human_delivery_fields(s,selection,2,1)); CHECK(selection.selected_class==0xffff);
    CHECK(encode_actor(s.actors[4])==before);
    selection.selected_class=0; s.actors[4].missiles[0]=0;
    unsupported([&] { select_human_delivery_fields(s,selection,2,0); }); CHECK(selection.pending_action==0);
    s.actors[4].bombers[0]=1;
    unsupported([&] { select_human_delivery_fields(s,selection,4,3); });
    unsupported([&] { select_human_delivery_fields(s,selection,2,5); });
    unsupported([&] { select_human_delivery_fields(s,selection,3,0); }); CHECK(selection.selected_class==0);
});
group("message RNG skips spaces retains other bytes and cursor initial call",[] {
    for (Word x:{Word(78),Word(0),Word(0xffff)}) for (Word y:{Word(182),Word(0)}) {
        Rng r(1),reference(1); auto report=advance_message_rng("A B  ",x,y,r);
        unsigned calls=2+(x!=78 || y!=182); CHECK(report.noise_calls==calls); CHECK(report.draws==128*calls);
        for (unsigned n=0;n<128*calls;++n) reference.next(); CHECK(r.seed()==reference.seed());
    }
    Rng r(1); auto empty=advance_message_rng("   ",78,182,r);
    CHECK(empty.draws==0); CHECK(r.seed()==1);
    auto initial=advance_message_rng("",0,0,r); CHECK(initial.draws==128);
    Rng reference(1); for (unsigned i=0;i<128;++i) reference.next(); CHECK(r.seed()==reference.seed());
});
group("message RNG original C-string termination and byte space comparison",[] {
    Rng r(1),reference(1); std::string text("A\0ZZ",4);
    auto result=advance_message_rng(text,78,182,r); CHECK(result.noise_calls==1); CHECK(result.draws==128);
    for (unsigned i=0;i<128;++i) reference.next(); CHECK(r.seed()==reference.seed());
    const std::string bytes={char(0xff),'\n','\t',' '};
    result=advance_message_rng(bytes,78,182,r); CHECK(result.noise_calls==3); CHECK(result.draws==384);
    for (unsigned i=0;i<384;++i) reference.next(); CHECK(r.seed()==reference.seed());
});
group("message RNG unsupported long string preserves RNG",[] {
    Rng r(1); unsupported([&] { advance_message_rng(std::string(32768,'A'),0,0,r); }); CHECK(r.seed()==1);
});
group("human warhead signed capacity guards and target sentinel",[] {
    for (Word action:{Word(2),Word(4),Word(5),Word(3)})
    for (Word capacity:{Word(0),Word(9),Word(10),Word(100),Word(0xffff),Word(0x8000)}) {
        State s; auto& a=s.actors[4]; a.action=action; a.warheads[0]=1;
        s.human_prepared_capacity=capacity; HumanSelection selection{6,2,99}; auto before=encode_actor(a);
        auto expected=HumanWarheadCheck::accepted;
        if ((action==4 || action==5) && capacity==0) expected=HumanWarheadCheck::no_capacity;
        else if ((action==2 || action==4 || action==5) && signed_word(capacity)<10) expected=HumanWarheadCheck::too_large;
        else if (action==3) expected=HumanWarheadCheck::no_prepared_delivery;
        CHECK(select_human_warhead_fields(s,selection,1,0)==expected);
        CHECK(selection.selected_class==0); CHECK(selection.selected_capacity==10);
        CHECK(selection.pending_action==6); CHECK(encode_actor(a)==before);
        auto missing=select_human_warhead_fields(s,selection,1,0xffff);
        CHECK(missing==(expected==HumanWarheadCheck::accepted?HumanWarheadCheck::no_target:expected));
    }
});
group("human warhead selection writes survive rejection with exact empty semantics",[] {
    State s; HumanSelection selection{6,2,99};
    CHECK(select_human_warhead_fields(s,selection,1,0)==HumanWarheadCheck::no_warhead);
    CHECK(selection.selected_class==2); CHECK(selection.selected_capacity==99);
    CHECK(select_human_warhead_fields(s,selection,0,0)==HumanWarheadCheck::no_warhead);
    CHECK(selection.selected_class==0xffff); CHECK(selection.selected_capacity==99);
    s.actors[4].warheads={0,0xffff,1,0};
    CHECK(select_human_warhead_fields(s,selection,0,0)==HumanWarheadCheck::no_prepared_delivery);
    CHECK(selection.selected_class==2); CHECK(selection.selected_capacity==50);
    CHECK(select_human_warhead_fields(s,selection,2,0)==HumanWarheadCheck::no_prepared_delivery);
    CHECK(selection.selected_class==1); CHECK(selection.selected_capacity==20);
});
group("human attack final fields and resource commit remain separate",[] {
    for (Word action:{Word(2),Word(4),Word(5)}) {
        State s; auto& a=s.actors[4]; a.action=action; a.magnitude=99; a.warheads[2]=1;
        s.human_prepared_capacity=50; HumanSelection selection;
        CHECK(select_human_warhead_fields(s,selection,3,0)==HumanWarheadCheck::accepted);
        finish_human_attack_fields(s,selection);
        CHECK(selection.pending_action==(action==2?1:5)); CHECK(a.magnitude==10);
        CHECK(a.action==action); CHECK(a.warheads[2]==1); CHECK(s.human_prepared_capacity==50);
        human_commit(s,selection.pending_action,selection.selected_class,selection.selected_capacity);
        CHECK(a.warheads[2]==0); CHECK(a.action==(action==2?1:5)); CHECK(s.human_prepared_capacity==0);
    }
});
group("human warhead and attack field unsupported paths are transactional",[] {
    State s; s.actors[4].action=2; s.actors[4].magnitude=99; HumanSelection selection{3,1,20};
    unsupported([&] { select_human_warhead_fields(s,selection,5,0); }); CHECK(selection.selected_class==1);
    selection.selected_class=0xffff; unsupported([&] { finish_human_attack_fields(s,selection); });
    CHECK(s.actors[4].magnitude==99); CHECK(selection.pending_action==3);
    selection.selected_class=0; s.actors[4].action=3;
    unsupported([&] { finish_human_attack_fields(s,selection); }); CHECK(s.actors[4].magnitude==99);
});
group("human propaganda pending sentinel and exact three direct draws",[] {
    State s; HumanSelection selection{6,1,99}; Rng r(1);
    CHECK(!begin_human_propaganda_fields(selection,0xffff)); CHECK(selection.pending_action==3);
    CHECK(selection.selected_class==1); CHECK(selection.selected_capacity==99);
    CHECK(begin_human_propaganda_fields(selection,0)); auto before=encode_actor(s.actors[4]);
    CHECK(roll_human_propaganda_fields(s,selection,r)==59);
    CHECK(selection.selected_class==2); CHECK(selection.selected_capacity==50);
    CHECK(s.actors[4].magnitude==4); CHECK(r.seed()==0x2781e494);
    s.actors[4].magnitude=0; CHECK(encode_actor(s.actors[4])==before);
    selection.pending_action=6; auto seed=r.seed();
    unsupported([&] { roll_human_propaganda_fields(s,selection,r); }); CHECK(r.seed()==seed);
});
group("propaganda effect transfer and event2 diplomacy",[] {
    State s; auto& a=s.actors[0]; a.population={10,20,0,0,0}; a.action=3;
    a.target_actor=1; a.target_city=2; a.magnitude=7; s.actors[1].population[2]=30;
    Rng r(1); auto result=propaganda_commit(s,0,r);
    CHECK(result.loss==7); CHECK(result.source_city==2); CHECK(result.destination_city==1); CHECK(result.draws==0);
    CHECK(a.population[1]==27); CHECK(s.actors[1].population[2]==23); CHECK(a.magnitude==7); CHECK(a.action==3);
    CHECK(s.actors[1].scores[0]==11100); CHECK(s.actors[2].scores[0]==700); CHECK(r.seed()==1);
});
group("propaganda human smallest-city profile override clears instead of restores",[] {
    State s; auto& a=s.actors[4]; a.profile=4; a.population={20,5,15,0,0};
    a.target_actor=1; a.target_city=0; a.magnitude=10; s.actors[1].population[0]=30;
    Rng r(1); auto result=propaganda_commit(s,4,r);
    CHECK(result.destination_city==1); CHECK(a.population[1]==15); CHECK(a.profile==0);
    CHECK(result.draws==0); CHECK(r.seed()==1);
    a.population={10,5,0,0,0}; a.target_actor=4; a.target_city=0; a.magnitude=6;
    result=propaganda_commit(s,4,r); CHECK(result.destination_city==0); CHECK(a.population[0]==10);
    CHECK(a.population[1]==5); CHECK(a.magnitude==6);
});
group("propaganda source signed cap overflow and chooser RNG",[] {
    State s; auto& a=s.actors[0]; a.profile=3; a.population={10,20,0,0,0};
    a.target_actor=1; a.target_city=0; a.magnitude=20; s.actors[1].population[0]=5;
    Rng r(1); auto result=propaganda_commit(s,0,r);
    CHECK(result.loss==5); CHECK(a.magnitude==5); CHECK(result.draws==1); CHECK(r.seed()==0x41c67ea6);
    CHECK(result.destination_city==1); CHECK(a.population[1]==25); CHECK(s.actors[1].population[0]==0);
    a.profile=0; a.magnitude=0xffff; a.population={0x8000,0x7fff,0,0,0}; s.actors[1].population[0]=7;
    result=propaganda_commit(s,0,r); CHECK(result.loss==0xffff); CHECK(a.magnitude==0xffff);
    CHECK(s.actors[1].population[0]==8); CHECK(a.population[1]==0x7ffe);
});
group("propaganda unsupported paths roll back scores population and RNG",[] {
    State s; auto& a=s.actors[0]; a.profile=3; a.target_actor=1; a.magnitude=5;
    s.actors[1].population[0]=10; State before=s; Rng r(1);
    unsupported([&] { propaganda_commit(s,0,r); }); CHECK(r.seed()==1);
    for (unsigned n=0;n<5;++n) CHECK(encode_actor(s.actors[n])==encode_actor(before.actors[n]));
    a.target_city=5; unsupported([&] { propaganda_commit(s,0,r); });
    a.target_city=0; a.target_actor=5; unsupported([&] { propaganda_commit(s,0,r); });
    unsupported([&] { propaganda_commit(s,5,r); }); CHECK(r.seed()==1);
});
group("special effect dispatch composes all ten verified mutation paths",[] {
    for (Word effect:{Word(0x0d),Word(0x0f),Word(0x10),Word(0x11),Word(0x12),Word(0x13),
                     Word(0x14),Word(0x15),Word(0x45),Word(0x56)}) {
        State s; for (auto& a:s.actors) { a.profile=0; a.population={10,20,30,40,50}; }
        s.actors[0].action=3; s.actors[0].target_actor=1; s.actors[0].target_city=2; s.actors[0].magnitude=7;
        State expected=s; Rng r(1),reference(1); auto& a=expected.actors[0];
        switch (effect) {
        case 0x0d: propaganda_commit(expected,0,reference); break;
        case 0x0f: defection_commit(expected,0,reference); break;
        case 0x10: growth_commit(a,2); break;
        case 0x11: city_removal_commit(a,reference); break;
        case 0x12: weight_drop_commit(a,reference); break;
        case 0x13: cow_population_commit(expected,0,reference); break;
        case 0x14: transfer_arrival_commit(expected,0); break;
        case 0x15: radiation_population_commit(a,reference); break;
        case 0x45: double_largest_city(a); break;
        case 0x56: quake_effect_notice(a,reference); break;
        }
        apply_special_event_effect(s,0,effect,r); CHECK(r.seed()==reference.seed());
        for (unsigned n=0;n<5;++n) CHECK(encode_actor(s.actors[n])==encode_actor(expected.actors[n]));
    }
});
group("bounded selected event initialization marker and commit golden sequence",[] {
    State s; s.actors[0].action=3; s.actors[0].population={10,20,30,40,50}; Rng r(19);
    auto prelude=apply_action3_prelude(s,0,r); CHECK(prelude.decision.selected_action==11); CHECK(prelude.draws==2);
    auto initial=initialize_event_handler(s.actors[0],r); CHECK(initial.effect==17); CHECK(initial.draws==1);
    CHECK(s.actors[0].target_city==0); CHECK(s.actors[0].magnitude==10);
    apply_special_event_effect(s,0,initial.effect,r);
    CHECK(s.actors[0].population[0]==0); CHECK(s.actors[0].population[4]==50);
    CHECK(s.actors[0].action==17); CHECK(s.actors[0].magnitude==10);
    Rng reference(19); for (unsigned i=0;i<4;++i) reference.next(); CHECK(r.seed()==reference.seed());
});
group("special effect unsupported entry and nested failure roll back",[] {
    for (Word effect:{Word(0),Word(2),Word(0x0e),Word(0xffff)}) {
        State s; s.actors[0].magnitude=7; auto before=encode_actor(s.actors[0]); Rng r(1);
        unsupported([&] { apply_special_event_effect(s,0,effect,r); }); CHECK(r.seed()==1);
        CHECK(encode_actor(s.actors[0])==before);
    }
    State s; s.actors[0].profile=3; s.actors[0].magnitude=7;
    s.actors[0].target_actor=1; s.actors[1].population[0]=10; State before=s; Rng r(1);
    unsupported([&] { apply_special_event_effect(s,0,0x0d,r); }); CHECK(r.seed()==1);
    for (unsigned n=0;n<5;++n) CHECK(encode_actor(s.actors[n])==encode_actor(before.actors[n]));
    unsupported([&] { apply_special_event_effect(s,5,0x10,r); }); CHECK(r.seed()==1);
});
group("human confirmation pending and target guard matrix",[] {
    for (Word action=0;action<=7;++action) for (Word marker:{Word(0),Word(4),Word(0xffff)}) {
        bool ready=action!=0 && (!(action==1 || action==3 || action==5) || marker!=0xffff);
        CHECK(human_confirmation_ready(action,marker)==ready);
    }
    CHECK(human_confirmation_ready(0xffff,0xffff)); // predicate only, not commit permission
    State s; HumanSelection selection{3,0,10}; auto before=encode_actor(s.actors[4]);
    CHECK(!confirm_human_selection_fields(s,selection,0xffff)); CHECK(encode_actor(s.actors[4])==before);
    CHECK(confirm_human_selection_fields(s,selection,1)); CHECK(s.actors[4].action==3);
    selection.pending_action=0xffff; before=encode_actor(s.actors[4]);
    unsupported([&] { confirm_human_selection_fields(s,selection,0); }); CHECK(encode_actor(s.actors[4])==before);
});
group("human production choice and confirmation resource penalty",[] {
    State s; s.actors[4].action=6; HumanSelection selection{3,0xffff,99};
    select_human_production_fields(selection); CHECK(selection.pending_action==6);
    CHECK(selection.selected_class==0xffff); CHECK(selection.selected_capacity==99);
    CHECK(confirm_human_selection_fields(s,selection,0xffff)); CHECK(s.actors[4].action==6);
    CHECK(s.actors[0].scores[4]==13106); CHECK(s.actors[0].scores[0]==Word(-13106));
    CHECK(s.human_prepared_capacity==0);
});
group("human classification byte cycle all original bit patterns",[] {
    for (std::size_t target=0;target<4;++target) for (unsigned raw=0;raw<256;++raw) {
        State s; s.actors[4].classifications[target]=std::uint8_t(raw);
        State expected=s; unsigned decremented=(raw+255)%256;
        int signed_result=decremented<128?int(decremented):int(decremented)-256;
        auto wanted=std::uint8_t(signed_result<-2?2:decremented);
        expected.actors[4].classifications[target]=wanted;
        CHECK(cycle_human_classification(s,target)==wanted);
        for (unsigned a=0;a<5;++a) CHECK(encode_actor(s.actors[a])==encode_actor(expected.actors[a]));
    }
    State s; auto before=encode_actor(s.actors[4]);
    unsupported([&] { cycle_human_classification(s,4); }); CHECK(encode_actor(s.actors[4])==before);
});
group("human menu raw code mask all word values",[] {
    for (unsigned raw=0;raw<65536;++raw) {
        Word expected=0;
        if (raw!=65535 && raw%32==0) {
            unsigned item=(raw/32)%64;
            if (item==0) expected=1; if (item==1) expected=2;
        }
        CHECK(human_menu_control(Word(raw))==expected);
    }
    CHECK(human_menu_control(0)==1); CHECK(human_menu_control(32)==2);
    CHECK(human_menu_control(0x8000)==1); CHECK(human_menu_control(0xffff)==0);
});
group("human target fields all opponent city slots no invented population guard",[] {
    for (Word actor=0;actor<4;++actor) for (Word city=0;city<5;++city) {
        State s; s.actors[actor].population[city]=city%2==0?0:0xffff;
        s.actors[4].action=3; s.actors[4].magnitude=7; State expected=s;
        expected.actors[4].target_actor=actor; expected.actors[4].target_city=city;
        CHECK(select_human_target_fields(s,actor,city));
        for (unsigned a=0;a<5;++a) CHECK(encode_actor(s.actors[a])==encode_actor(expected.actors[a]));
    }
});
group("human target own-view rejection and invalid host indices",[] {
    State s; s.actors[4].target_actor=2; s.actors[4].target_city=3; auto before=encode_actor(s.actors[4]);
    CHECK(!select_human_target_fields(s,4,0xffff)); CHECK(encode_actor(s.actors[4])==before);
    unsupported([&] { select_human_target_fields(s,5,0); });
    unsupported([&] { select_human_target_fields(s,0,5); }); CHECK(encode_actor(s.actors[4])==before);
});
group("preparation handler messages selector and no inventory mutation",[] {
    for (Word action:{Word(2),Word(4),Word(6),Word(7)}) {
        State s; s.actors[0].action=action; s.actors[0].magnitude=99; s.actors[0].missiles[1]=4;
        auto actor=encode_actor(s.actors[0]); Rng r(1); auto result=initialize_preparation_handler(s,0,r);
        CHECK(result.effect==(action==2?16:255)); CHECK(result.draws==1);
        CHECK(result.message_variant==(action==6?4:3)); CHECK(r.seed()==0x41c67ea6);
        CHECK(encode_actor(s.actors[0])==actor);
    }
});
group("preparation action6 event5 and action7 event4 diplomacy tables",[] {
    for (Word action:{Word(6),Word(7)}) {
        State s; s.actors[0].action=action; s.actors[1].profile=1;
        s.actors[2].profile=2; s.actors[3].profile=3; Rng r(1);
        initialize_preparation_handler(s,0,r);
        CHECK(s.actors[1].scores[0]==Word(action==6?-4000:3000));
        CHECK(s.actors[2].scores[0]==Word(action==6?6000:-1000));
        CHECK(s.actors[3].scores[0]==Word(action==6?-2000:1000));
        CHECK(s.actors[4].scores[0]==0);
    }
});
group("preparation unsupported input and relationship failures preserve RNG",[] {
    State s; s.actors[0].action=6; s.actors[1].profile=99; State before=s; Rng r(1);
    unsupported([&] { initialize_preparation_handler(s,0,r); }); CHECK(r.seed()==1);
    for (unsigned a=0;a<5;++a) CHECK(encode_actor(s.actors[a])==encode_actor(before.actors[a]));
    s.actors[0].action=3; unsupported([&] { initialize_preparation_handler(s,0,r); }); CHECK(r.seed()==1);
    unsupported([&] { initialize_preparation_handler(s,5,r); }); CHECK(r.seed()==1);
});
group("resolver end AI production ordered actors excludes human and nonliving",[] {
    State s; for (auto& a:s.actors) { a.action=6; a.population[0]=1; }
    s.actors[2].population[0]=0xffff; s.actors[3].action=3;
    auto before_human=encode_actor(s.actors[4]); Rng r(1); auto report=finish_ai_production(s,r);
    CHECK(report.draws==8); CHECK(r.seed()==0x9cfbae39);
    CHECK(report.produced[0]); CHECK(report.produced[1]); CHECK(!report.produced[2]); CHECK(!report.produced[3]);
    for (unsigned actor=0;actor<2;++actor) {
        CHECK(report.production[actor].draws==4); CHECK(s.actors[actor].warheads[0]==1);
        CHECK(s.actors[actor].warheads[1]==1); CHECK(s.actors[actor].action==6);
    }
    CHECK(s.actors[2].warheads[0]==0); CHECK(s.actors[3].warheads[0]==0);
    CHECK(encode_actor(s.actors[4])==before_human);
});
group("resolver end no AI production consumes no RNG",[] {
    State s; s.actors[4].action=6; s.actors[4].population[0]=1; Rng r(1);
    auto result=finish_ai_production(s,r); CHECK(result.draws==0); CHECK(r.seed()==1);
    for (auto produced:result.produced) CHECK(!produced);
});
group("resolver actor pass eligibility exact living processed matrix",[] {
    for (unsigned mask=0;mask<32;++mask) for (std::size_t actor=0;actor<5;++actor)
    for (Word pass:{Word(0),Word(1),Word(2),Word(0xffff)}) for (Word processed:{Word(0),Word(1),Word(0xffff)}) {
        State s; unsigned cities=0;
        for (unsigned a=0;a<5;++a) {
            s.actors[a].population[0]=0xffff;
            if (mask&(1u<<a)) { s.actors[a].population[1]=1; ++cities; }
        }
        s.pass_processed[actor]=processed;
        bool alive=(mask&(1u<<actor))!=0;
        auto result=classify_resolver_actor(s,actor,pass); CHECK(result.total_cities==cities);
        auto expected=ResolverActorRoute::skip_actor;
        if (mask==0) expected=ResolverActorRoute::no_cities;
        else if (pass==0 && alive) expected=ResolverActorRoute::ordinary_action;
        else if (pass!=0 && !alive && processed==0) expected=ResolverActorRoute::followup_requires_evidence;
        CHECK(result.route==expected); CHECK(result.pass==(mask==0?1:pass));
        CHECK(result.first_followup_index==(expected==ResolverActorRoute::ordinary_action?4:0));
    }
});
group("resolver actor pass city sum and invalid host index",[] {
    State s; for (auto& a:s.actors) a.population={1,1,1,1,1};
    auto result=classify_resolver_actor(s,4,0); CHECK(result.total_cities==25);
    CHECK(result.route==ResolverActorRoute::ordinary_action); CHECK(s.pass_processed[4]==0);
    unsupported([&] { classify_resolver_actor(s,5,0); });
});
group("resolver pass exit signed word comparisons",[] {
    for (unsigned raw=0;raw<65536;++raw)
        CHECK(resolver_pass_has_finished(Word(raw))==(raw>=2 && raw<0x8000));
});
group("resolver pass abort repeat and wrapped advance ordering",[] {
    for (Word pass:{Word(0),Word(1),Word(0xffff),Word(0x8000)})
    for (Word repeat:{Word(0),Word(1),Word(0xffff)}) for (Word abort:{Word(0),Word(1),Word(2),Word(0xffff)}) {
        auto result=advance_resolver_pass(pass,repeat,abort);
        if (abort!=0) {
            CHECK(result.route==ResolverPassRoute::production_phase); CHECK(result.pass==pass); CHECK(result.control==abort);
        } else if (repeat!=0) {
            CHECK(result.route==ResolverPassRoute::repeat_pass); CHECK(result.pass==pass); CHECK(result.control==0);
        } else {
            CHECK(result.pass==add_word(pass,1)); CHECK(result.control==0);
            CHECK(result.route==(pass==1?ResolverPassRoute::production_phase:ResolverPassRoute::begin_next_pass));
        }
    }
    unsupported([] { advance_resolver_pass(2,0,0); });
    unsupported([] { advance_resolver_pass(0x7fff,1,1); });
});
group("message sequence compressed cursor initial and subsequent empty text",[] {
    MessageSequenceRng cursor{true}; Rng r(1);
    auto first=advance_message_sequence(cursor,"",r);
    CHECK(first.draws==0); CHECK(!cursor.at_initial_origin); CHECK(r.seed()==1);
    auto second=advance_message_sequence(cursor,"",r); CHECK(second.draws==128);
    Rng reference(1); for (unsigned i=0;i<128;++i) reference.next(); CHECK(r.seed()==reference.seed());
    auto third=advance_message_sequence(cursor," ",r); CHECK(third.draws==128);
    for (unsigned i=0;i<128;++i) reference.next(); CHECK(r.seed()==reference.seed());
});
group("message sequence nonspace calls and persisted post-message cursor",[] {
    MessageSequenceRng cursor{true}; Rng r(1),reference(1);
    auto first=advance_message_sequence(cursor,"A B",r); CHECK(first.noise_calls==2); CHECK(first.draws==256);
    auto next=advance_message_sequence(cursor,"C",r); CHECK(next.noise_calls==2); CHECK(next.draws==256);
    for (unsigned i=0;i<512;++i) reference.next(); CHECK(r.seed()==reference.seed());
    MessageSequenceRng resumed{false}; Rng continued(1); auto result=advance_message_sequence(resumed,"C",continued);
    CHECK(result.draws==256); CHECK(!resumed.at_initial_origin);
});
group("message sequence unsupported length keeps RNG and cursor",[] {
    MessageSequenceRng cursor{true}; Rng r(1);
    unsupported([&] { advance_message_sequence(cursor,std::string(32768,'A'),r); });
    CHECK(cursor.at_initial_origin); CHECK(r.seed()==1);
});
group("human production exact original summary text order and singular spaces",[] {
    Production p; p.missiles[0]=1; p.warheads[0]=1; p.bombers={1,1}; p.defenses={1,1};
    const std::vector<std::string> expected={"01  10MT Missile ","01  10MT Warhead ",
        "01  NP-1 Bomber ","01  L.N.D.S.  ","01  GR-2 Bomber ","01  MegaCannon  "};
    CHECK(production_summary_messages(p)==expected);
    p={}; p.bombers[0]=2; p.missiles[3]=2; p.warheads[3]=2;
    const std::vector<std::string> plural={"02  NP-1 Bombers","02 100MT Missiles","02 100MT Warheads"};
    CHECK(production_summary_messages(p)==plural);
    p={}; p.defenses={2,2};
    CHECK((production_summary_messages(p)==std::vector<std::string>{"02  L.N.D.S.'s","02  MegaCannon's"}));
    p.defenses[0]=7; unsupported([&] { production_summary_messages(p); });
});
group("human production header text changes RNG before inventory golden cases",[] {
    struct C { bool fresh; unsigned cities; std::array<Word,4> w,m; std::array<Word,2> b,d;
        unsigned direct,total,seed; };
    const C cases[]={
        {true,0,{0,0,1,0},{0,0,0,0},{0,0},{0,0},3,3971,0x64019314},
        {true,1,{0,1,1,0},{0,0,0,0},{0,0},{0,0},5,5765,0xf14e0eb2},
        {true,5,{1,1,2,0},{0,0,2,0},{0,0},{0,0},15,9615,0x66524d98},
        {false,0,{0,0,1,0},{0,0,0,0},{0,0},{0,0},3,4099,0xf5421494},
        {false,1,{0,1,1,0},{0,0,0,0},{0,0},{0,0},5,5893,0xa7742c32},
        {false,5,{0,2,3,0},{0,0,0,0},{0,0},{0,1},13,7821,0xa7519a0a}
    };
    for (auto c:cases) {
        State s; auto& a=s.actors[4]; a.action=6; a.magnitude=99; a.target_actor=2; a.target_city=3;
        for (unsigned city=0;city<c.cities;++city) a.population[city]=1;
        State expected=s; expected.actors[4].warheads=c.w; expected.actors[4].missiles=c.m;
        expected.actors[4].bombers=c.b; expected.actors[4].defenses=c.d;
        MessageSequenceRng cursor{c.fresh}; Rng r(1); auto result=produce_human(s,cursor,r);
        CHECK(result.inventory.warheads==c.w); CHECK(result.inventory.missiles==c.m);
        CHECK(result.inventory.bombers==c.b); CHECK(result.inventory.defenses==c.d);
        CHECK(result.inventory.human_message_variant==3); CHECK(result.inventory.draws==c.direct);
        CHECK(result.total_draws==c.total); CHECK(r.seed()==c.seed); CHECK(!cursor.at_initial_origin);
        CHECK(result.messages[0]=="Your citizens made:");
        for (unsigned actor=0;actor<5;++actor) CHECK(encode_actor(s.actors[actor])==encode_actor(expected.actors[actor]));
    }
});
group("human production text variants use duplicate first two original strings",[] {
    bool seen[3]{};
    for (unsigned seed=0;seed<20;++seed) {
        State s; MessageSequenceRng cursor{true}; Rng r(seed),reference(seed);
        Word variant=Word(reference.next()%3+1); auto result=produce_human(s,cursor,r);
        CHECK(result.inventory.human_message_variant==variant);
        CHECK(result.messages[0]==(variant==3?"Your citizens made:":"Your factories produced:"));
        CHECK(result.messages.size()>=2); seen[variant-1]=true;
    }
    for (bool present:seen) CHECK(present);
});
group("human production wraps inventory and requires explicit persistent context",[] {
    State s; s.actors[4].warheads[2]=0xffff; MessageSequenceRng cursor{true}; Rng r(1);
    auto report=produce_human(s,cursor,r); CHECK(s.actors[4].warheads[2]==0);
    CHECK(report.total_draws==3971); CHECK(r.seed()==0x64019314);
    auto before=encode_actor(s.actors[4]); auto seed=r.seed();
    unsupported([&] { produce(s,4,r); }); CHECK(encode_actor(s.actors[4])==before); CHECK(r.seed()==seed);
});
group("human entry prefix exact four draws two status refreshes and blank",[] {
    for (bool fresh:{false,true}) {
        State s; for (auto& a:s.actors) a.population[0]=1;
        State before=s; MessageSequenceRng cursor{fresh}; Rng r(1),reference(1);
        auto result=prepare_human_entry_prefix(s,cursor,r);
        const std::array<Word,4> rolls={40,88,56,75}; CHECK(result.entry_rolls==rolls);
        CHECK(result.first_refresh.draws==8); CHECK(result.second_refresh.draws==8);
        CHECK(result.empty_message.draws==(fresh?0:128)); CHECK(!result.produced);
        CHECK(result.total_draws==(fresh?20:148)); CHECK(!cursor.at_initial_origin);
        for (unsigned n=0;n<result.total_draws;++n) reference.next(); CHECK(r.seed()==reference.seed());
        for (unsigned actor=0;actor<5;++actor) CHECK(encode_actor(s.actors[actor])==encode_actor(before.actors[actor]));
    }
});
group("human entry oldaction6 produces after second refresh with resumed message",[] {
    State s; for (auto& a:s.actors) a.population[0]=1; s.actors[4].action=6;
    State expected=s; Rng r(1),reference(1); MessageSequenceRng cursor{true},reference_cursor{true};
    for (unsigned n=0;n<20;++n) reference.next(); reference_cursor.at_initial_origin=false;
    auto production=produce_human(expected,reference_cursor,reference);
    auto result=prepare_human_entry_prefix(s,cursor,r);
    CHECK(result.produced); CHECK(result.total_draws==20+production.total_draws);
    CHECK(result.production.messages==production.messages); CHECK(r.seed()==reference.seed());
    CHECK(s.actors[4].action==6); CHECK(!cursor.at_initial_origin);
    for (unsigned actor=0;actor<5;++actor) CHECK(encode_actor(s.actors[actor])==encode_actor(expected.actors[actor]));
});
group("human entry prefix dead and hostile AI status RNG filter preserved",[] {
    State s; s.actors[0].population[0]=1; s.actors[0].scores[4]=Word(-18001);
    s.actors[1].population[0]=0xffff; s.actors[1].profile=4; s.actors[1].scores[4]=Word(-18001);
    MessageSequenceRng cursor{true}; Rng r(1); auto result=prepare_human_entry_prefix(s,cursor,r);
    CHECK(result.first_refresh.draws==0); CHECK(result.second_refresh.draws==0);
    CHECK(result.total_draws==4); CHECK(r.seed()==0xc46b9b3d); CHECK(!cursor.at_initial_origin);
});
group("16-bit arithmetic / signed comparisons",[] {
    CHECK(signed_word(0)==0); CHECK(signed_word(0x7fff)==32767);
    CHECK(signed_word(0x8000)==-32768); CHECK(signed_word(0xffff)==-1);
    CHECK(add_word(0xffff,1)==0); CHECK(add_word(0x7fff,1)==0x8000);
    CHECK(sub_word(0,1)==0xffff); CHECK(sub_word(0x8000,2)==0x7ffe);
});
group("all actor bytes round-trip, explicit known offsets",[] {
    ActorImage image{};
    for (std::size_t i=0;i<image.size();++i) image[i]=std::uint8_t(i*37+19);
    auto a=decode_actor(image); CHECK(encode_actor(a)==image);
    a={}; a.profile=0x1234; a.action=0x5678; a.target_actor=0x9abc; a.target_city=0xdef0;
    a.unknown_longs[4]=0x10203040; a.classifications[4]=0xaa; a.unknown_21=0xbb;
    a.scores[4]=0x8001; a.city_x[0]=0x1122; a.city_y[0]=0x3344;
    a.population[0]=0x5566; a.population[4]=0x7788; a.unknown_4a[19]=0xcc;
    a.missiles[0]=0x1112; a.warheads[0]=0x2122; a.bombers[0]=0x3132;
    a.defenses[0]=0x4142; a.magnitude=0x5152; a.tally=0x6162;
    auto b=encode_actor(a);
    CHECK(b[0]==0x12 && b[1]==0x34 && b[2]==0x56 && b[7]==0xf0);
    CHECK(b[0x18]==0x10 && b[0x1b]==0x40 && b[0x20]==0xaa && b[0x21]==0xbb);
    CHECK(b[0x2a]==0x80 && b[0x2b]==1 && b[0x2c]==0x11 && b[0x36]==0x33);
    CHECK(b[0x40]==0x55 && b[0x48]==0x77 && b[0x5d]==0xcc);
    CHECK(b[0x5e]==0x11 && b[0x66]==0x21 && b[0x6e]==0x31 && b[0x72]==0x41);
    CHECK(b[0x76]==0x51 && b[0x78]==0x61 && b[0x79]==0x62);
});
group("RNG 11aae0 seed and result golden vectors",[] {
    Rng r(1);
    constexpr Word results[]{16838,5758,10113,17515,31051,5627,23010,7419,16212,4086,2749,12767};
    constexpr std::uint32_t states[]{0x41c67ea6,0x967eb0e7,0x2781e494,0xc46b9b3d,
        0xf94bdf32,0x95fb7483,0xd9e2b600,0x9cfbae39,0xbf54bc7e,0x0ff6d5df,0x0abd322c,0x31dff4f5};
    for (unsigned i=0;i<12;++i) { CHECK(r.next()==results[i]); CHECK(r.seed()==states[i]); }
    Rng zero(0); CHECK(zero.next()==0); CHECK(zero.seed()==0x3039);
    Rng high(0x80000000); CHECK(high.next()==0); CHECK(high.seed()==0x80003039);
    Rng max(0xffffffff); CHECK(max.next()==15929); CHECK(max.seed()==0xbe39e1cc);
    max.reseed_word(65535); CHECK(max.seed()==65535);
    max.reseed_word(0); CHECK(max.seed()==0 && max.next()==0 && max.seed()==0x3039);
});
group("action3 decision all event rolls and threshold boundaries",[] {
    struct Band { Word first,last,action; };
    constexpr Band bands[]{{1,4,11},{5,6,12},{7,8,16},{9,13,9},{14,18,13},
        {19,20,0x56},{21,23,15},{24,25,14},{26,100,8}};
    for (auto band:bands) {
        CHECK(action3_event_for_roll(band.first)==band.action);
        CHECK(action3_event_for_roll(band.last)==band.action);
        unsigned hits=0;
        for (Word roll=1;roll<=100;++roll) if (action3_event_for_roll(roll)==band.action) ++hits;
        CHECK(hits==unsigned(band.last-band.first+1));
        for (Word roll=band.first;roll<=band.last;++roll) CHECK(action3_event_for_roll(roll)==band.action);
    }
    unsupported([] { action3_event_for_roll(0); });
    unsupported([] { action3_event_for_roll(101); });
    unsupported([] { action3_event_for_roll(0xffff); });
});
group("action3 decision equality gate preserves other word actions without draws",[] {
    for (Word bits:{Word(0),Word(1),Word(2),Word(4),Word(5),Word(6),Word(7),Word(8),
                   Word(14),Word(16),Word(0x56),Word(0x8000),Word(0xffff)}) {
        Rng r(1); auto result=decide_action3_substitution(Word(bits),r);
        CHECK(result.selected_action==bits && !result.substituted);
        CHECK(result.gate_roll==0 && result.event_roll==0 && result.draws==0 && r.seed()==1);
    }
});
group("action3 gate29 passes while30 and100 stop after one draw",[] {
    Rng pass(61); auto result=decide_action3_substitution(3,pass);
    CHECK(result.gate_roll==29 && result.event_roll==67 && result.selected_action==8);
    CHECK(result.substituted && result.draws==2 && pass.seed()==0x349ae183);
    Rng thirty(30); result=decide_action3_substitution(3,thirty);
    CHECK(result.gate_roll==30 && result.event_roll==0 && result.selected_action==3);
    CHECK(!result.substituted && result.draws==1 && thirty.seed()==0xb53d60ff);
    CHECK(thirty.next()==8938); // second draw was not consumed
    Rng hundred(194); result=decide_action3_substitution(3,hundred);
    CHECK(result.gate_roll==100 && result.event_roll==0 && result.selected_action==3);
    CHECK(!result.substituted && result.draws==1 && hundred.seed()==0xd8479ed3);
});
group("action3 RNG fixtures cover all nine selected actions with exactly two draws",[] {
    struct Case { std::uint32_t seed; Word gate,event,action,next; std::uint32_t end; };
    constexpr Case cases[]{
        {0,1,69,8,9988,0xd3dc167e},{2,9,18,13,10239,0x59214b50},
        {19,17,2,11,12377,0x45ed8c49},{55,4,20,0x56,16903,0xa4cb430d},
        {79,7,10,9,19921,0xe409bce5},{123,28,25,14,25453,0x57fc46f1},
        {148,2,6,12,28597,0x59dd5b32},{249,3,21,15,8528,0x2404469f},
        {262,25,7,16,10163,0x06461df4}
    };
    for (auto c:cases) {
        Rng r(c.seed); auto result=decide_action3_substitution(3,r);
        CHECK(result.gate_roll==c.gate && result.event_roll==c.event);
        CHECK(result.selected_action==c.action && result.substituted && result.draws==2);
        CHECK(r.seed()==c.end); CHECK(r.next()==c.next);
    }
});
group("action3 decision leaves actor state untouched including setup branches",[] {
    for (std::uint32_t seed:{55u,123u}) {
        ActorImage image{};
        for (std::size_t i=0;i<image.size();++i) image[i]=std::uint8_t(i*37+19);
        auto a=decode_actor(image); a.action=3; a.target_actor=0xffff; a.target_city=0xffff;
        a.population={}; auto before=encode_actor(a); Rng r(seed);
        auto result=decide_action3_substitution(a.action,r);
        CHECK(result.selected_action==(seed==55?0x56:14));
        CHECK(result.draws==2 && encode_actor(a)==before);
    }
});
group("initial population formulas and disabled-human draw suppression",[] {
    constexpr Word first[]{26,21,26,39,31};
    for (Word p=0;p<5;++p) { Rng r(1); CHECK(initial_city_population(p,false,0,r)==first[p]); CHECK(r.seed()==0x41c67ea6); }
    Rng r(1); CHECK(initial_city_population(65535,true,1,r)==0 && r.seed()==1);
    CHECK(initial_city_population(65535,true,0,r)==26 && r.seed()==0x41c67ea6);
    unsupported([&] { initial_city_population(5,false,0,r); }); CHECK(r.seed()==0x41c67ea6);
});
group("state initialization tables, profile inventories and preserved fields",[] {
    State s; Rng r(1);
    for (auto& a:s.actors) { a.unknown_21=0xab; a.unknown_4a[0]=0xcd; a.scores.fill(65535); a.action=100; }
    s.actors[1].profile=1; s.actors[2].profile=2; s.actors[3].profile=4; s.identity[2]=9;
    s.prepared_capacity.fill(0xdead); s.human_prepared_capacity=0xbeef;
    initialize_state(s,0,r);
    CHECK((s.actors[0].population==std::array<Word,5>{26,22,35,27,17}));
    CHECK(s.actors[0].tally==127 && s.remaining_total[0]==127);
    CHECK((s.actors[0].warheads==std::array<Word,4>{1,3,0,0}));
    CHECK((s.actors[1].warheads==std::array<Word,4>{1,2,2,0}));
    CHECK((s.actors[1].missiles==std::array<Word,4>{1,1,1,0}) && s.actors[1].bombers[0]==1);
    CHECK(s.actors[2].warheads[0]==0 && s.actors[2].missiles[0]==0);
    CHECK((s.actors[2].defenses==std::array<Word,2>{1,1}));
    CHECK((s.actors[3].warheads==std::array<Word,4>{2,2,0,0}));
    CHECK(s.actors[4].warheads[0]==1 && s.actors[4].missiles[0]==1 && s.actors[4].defenses[0]==1);
    for (std::size_t i=0;i<5;++i) {
        auto& a=s.actors[i];
        CHECK(a.unknown_21==0xab && a.unknown_4a[0]==0xcd);
        CHECK(a.action==0 && a.magnitude==0 && a.target_actor==0 && a.target_city==0);
        CHECK(s.prepared_capacity[i]==0xdead && s.pass_processed[i]==0);
        Word total=0;
        for (std::size_t c=0;c<5;++c) {
            CHECK(a.city_x[c]==evidence::coordinates[0][i][c]); CHECK(a.city_y[c]==evidence::coordinates[1][i][c]);
            CHECK(a.scores[c]==0 && a.unknown_longs[c]==0 && a.classifications[c]==0);
            total=add_word(total,a.population[c]);
        }
        CHECK(a.tally==total && s.remaining_total[i]==total);
    }
    CHECK(s.human_prepared_capacity==0xbeef);
    s.identity[2]=0; Rng disabled(1); initialize_state(s,1,disabled);
    CHECK(s.actors[2].warheads[0]==3 && s.actors[2].missiles[0]==3);
    CHECK(populated_cities(s.actors[4])==0 && s.actors[4].warheads[0]==0 && s.actors[4].defenses[0]==0);
    s.actors[0].profile=5; auto before=encode_actor(s.actors[0]);
    auto seed=disabled.seed(); unsupported([&] { initialize_state(s,0,disabled); });
    CHECK(encode_actor(s.actors[0])==before && disabled.seed()==seed);
});
group("profile3 initialization executes two six-item production calls",[] {
    State s; s.actors[0].profile=3; Rng r(1); initialize_state(s,0,r);
    CHECK((s.actors[0].population==std::array<Word,5>{39,9,14,16,2}));
    CHECK(s.actors[0].tally==80);
    // Golden inventory and final seed are listed with draw trace in reconstruction record.
    CHECK((s.actors[0].warheads==std::array<Word,4>{3,2,3,0}));
    CHECK((s.actors[0].missiles==std::array<Word,4>{0,0,1,1}));
    CHECK((s.actors[0].bombers==std::array<Word,2>{1,1}));
    CHECK(r.seed()==0x7a79cb24);
});
group("population and end-condition signed-word tests",[] {
    Actor a; a.population={0,1,0xffff,0x8000,0x7fff}; CHECK(populated_cities(a)==2);
    State s; CHECK(living_actors(s)==0); CHECK(end_condition(s));
    s.actors[4]=a; CHECK(living_actors(s)==1); CHECK(end_condition(s));
    s.actors[0].population[0]=1; CHECK(living_actors(s)==2); CHECK(!end_condition(s));
    CHECK(clamp_loss(30,20)==20); CHECK(clamp_loss(5,20)==5);
    CHECK(clamp_loss(0xffff,20)==0); CHECK(clamp_loss(2,0x8000)==0);
    CHECK(population_add(32767,1)==32768); CHECK(population_add(65535,2)==1);
    CHECK(population_subtract(0,1)==65535); CHECK(population_subtract(5,5)==0);
    a.magnitude=2; a.population[3]=32767; growth_commit(a,3); CHECK(a.population[3]==32769);
    unsupported([&] { growth_commit(a,5); });
});
group("capacity/effect class mappings and inventory searches",[] {
    constexpr Word cap[]{10,20,50,100},eff[]{2,5,10,25};
    for (Word i=0;i<4;++i) { CHECK(class_capacity(i)==cap[i]); CHECK(class_effect(i)==eff[i]); }
    CHECK(class_capacity(4)==0); CHECK(class_effect(0xffff)==0);
    Actor a; CHECK(highest_bomber(a)==-1); CHECK(highest_missile(a)==-1); CHECK(highest_warhead(a)==-1);
    a.missiles={1,0,1,0}; CHECK(highest_missile(a)==2);
    a.missiles[3]=0xffff; CHECK(highest_missile(a)==3); // TST != 0, not >0
    a.warheads={1,1,0,0}; CHECK(highest_warhead(a)==1);
    a.bombers={1,0x8000}; CHECK(highest_bomber(a)==1);
});
group("city choice profiles, ties, 255 sentinel and circular scan",[] {
    Actor a,t; Rng r(1); t.population={0,20,20,10,0xffff};
    a.profile=0; CHECK(choose_city(a,t,r)==3); CHECK(r.seed()==1);
    a.profile=1; CHECK(choose_city(a,t,r)==1);
    t.population={0,10,20,10,0}; a.profile=2; CHECK(choose_city(a,t,r)==3);
    t.population={0,300,400,0,0}; CHECK(choose_city(a,t,r)==0); // original sentinel quirk
    t.population={0,10,0,20,0}; a.profile=3;
    CHECK(choose_city(a,t,r)==3); // 16838%5+1=4, visits 1,3,1,3
    CHECK(r.seed()==0x41c67ea6);
    a.profile=4; Rng r2(1); CHECK(choose_city(a,t,r2)==3);
    t.population={}; Rng before(1); unsupported([&] { choose_city(a,t,before); }); CHECK(before.seed()==1);
    a.profile=0; CHECK(choose_city(a,t,r)==-1); a.profile=1; CHECK(choose_city(a,t,r)==-1);
    a.profile=2; CHECK(choose_city(a,t,r)==0); a.profile=0xffff; CHECK(choose_city(a,t,r)==-1);
});
group("AI probability tables: every cell, all 100 rolls",[] {
    for (Word row=0;row<5;++row) for (Word profile=0;profile<5;++profile) {
        CHECK(category_one_table[row][profile]==evidence::tables[0][row][profile]);
        CHECK(category_two_table[row][profile]==evidence::tables[1][row][profile]);
        CHECK(category_one_table[row][profile]+category_two_table[row][profile]==100);
        unsigned first=0,second=0;
        for (Word roll=1;roll<=100;++roll) {
            auto category=ai_category_for_roll(row,profile,roll);
            if (category==1) ++first; else if (category==2) ++second;
            CHECK(category==(roll<=evidence::tables[0][row][profile]?1:2));
        }
        CHECK(first==evidence::tables[0][row][profile]); CHECK(second==evidence::tables[1][row][profile]);
    }
    Rng r(1); CHECK(ai_category(0,0,r)==2); CHECK(r.seed()==0x41c67ea6);
    unsupported([&] { ai_category(5,0,r); }); CHECK(r.seed()==0x41c67ea6);
    unsupported([] { ai_category_for_roll(0,0,0); }); unsupported([] { ai_category_for_roll(0,0,101); });
});
group("relationship bucket exact signed boundaries",[] {
    CHECK(relationship_bucket(Word(-18001))==0xfffe); CHECK(relationship_bucket(Word(-18000))==0xffff);
    CHECK(relationship_bucket(Word(-9001))==0xffff); CHECK(relationship_bucket(Word(-9000))==0);
    CHECK(relationship_bucket(0)==0); CHECK(relationship_bucket(9000)==0);
    CHECK(relationship_bucket(9001)==1); CHECK(relationship_bucket(18000)==1);
    CHECK(relationship_bucket(18001)==2); CHECK(relationship_bucket(0x8000)==0xfffe);
});
group("attitude tiers, signed-byte conversion and all table cells",[] {
    CHECK(signed_byte(0x80)==-128 && signed_byte(0xfe)==-2 && signed_byte(0xff)==-1);
    CHECK(relationship_tier(Word(-9001))==0 && relationship_tier(Word(-9000))==1);
    CHECK(relationship_tier(9000)==1 && relationship_tier(9001)==2);
    for (Word t=0;t<3;++t) for (Word row=0;row<5;++row) for (Word p=0;p<5;++p)
        CHECK(attitude_table_value(t,sub_word(row,2),p)==evidence::attitude[t][row][p]);
    CHECK(attitude_table_value(3,0xffff,0xffff)==0);
    unsupported([] { attitude_table_value(0,3,0); });
    unsupported([] { attitude_table_value(0,0,5); });
});
group("attitude update low-word multiplication and longword clamps",[] {
    State s; auto& a=s.actors[0]; a.profile=4;
    a.scores={Word(-10000),Word(-10000),32760,0x8000,Word(-32760)};
    s.actors[1].classifications[0]=0xfe; s.actors[2].classifications[0]=2;
    s.actors[3].classifications[0]=0; s.actors[4].classifications[0]=0xff;
    auto human=encode_actor(s.actors[4]); update_attitudes(s);
    CHECK(a.scores[0]==Word(-10000)); // self score skipped
    CHECK(a.scores[1]==Word(-6608)); // 2000*100 -> low word3392 before signed addition
    CHECK(a.scores[2]==32767); CHECK(a.scores[3]==Word(-30768)); CHECK(a.scores[4]==0x8001);
    CHECK(encode_actor(s.actors[4])==human);
});
group("attitude classification refresh preserves diagonal-only carry",[] {
    State s;
    s.actors[0].scores[0]=Word(-10000); s.actors[1].scores[1]=0;
    s.actors[2].scores[2]=10000; s.actors[3].scores[3]=Word(-10000);
    update_attitudes(s);
    CHECK((s.actors[0].classifications==std::array<std::uint8_t,5>{254,254,254,254,254}));
    CHECK((s.actors[1].classifications==std::array<std::uint8_t,5>{254,255,255,255,255}));
    CHECK((s.actors[2].classifications==std::array<std::uint8_t,5>{255,255,0,0,0}));
    CHECK((s.actors[3].classifications==std::array<std::uint8_t,5>{0,0,0,254,254}));
    auto before=encode_actor(s.actors[0]); s.actors[4].classifications[0]=127;
    unsupported([&] { update_attitudes(s); }); CHECK(encode_actor(s.actors[0])==before);
});
group("relationship-event tables and multiplication boundaries",[] {
    for (Word e=0;e<6;++e) for (Word t=0;t<3;++t) for (Word p=0;p<5;++p) {
        auto raw=signed_word(evidence::events[e][t][p]);
        CHECK(event_score_delta(p,t,e,1)==Word(raw*(e==4?10:100)));
    }
    CHECK(event_score_delta(2,0,0,65535)==Word(-300)); // signed magnitude -1
    CHECK(event_score_delta(2,0,1,100)==Word(60000));
    CHECK(event_score_delta(2,2,4,0)==Word(-5000));
    CHECK(event_score_delta(2,2,4,65535)==Word(-5000)); // type4 ignores magnitude
    CHECK(event_score_delta(2,0,5,0)==6000); CHECK(event_score_delta(2,0,5,30000)==6000);
    unsupported([] { event_score_delta(0,0,6,1); });
    unsupported([] { event_score_delta(5,0,0,1); });
    unsupported([] { event_score_delta(0,3,0,1); });
});
group("relationship event observer exclusion and affected-actor promotion",[] {
    State s; s.actors[0].scores[4]=0; s.actors[1].scores[4]=0;
    s.actors[2].scores[4]=0; s.actors[3].scores[4]=0;
    auto human=encode_actor(s.actors[4]);
    apply_relationship_event(s,4,0,1,10);
    CHECK(s.actors[0].scores[4]==0); CHECK(s.actors[1].scores[4]==13000);
    CHECK(s.actors[2].scores[4]==0 && s.actors[3].scores[4]==0);
    CHECK(encode_actor(s.actors[4])==human);
    State attack; apply_relationship_event(attack,3,2,0,2);
    CHECK(attack.actors[0].scores[3]==9600); // promoted type3, 3*100*2 +9000
    CHECK(attack.actors[1].scores[3]==200 && attack.actors[2].scores[3]==200);
    CHECK(attack.actors[3].scores[3]==0); // acting AI observer excluded
});
group("direct relationship events retain NEG.W overflow and long clamps",[] {
    State s; s.actors[0].scores[4]=0x8000; s.actors[1].scores[4]=Word(-9001);
    s.actors[2].scores[4]=Word(-9000); s.actors[3].scores[4]=32760;
    apply_relationship_event(s,4,1,0xffff,100);
    CHECK(s.actors[0].scores[4]==0x8000); CHECK(s.actors[1].scores[4]==9001);
    // 4*100*100=40000 -> low signed word -25536; -9000-25536+9000=-25536.
    CHECK(s.actors[2].scores[4]==Word(-25536)); CHECK(s.actors[3].scores[4]==16224);
    s.actors[0].scores[4]=32700; apply_relationship_event(s,4,1,0xffff,1);
    CHECK(s.actors[0].scores[4]==32767);
    s.actors[0].profile=2; s.actors[0].scores[4]=Word(-32760);
    apply_relationship_event(s,4,4,0,1); CHECK(s.actors[0].scores[4]==0x8001);
    auto before=encode_actor(s.actors[0]); s.actors[1].profile=5;
    unsupported([&] { apply_relationship_event(s,4,0,0xffff,1); });
    CHECK(encode_actor(s.actors[0])==before);
});
group("profile3 perturbation alive filters, draw order and clamps",[] {
    constexpr std::uint32_t seeds[]{0,1,3,6,7};
    constexpr int deltas[]{-18000,9000,0,-9000,18000};
    for (std::size_t k=0;k<5;++k) {
        State fixture; fixture.actors[0].profile=3; fixture.actors[0].population[0]=1;
        fixture.actors[4].population[0]=1; Rng random(seeds[k]);
        perturb_profile3_scores(fixture,random); CHECK(fixture.actors[0].scores[4]==Word(deltas[k]));
    }
    State s; Rng r(1); for (auto& a:s.actors) a.population[0]=1;
    s.actors[0].profile=3; s.actors[3].population[0]=0xffff;
    s.actors[0].scores[0]=100; s.actors[0].scores[1]=30000; s.actors[0].scores[2]=Word(-30000);
    perturb_profile3_scores(s,r);
    CHECK(s.actors[0].scores[0]==100); CHECK(s.actors[0].scores[1]==32767);
    CHECK(s.actors[0].scores[2]==Word(-21000)); CHECK(s.actors[0].scores[3]==0 && s.actors[0].scores[4]==9000);
    CHECK(r.seed()==0x2781e494); CHECK(s.actors[1].scores[0]==0);
    s.actors[0].population[0]=0x8000; auto seed=r.seed(); perturb_profile3_scores(s,r); CHECK(r.seed()==seed);
    State negative; negative.actors[0].profile=3; negative.actors[0].population[0]=1;
    negative.actors[4].population[0]=1; negative.actors[0].scores[4]=Word(-30000); Rng zero(0);
    perturb_profile3_scores(negative,zero); CHECK(negative.actors[0].scores[4]==0x8001 && zero.seed()==0x3039);
});
group("propaganda plan retains two RNG draws",[] {
    State s; Rng r(1); s.actors[0].profile=1; s.actors[4].population={10,20,20,0,0};
    plan_propaganda(s,0,4,r);
    auto& a=s.actors[0]; CHECK(a.action==3 && a.target_actor==4 && a.target_city==1);
    CHECK(a.magnitude==9); CHECK(r.seed()==0x967eb0e7);
    s.actors[4].population={}; auto before=encode_actor(a);
    plan_propaganda(s,0,4,r); CHECK(encode_actor(a)==before); CHECK(r.seed()==0x967eb0e7);
    unsupported([&] { plan_propaganda(s,5,4,r); });
});
group("defense planning order, threat score and nonzero inventory",[] {
    State s; s.selected_opponent[2]=4; s.actors[4].action=1;
    s.actors[2].defenses={1,1}; CHECK(plan_defense(s,2));
    CHECK(s.actors[2].action==7 && s.actors[2].magnitude==0 && s.actors[2].defenses[0]==0);
    CHECK(plan_defense(s,2)); CHECK(s.actors[2].magnitude==1 && s.actors[2].defenses[1]==0);
    CHECK(!plan_defense(s,2));
    s.actors[2].defenses={1,1}; s.selected_opponent[2]=3; s.actors[3].action=2;
    CHECK(plan_defense(s,2)); CHECK(s.actors[2].magnitude==0);
    s.actors[3].action=1; auto before=encode_actor(s.actors[2]);
    CHECK(!plan_defense(s,2)); CHECK(encode_actor(s.actors[2])==before);
    s.actors[3].action=4; CHECK(plan_defense(s,2)); CHECK(s.actors[2].magnitude==1);
    s.actors[2].defenses={0,0xffff}; s.selected_opponent[2]=0;
    s.actors[0].action=5; s.actors[0].scores[2]=10;
    s.actors[1].population[0]=1; s.actors[1].scores[2]=11; s.actors[1].action=5;
    CHECK(plan_defense(s,2)); CHECK(s.actors[2].defenses[1]==0xfffe);
    CHECK(s.selected_opponent[2]==0); // local threat override does not rewrite selection
    s.selected_opponent[2]=5; unsupported([&] { plan_defense(s,2); });
});
group("opponent tie ordinal and second-best threshold",[] {
    State s; for (auto& a:s.actors) a.population[0]=1;
    Rng r(1); CHECK(select_opponent(s,0,r)==0); CHECK(s.selected_opponent[0]==3);
    CHECK(r.seed()==0x41c67ea6);
    s.actors[0].scores={0,20000,19999,0,0}; Rng second(1);
    CHECK(select_opponent(s,0,second)==2); CHECK(s.selected_opponent[0]==1); // roll39 <=51
    Rng next(1); (void)next.next(); // next roll59 chooses second despite max relationship +2
    CHECK(select_opponent(s,0,next)==2); CHECK(s.selected_opponent[0]==2);
    s.actors[0].scores[2]=19000; Rng capped(1); (void)capped.next();
    select_opponent(s,0,capped); CHECK(s.selected_opponent[0]==1); // raw gap1000 capped50, threshold100
    for (std::size_t i=2;i<5;++i) s.actors[i].population={};
    Rng only(1); select_opponent(s,0,only); CHECK(s.selected_opponent[0]==1 && only.seed()==1);
    s.actors[1].population={}; CHECK(select_opponent(s,0,only)==0); CHECK(s.selected_opponent[0]==0);
});
group("warhead fitting signed capacity and inventory nonzero",[] {
    Actor a; a.warheads={1,1,1,1};
    CHECK(fitting_warhead(a,9)==-1); CHECK(fitting_warhead(a,10)==0);
    CHECK(fitting_warhead(a,20)==1); CHECK(fitting_warhead(a,50)==2); CHECK(fitting_warhead(a,100)==3);
    CHECK(fitting_warhead(a,0xffff)==-1); a.warheads[3]=0; CHECK(fitting_warhead(a,100)==2);
    a.warheads[3]=0x8000; CHECK(fitting_warhead(a,100)==3);
});
group("AI weapon preparation keeps original capacity-index quirk",[] {
    State s; Rng r(1); auto& a=s.actors[0]; a.population[0]=1; s.actors[4].population[0]=1;
    a.warheads={0,0,0,3}; a.missiles={3,0,0,0};
    plan_weapons(s,0,4,r); CHECK(a.action==2 && a.missiles[0]==2 && s.prepared_capacity[0]==100);
    CHECK(a.warheads[3]==3); // capacity is indexed by warhead3, although missile0 was consumed
    plan_weapons(s,0,4,r); CHECK(a.action==1 && a.target_actor==4 && a.target_city==0 && a.magnitude==25);
    CHECK(a.warheads[3]==2 && s.prepared_capacity[0]==100 && r.seed()==1);
    a.action=0; a.warheads={5,0,0,0}; a.missiles={}; a.bombers={3,0};
    plan_weapons(s,0,4,r); CHECK(a.action==4 && a.bombers[0]==2 && s.prepared_capacity[0]==50);
    plan_weapons(s,0,4,r); CHECK(a.action==5 && a.warheads[0]==4 && s.prepared_capacity[0]==40 && a.magnitude==2);
    a.action=5; s.prepared_capacity[0]=0; plan_weapons(s,0,4,r);
    CHECK(a.action==5 && s.prepared_capacity[0]==65526); // zero allowed before subtraction
    plan_weapons(s,0,4,r); CHECK(a.action==6); // now capacity is signed negative
});
group("weapon fallback, empty warhead and defined bomber capacity",[] {
    State s; auto& a=s.actors[0]; a.population[0]=1; s.actors[4].population[0]=1; Rng r(1);
    a.action=2; s.prepared_capacity[0]=10; plan_weapons(s,0,4,r);
    CHECK(a.action==1 && a.magnitude==0); // no fitting warhead still stores attack action
    a.action=4; plan_weapons(s,0,4,r); CHECK(a.action==6);
    a.action=0; a.warheads={3,0,0,0}; a.bombers={3,0};
    plan_weapons(s,0,4,r); CHECK(a.action==6 && a.bombers[0]==3 && r.seed()==1);
    a.action=0; a.warheads={0,3,0,0};
    plan_weapons(s,0,4,r); CHECK(a.action==4 && a.bombers[0]==2 && s.prepared_capacity[0]==50 && r.seed()==1);
    a.action=0; a.warheads={0,0,0,5}; a.missiles={}; a.bombers={3,0};
    plan_weapons(s,0,4,r); CHECK(a.action==6); // largest100 does not fit bomber50; no smaller class>0
});
group("four-actor AI planner instruction-derived seed-one sequence",[] {
    State s; for (auto& a:s.actors) a.population[0]=1; Rng r(1);
    ai_turn(s,r);
    CHECK(s.selected_opponent[0]==3 && s.selected_opponent[1]==4 && s.selected_opponent[2]==0 && s.selected_opponent[3]==1);
    CHECK(s.actors[0].action==3 && s.actors[0].magnitude==6);
    CHECK(s.actors[1].action==3 && s.actors[1].magnitude==10);
    CHECK(s.actors[2].action==6 && s.actors[3].action==3);
    CHECK(r.seed()==0xaf1cf0fb); // 14th RNG draw
});
group("production inclusive branch boundaries",[] {
    for (Word k=1;k<=100;++k) for (Word c=1;c<=100;++c) {
        auto item=production_item(k,c);
        if (k<=55) { CHECK(item.kind==InventoryKind::Warhead); CHECK(item.index==std::size_t(c<=25?0:c<=65?1:c<=90?2:3)); }
        else if (k<=80) { CHECK(item.kind==InventoryKind::Missile); CHECK(item.index==std::size_t(c<=20?0:c<=55?1:c<=85?2:3)); }
        else if (k<=95) { CHECK(item.kind==InventoryKind::Bomber); CHECK(item.index==std::size_t(c>60)); }
        else { CHECK(item.kind==InventoryKind::Defense); CHECK(item.index==std::size_t(c>60)); }
    }
    unsupported([] { production_item(0,1); }); unsupported([] { production_item(1,101); });
});
group("production original draw order, counts and inventory wrap",[] {
    State s; Rng r(1); s.actors[0].warheads[1]=65535;
    auto p=produce(s,0,r); CHECK(p.draws==2 && p.warheads[1]==1);
    CHECK(s.actors[0].warheads[1]==0); CHECK(r.seed()==0x967eb0e7);
    Rng human(1); auto before_human=encode_actor(s.actors[4]);
    unsupported([&] { produce(s,4,human); });
    CHECK(encode_actor(s.actors[4])==before_human); CHECK(human.seed()==1);
    Rng six(1); s.actors[1].population={1,1,1,1,1}; p=produce(s,1,six);
    CHECK(p.draws==12); CHECK((p.warheads==std::array<Word,4>{2,2,2,0})); CHECK(six.seed()==0x31dff4f5);
    unsupported([&] { produce(s,5,six); }); CHECK(six.seed()==0x31dff4f5);
});
group("damage effect arithmetic including 16-bit overflow",[] {
    constexpr Word expected[]{0,0,8,11,10,10,12,15,20,20,30,0};
    for (Word e=2;e<=13;++e) {
        auto d=damage_effect(e,10,100,1,50,false); CHECK(d.loss==expected[e-2]);
    }
    CHECK(damage_effect(4,1,100,1,50,false).loss==0);
    CHECK(damage_effect(4,0x8000,32767,1,50,false).loss==32766);
    CHECK(damage_effect(5,32767,32767,1,50,false).loss==0);
    CHECK(damage_effect(8,32767,32767,1,50,false).loss==0);
    CHECK(damage_effect(9,32767,32767,1,50,false).loss==0);
    CHECK(damage_effect(10,32767,32767,1,50,false).loss==0);
    CHECK(damage_effect(11,20000,30000,1,50,false).loss==0);
    CHECK(damage_effect(12,100,30000,1,50,true).loss==24464); // low word of 90000
    CHECK(damage_effect(12,1,20000,1,50,true).loss==0); // low word 60000 is negative
    CHECK(damage_effect(9,20,22,1,50,false).loss==22);
    CHECK(damage_effect(6,10,0x8000,1,50,false).loss==0);
    unsupported([] { damage_effect(14,1,10,1,50,false); });
});
group("crash/interception action and capacity effects",[] {
    auto d=damage_effect(2,10,100,5,50,false); CHECK(d.loss==0 && d.action==0x58 && d.prepared_capacity==0);
    d=damage_effect(2,10,100,1,50,false); CHECK(d.action==1 && d.prepared_capacity==0);
    d=damage_effect(2,10,100,4,50,false); CHECK(d.loss==10 && d.action==4 && d.prepared_capacity==50);
    d=damage_effect(13,10,100,5,50,false); CHECK(d.loss==0 && d.action==0 && d.prepared_capacity==0);
    d=damage_effect(13,10,100,1,50,false); CHECK(d.action==1 && d.prepared_capacity==0);
    d=damage_effect(13,10,100,4,50,false); CHECK(d.loss==10 && d.action==4 && d.prepared_capacity==50);
});
group("action8 effect15 source loss / destination growth",[] {
    State s; auto& a=s.actors[0]; Rng r(1); a.profile=1; a.target_actor=4; a.target_city=3;
    a.magnitude=30; a.population={5,20,20,0,0}; s.actors[4].population[3]=32760;
    CHECK(defection_commit(s,0,r)==1); CHECK(a.population[1]==0 && a.magnitude==20);
    CHECK(s.actors[4].population[3]==32780); CHECK(r.seed()==1);
    a.target_actor=0; a.target_city=2; a.magnitude=10;
    CHECK(defection_commit(s,0,r)==2); CHECK(a.population[2]==20); // source/destination alias, ordered SUB/ADD
    a.target_city=5; unsupported([&] { defection_commit(s,0,r); });
});
group("alien prelude uses all three original RNG draws",[] {
    Actor a; Rng r(1); auto p=alien_prelude(a,r);
    CHECK(p.message_variant==1 && p.loss==9 && p.initial_x==164);
    CHECK(a.magnitude==9 && r.seed()==0x2781e494);
});
group("action16 effect45 doubles first largest positive city",[] {
    Actor a; a.population={0,20,20,10,0xffff}; CHECK(double_largest_city(a)==1);
    CHECK(a.population[1]==40 && a.population[2]==20 && a.magnitude==20 && a.action==0x16);
    a.population={20000,0,0,0,0}; double_largest_city(a); CHECK(a.population[0]==40000);
    a.population={0xffff,0x8000,0,0,0}; CHECK(double_largest_city(a)==0);
    CHECK(a.population[0]==0xffff && a.magnitude==0); // original zero sentinel
});
group("city removal clears own selected slot and preserves other actor bytes",[] {
    for (Word city=0;city<5;++city) {
        ActorImage image{};
        for (std::size_t i=0;i<image.size();++i) image[i]=std::uint8_t(i*37+19);
        Actor a=decode_actor(image); a.target_city=city; a.target_actor=65535;
        a.population={0,1,0x7fff,0x8000,0xffff}; a.magnitude=0xabcd;
        Actor expected=a; expected.action=0x11; expected.population[city]=0;
        Rng r(1); CHECK(city_removal_commit(a,r)==3);
        CHECK(encode_actor(a)==encode_actor(expected)); CHECK(r.seed()==0x41c67ea6);
    }
});
group("city removal message draw variants and invalid-slot rejection",[] {
    constexpr std::uint32_t seeds[]{0,5,1};
    for (Word message=1;message<=3;++message) {
        Actor a; a.target_city=4; Rng r(seeds[message-1]);
        CHECK(city_removal_commit(a,r)==message); CHECK(a.action==0x11 && a.magnitude==0);
    }
    for (Word invalid:{Word(5),Word(0xffff)}) {
        Actor a; a.target_city=invalid; a.action=99; a.population.fill(10);
        auto before=encode_actor(a); Rng r(1);
        unsupported([&] { city_removal_commit(a,r); });
        CHECK(encode_actor(a)==before && r.seed()==1);
    }
});
group("quake notice changes action only and preserves population bits",[] {
    ActorImage image{};
    for (std::size_t i=0;i<image.size();++i) image[i]=std::uint8_t(i*37+19);
    Actor a=decode_actor(image); a.population={0,1,0x7fff,0x8000,0xffff};
    a.target_actor=65535; a.target_city=65535; a.magnitude=0xabcd;
    Actor expected=a; expected.action=0x56; Rng r(1);
    CHECK(quake_effect_notice(a,r)==3); CHECK(encode_actor(a)==encode_actor(expected));
    CHECK(r.seed()==0x41c67ea6);
    constexpr std::uint32_t seeds[]{0,5,1};
    for (Word message=1;message<=3;++message) {
        Actor empty; Rng random(seeds[message-1]);
        CHECK(quake_effect_notice(empty,random)==message);
        CHECK(empty.action==0x56 && populated_cities(empty)==0 && empty.magnitude==0);
    }
});
group("city removal followed by quake preserves one draw per block",[] {
    Actor a; a.population.fill(10); a.target_city=2; a.magnitude=100; Rng r(1);
    CHECK(city_removal_commit(a,r)==3); CHECK(quake_effect_notice(a,r)==2);
    CHECK(r.seed()==0x967eb0e7 && a.action==0x56 && a.magnitude==100);
    CHECK((a.population==std::array<Word,5>{10,10,0,10,10}));
});
group("weight-drop effect clears own city and preserves all other fields",[] {
    constexpr Word populations[]{0,1,0x7fff,0x8000,0xffff};
    for (Word city=0;city<5;++city) {
        ActorImage image{};
        for (std::size_t i=0;i<image.size();++i) image[i]=std::uint8_t(i*17+7);
        Actor a=decode_actor(image); a.target_city=city; a.target_actor=4;
        a.population={populations[0],populations[1],populations[2],populations[3],populations[4]};
        a.magnitude=0xffff; auto expected=a; expected.action=0x12; expected.population[city]=0;
        Rng r(1); CHECK(weight_drop_commit(a,r)==3);
        CHECK(encode_actor(a)==encode_actor(expected)); CHECK(r.seed()==0x41c67ea6);
    }
    constexpr std::uint32_t seeds[]{0,5,1};
    for (Word message=1;message<=3;++message) {
        Actor a; Rng r(seeds[message-1]); CHECK(weight_drop_commit(a,r)==message);
        CHECK(a.action==0x12 && a.population[0]==0);
    }
    Actor a; a.target_city=5; Rng r(1); auto before=encode_actor(a);
    unsupported([&] { weight_drop_commit(a,r); }); CHECK(encode_actor(a)==before && r.seed()==1);
});
group("cow effect caps local loss without replacing acting magnitude",[] {
    State s; auto& a=s.actors[0]; a.target_actor=4; a.target_city=3; a.magnitude=30;
    a.population.fill(50); s.actors[4].population.fill(20);
    auto expected=s; expected.actors[0].action=0x13; expected.actors[4].population[3]=0;
    Rng r(1); auto result=cow_population_commit(s,0,r);
    CHECK(result.loss==20 && result.message_variant==4); CHECK(r.seed()==0x41c67ea6);
    CHECK(a.magnitude==30);
    for (std::size_t i=0;i<5;++i) CHECK(encode_actor(s.actors[i])==encode_actor(expected.actors[i]));
    a.target_actor=0; a.target_city=1; a.magnitude=10;
    result=cow_population_commit(s,0,r); CHECK(result.loss==10 && a.population[1]==40 && a.magnitude==10);
});
group("cow effect signed comparisons and SUB.W negative-loss behavior",[] {
    struct Case { Word population, magnitude, loss, remaining; };
    constexpr Case cases[]{
        {20,5,5,15},{20,20,20,0},{20,30,20,0},{0,10,0,0},
        {20,0xffff,0xffff,21},{0x8000,10,0x8000,0},
        {0xffff,0x8000,0x8000,0x7fff},{0x7fff,0x8000,0x8000,0xffff}
    };
    for (auto c:cases) {
        State s; s.actors[2].target_actor=4; s.actors[2].target_city=0; s.actors[2].magnitude=c.magnitude;
        s.actors[4].population[0]=c.population; Rng r(1);
        auto result=cow_population_commit(s,2,r);
        CHECK(result.loss==c.loss && s.actors[4].population[0]==c.remaining);
        CHECK(s.actors[2].magnitude==c.magnitude && s.actors[2].action==0x13);
    }
});
group("cow message divisor5 and invalid indices preserve state and RNG",[] {
    constexpr std::uint32_t seeds[]{0,6,3,1,7};
    for (Word message=1;message<=5;++message) {
        State s; Rng r(seeds[message-1]);
        CHECK(cow_population_commit(s,0,r).message_variant==message);
    }
    State s; s.actors[0].target_actor=5; Rng r(1); auto before=encode_actor(s.actors[0]);
    unsupported([&] { cow_population_commit(s,0,r); }); CHECK(encode_actor(s.actors[0])==before && r.seed()==1);
    s.actors[0].target_actor=4; s.actors[0].target_city=5; before=encode_actor(s.actors[0]);
    unsupported([&] { cow_population_commit(s,0,r); }); CHECK(encode_actor(s.actors[0])==before && r.seed()==1);
    unsupported([&] { cow_population_commit(s,5,r); }); CHECK(r.seed()==1);
});
group("transfer prelude restores source profile and forces destination profile1",[] {
    State s; auto& a=s.actors[0]; a.action=3; a.profile=0;
    a.population={50,0,10,0,20}; a.target_actor=2; a.target_city=4; a.magnitude=99;
    s.actors[3].population={10,40,40,0,20};
    s.prepared_capacity.fill(111); s.remaining_total.fill(222); s.human_prepared_capacity=333;
    auto expected=s; expected.actors[0].population[4]=1;
    expected.actors[0].action=14; expected.actors[0].target_actor=3;
    expected.actors[0].target_city=1; expected.actors[0].magnitude=19;
    Rng r(1); auto report=transfer_event_prelude(s,0,24,r);
    CHECK(report.source_city==4 && report.destination_actor==3 && report.destination_city==1);
    CHECK(report.magnitude==19 && report.draws==2 && report.candidate_draws==1);
    CHECK(r.seed()==0x967eb0e7 && a.profile==0);
    for (std::size_t i=0;i<5;++i) CHECK(encode_actor(s.actors[i])==encode_actor(expected.actors[i]));
    CHECK(s.prepared_capacity==expected.prepared_capacity && s.remaining_total==expected.remaining_total);
    CHECK(s.human_prepared_capacity==333);
    // Source chooses profile0's last positive slot, not temporary profile1's
    // maximum. Destination chooses first largest, not profile0's last slot.
    transfer_arrival_commit(s,0);
    CHECK(s.actors[3].population[1]==59 && a.population[4]==1 && a.action==0x14);
    CHECK(r.seed()==0x967eb0e7); // arrival consumes no RNG and does not deduct again
});
group("transfer original-profile source calls account for all five profiles",[] {
    struct Case { Word profile, city, magnitude; std::uint32_t draws,seed; };
    constexpr Case cases[]{
        {0,3,9,2,0x967eb0e7},{1,1,19,2,0x967eb0e7},{2,3,9,2,0x967eb0e7},
        {3,3,9,3,0x2781e494},{4,3,9,3,0x2781e494}
    };
    for (auto c:cases) {
        State s; s.actors[0].profile=c.profile; s.actors[0].action=3;
        s.actors[0].population={0,20,0,10,0}; s.actors[3].population={0,30,30,5,0};
        Rng r(1); auto report=transfer_event_prelude(s,0,25,r);
        CHECK(report.source_city==c.city && report.magnitude==c.magnitude);
        CHECK(report.draws==c.draws && report.candidate_draws==1 && r.seed()==c.seed);
        CHECK(s.actors[0].profile==c.profile && s.actors[0].target_city==1);
    }
});
group("transfer rejects repeated self RNG draws then scans recipients without RNG",[] {
    State s; s.actors[0].action=3; s.actors[0].population[0]=100;
    s.actors[2].population[4]=1; Rng r(50);
    auto report=transfer_event_prelude(s,0,24,r);
    // Magnitude draw22715 ->16; candidate draws22430,16275,21417 ->0,0,2.
    CHECK(report.magnitude==16 && s.actors[0].population[0]==84);
    CHECK(report.destination_actor==2 && report.destination_city==4);
    CHECK(report.draws==4 && report.candidate_draws==3 && r.seed()==0x53a9ed7e);
    State wrap; wrap.actors[0].action=3; wrap.actors[0].population[0]=10; Rng random(1);
    report=transfer_event_prelude(wrap,0,25,random);
    CHECK(report.destination_actor==0 && report.destination_city==0); // candidate3 ->dead4 ->living0
    CHECK(report.draws==2 && report.candidate_draws==1 && random.seed()==0x967eb0e7);
    CHECK(wrap.actors[0].population[0]==1 && report.magnitude==9);
    transfer_arrival_commit(wrap,0); CHECK(wrap.actors[0].population[0]==10);
});
group("transfer destination uses populations after deduction including human actor",[] {
    State s; s.actors[4].action=3; s.actors[4].profile=1;
    s.actors[4].population[2]=1; s.actors[0].population[1]=30; Rng r(1);
    auto report=transfer_event_prelude(s,4,24,r);
    CHECK(report.source_city==2 && report.magnitude==1 && s.actors[4].population[2]==0);
    CHECK(report.destination_actor==0 && report.destination_city==1); // candidate3, dead4, then0
    CHECK(report.draws==2 && r.seed()==0x967eb0e7 && s.actors[4].profile==1);
    transfer_arrival_commit(s,4); CHECK(s.actors[0].population[1]==31);
    State self; self.actors[0].action=3; self.actors[0].profile=1;
    self.actors[0].population={20,10,0,0,0}; Rng random(1);
    report=transfer_event_prelude(self,0,24,random);
    CHECK(report.source_city==0 && report.magnitude==19 && report.destination_actor==0);
    CHECK(report.destination_city==1 && self.actors[0].population[0]==1);
    transfer_arrival_commit(self,0);
    CHECK((self.actors[0].population==std::array<Word,5>{1,29,0,0,0}));
    CHECK(report.draws==2 && random.seed()==0x967eb0e7);
});
group("transfer DIVS remainder and magnitude word preserve sentinel source cases",[] {
    State s; s.actors[0].profile=2; s.actors[0].action=3;
    s.actors[0].population={0xffff,300,400,0,0}; s.actors[3].population[0]=10; Rng r(1);
    auto report=transfer_event_prelude(s,0,24,r);
    CHECK(report.source_city==0 && report.magnitude==1 && s.actors[0].population[0]==0xfffe);
    CHECK(report.draws==2 && r.seed()==0x967eb0e7);
    State extreme; extreme.actors[0].profile=2; extreme.actors[0].action=3;
    extreme.actors[0].population={0x8000,300,400,0,0}; extreme.actors[3].population[0]=10;
    Rng maximum(24884); report=transfer_event_prelude(extreme,0,25,maximum);
    CHECK(report.source_city==0 && report.magnitude==0x8000 && extreme.actors[0].population[0]==0);
    CHECK(report.draws==2 && maximum.seed()==0xf9653cd2);
});
group("transfer unsupported traps and nontermination roll back state and RNG",[] {
    auto rejects=[](State s,Word roll) {
        auto before=s; Rng r(1);
        unsupported([&] { transfer_event_prelude(s,0,roll,r); });
        for (std::size_t i=0;i<5;++i) CHECK(encode_actor(s.actors[i])==encode_actor(before.actors[i]));
        CHECK(r.seed()==1);
    };
    State s; s.actors[0].action=3; s.actors[0].profile=2;
    s.actors[0].population={0,300,400,0,0}; rejects(s,24); // default slot0 -> divide-by-zero
    s.actors[0].population={}; s.actors[0].profile=0; rejects(s,24); // source -1
    s.actors[0].profile=3; rejects(s,25); // helper's original nonterminating source scan
    s.actors[0].profile=0; s.actors[0].population[0]=1; rejects(s,24); // deduction leaves no living actor
    s.actors[0].profile=5; rejects(s,25); // unknown source profile returns -1
    rejects(s,23); rejects(s,26);
    Rng r(1); unsupported([&] { transfer_event_prelude(s,5,24,r); }); CHECK(r.seed()==1);
});
group("transfer discarded fixed-profile scan is pure and roll24/25 results agree",[] {
    // The actual discarded scan reads DATA-tail words, not actor24/25. Its
    // result is dead. Any RAM contents produce only a local, with zero draws.
    for (auto values:{std::array<Word,5>{0,0,0,0,0},std::array<Word,5>{65535,32768,20,20,1}}) {
        Actor chooser,target; chooser.profile=1; target.population=values;
        auto before=encode_actor(target); Rng r(1); (void)choose_city(chooser,target,r);
        CHECK(encode_actor(target)==before && chooser.profile==1 && r.seed()==1);
    }
    State a; a.actors[0].action=3; a.actors[0].population[0]=20; a.actors[3].population[1]=10;
    auto b=a; Rng ra(1),rb(1);
    auto x=transfer_event_prelude(a,0,24,ra); auto y=transfer_event_prelude(b,0,25,rb);
    for (std::size_t i=0;i<5;++i) CHECK(encode_actor(a.actors[i])==encode_actor(b.actors[i]));
    CHECK(x.source_city==y.source_city && x.magnitude==y.magnitude && x.draws==y.draws && ra.seed()==rb.seed());
});
group("transfer seed123 instruction trace excludes two prior selection draws",[] {
    State s; s.actors[0].action=3; s.actors[0].population[0]=20; s.actors[3].population[0]=5;
    Rng r(123);
    CHECK(r.next()%100+1==28); // 102c2e: action3 substitution gate (<30)
    Word event=Word(r.next()%100+1); CHECK(event==25 && r.seed()==0x57fc46f1);
    auto report=transfer_event_prelude(s,0,event,r);
    CHECK(report.source_city==0 && report.magnitude==14 && s.actors[0].population[0]==6);
    CHECK(report.destination_actor==3 && report.destination_city==0 && report.draws==2);
    CHECK(report.candidate_draws==1 && r.seed()==0x33842957);
    transfer_arrival_commit(s,0);
    CHECK(s.actors[3].population[0]==19 && s.actors[0].action==0x14 && r.seed()==0x33842957);
});
group("transfer arrival adds to stored destination without a source deduction",[] {
    State s; auto& a=s.actors[1]; a.target_actor=4; a.target_city=2; a.magnitude=25;
    for (auto& actor:s.actors) actor.population.fill(50);
    s.prepared_capacity.fill(111); s.remaining_total.fill(222);
    auto expected=s; expected.actors[1].action=0x14; expected.actors[4].population[2]=75;
    transfer_arrival_commit(s,1);
    for (std::size_t i=0;i<5;++i) CHECK(encode_actor(s.actors[i])==encode_actor(expected.actors[i]));
    CHECK(s.prepared_capacity==expected.prepared_capacity && s.remaining_total==expected.remaining_total);
    a.target_actor=1; a.target_city=4; a.population[4]=32760;
    transfer_arrival_commit(s,1); CHECK(a.population[4]==32785 && a.magnitude==25 && a.action==0x14);
});
group("transfer arrival word addition preserves negative/overflow patterns",[] {
    struct Case { Word before, magnitude, after; };
    constexpr Case cases[]{{0,0,0},{0xffff,1,0},{32767,1,0x8000},
        {0,0xffff,0xffff},{10,0xffff,9},{0x8000,0x8000,0},{0xffff,0xffff,0xfffe}};
    for (auto c:cases) {
        State s; s.actors[3].target_actor=0; s.actors[3].target_city=1;
        s.actors[3].magnitude=c.magnitude; s.actors[0].population[1]=c.before;
        transfer_arrival_commit(s,3);
        CHECK(s.actors[0].population[1]==c.after && s.actors[3].magnitude==c.magnitude);
    }
});
group("transfer arrival invalid indices reject before state mutation",[] {
    State s; s.actors[0].target_actor=5; auto before=encode_actor(s.actors[0]);
    unsupported([&] { transfer_arrival_commit(s,0); }); CHECK(encode_actor(s.actors[0])==before);
    s.actors[0].target_actor=4; s.actors[0].target_city=5; before=encode_actor(s.actors[0]);
    unsupported([&] { transfer_arrival_commit(s,0); }); CHECK(encode_actor(s.actors[0])==before);
    unsupported([&] { transfer_arrival_commit(s,5); });
});
group("radiation five-city loss and total word accumulation",[] {
    Actor a; a.population={0,1,4,10,65535}; a.magnitude=123; a.tally=456; Rng r(1);
    radiation_population_commit(a,r);
    CHECK((a.population==std::array<Word,5>{0,0,0,6,0}));
    CHECK(a.magnitude==8 && a.action==0x15 && a.tally==456); // negative population contributes -1
    CHECK(r.seed()==0x41c67ea6);
    a.population.fill(0x8000); radiation_population_commit(a,r);
    CHECK(populated_cities(a)==0 && a.magnitude==0x8000); // five negative losses, low-word sum
    for (auto p:a.population) CHECK(p==0);
    a.population.fill(32767); Rng again(1); radiation_population_commit(a,again);
    CHECK(a.magnitude==20); for (auto p:a.population) CHECK(p==32763);
});
group("radiation all five draw outcomes and zero-city RNG consumption",[] {
    constexpr std::uint32_t seeds[]{0,6,3,1,7};
    for (Word loss=1;loss<=5;++loss) {
        Actor a; a.population={0,1,2,3,5}; Rng r(seeds[loss-1]);
        radiation_population_commit(a,r); Word total=0;
        constexpr Word before[]{0,1,2,3,5};
        for (std::size_t c=0;c<5;++c) {
            Word expected=before[c]>loss?Word(before[c]-loss):Word(0);
            CHECK(a.population[c]==expected); total=Word(total+before[c]-expected);
        }
        CHECK(a.magnitude==total && a.action==0x15);
    }
    Actor empty; Rng r(1); radiation_population_commit(empty,r);
    CHECK(empty.magnitude==0 && empty.action==0x15 && r.seed()==0x41c67ea6);
});
group("human commit resource and capacity transitions",[] {
    State s; auto& a=s.actors[4]; a.action=0; a.missiles[2]=3;
    human_commit(s,2,2,50); CHECK(a.action==2 && a.missiles[2]==2 && s.human_prepared_capacity==50);
    a.warheads[1]=1; human_commit(s,1,1,20);
    CHECK(a.action==1 && a.warheads[1]==0 && s.human_prepared_capacity==30);
    a.bombers[1]=1; human_commit(s,4,1,100); CHECK(a.bombers[1]==0 && s.human_prepared_capacity==100);
    a.warheads[3]=0; human_commit(s,5,3,100); CHECK(a.warheads[3]==65535 && s.human_prepared_capacity==0);
    a.defenses[0]=0; human_commit(s,7,0,0); CHECK(a.defenses[0]==65535);
    s.human_prepared_capacity=0; human_commit(s,1,0,1); CHECK(s.human_prepared_capacity==65535);
    auto before=encode_actor(a); unsupported([&] { human_commit(s,4,2,10); }); CHECK(encode_actor(a)==before);
    unsupported([&] { human_commit(s,8,0,0); });
});
group("human cancellation/production score penalty boundaries",[] {
    State s;
    for (auto& a:s.actors) a.scores={Word(-19960),Word(-19959),0,32767,19959};
    s.actors[4].action=6; human_commit(s,6,0,0);
    for (std::size_t i=0;i<4;++i) {
        CHECK(s.actors[i].scores[0]==0x8001); CHECK(s.actors[i].scores[1]==32471);
        CHECK(s.actors[i].scores[2]==Word(-13106)); CHECK(s.actors[i].scores[3]==19661);
        CHECK(s.actors[i].scores[4]==33065);
    }
    // Deliberately preserve ADD.W wrap at threshold 19959 + 13106 = 33065.
    CHECK(s.actors[4].scores[4]==19959);
    s.actors[0].scores[4]=19960; s.actors[4].action=2; human_commit(s,0,0,0);
    CHECK(s.actors[0].scores[4]==32767);
    s.actors[0].scores[4]=0; s.actors[4].action=4; human_commit(s,0,0,0);
    CHECK(s.actors[0].scores[4]==13106);
    s.actors[0].scores[4]=0; s.actors[4].action=2; human_commit(s,1,0,0);
    CHECK(s.actors[0].scores[4]==0);
});
group("complete action-handler dispatch and explicit unsupported turns",[] {
    constexpr std::uint32_t handlers[]{0,0x117576,0x1128e8,0x116c84,0x1169f6,0x1164dc,
        0x11729a,0x112c42,0x110cb2,0x1119b8,0,0x111f62,0x117c22,0x11145c,0x110f64,0x116f9a,0x117f48};
    for (Word i=0;i<17;++i) { CHECK(action_dispatch(i).handler==handlers[i]); CHECK(action_dispatch(i).action==i); }
    CHECK(action_dispatch(0x56).handler==0x11245e);
    CHECK(action_dispatch(0x16).handler==0); CHECK(action_dispatch(0x58).handler==0);
    CHECK(action_dispatch(0xffff).handler==0);
    State s; Rng r(1); s.actors[0].profile=5; auto before=encode_actor(s.actors[0]);
    unsupported([&] { resolve_action(s,0,r); }); unsupported([&] { human_turn(s); }); unsupported([&] { ai_turn(s,r); });
    CHECK(encode_actor(s.actors[0])==before && r.seed()==1);
});
group("prior human descriptions preserve duplicate and empty messages",[] {
    State s; std::array<std::string,5> names{"A","B","C","D","E"};
    MessageSequenceRng c{false}; Rng r(1); s.actors[4].action=20; s.actors[4].magnitude=Word(-32768);
    auto report=prior_human_messages(s,65535,names,c,r);
    CHECK(report.messages.size()==5);
    CHECK(report.messages[0]=="ejected -32768 million");
    CHECK(report.messages[2]=="Your Human Catapult");
    CHECK(report.messages[0]==report.messages[3]);
    CHECK(report.messages[1]==report.messages[4]);
    s.actors[4].action=88; c.at_initial_origin=false; r=Rng(1);
    report=prior_human_messages(s,65535,names,c,r);
    CHECK(report.messages.size()==3 && report.messages[2].empty());
    CHECK(report.draws==128*(17+19+3));
    Rng expected(1); for(unsigned i=0;i<report.draws;++i) expected.next();
    CHECK(r.seed()==expected.seed() && !c.at_initial_origin);
});
group("prior human signed formatting target names and transactional rejection",[] {
    State s; std::array<std::string,5> names{"A",std::string("B\0junk",6),"C","D","E"};
    MessageSequenceRng c{true}; Rng r(1); auto& h=s.actors[4];
    h.action=1; h.target_actor=1; h.magnitude=0;
    auto report=prior_human_messages(s,2,names,c,r);
    CHECK(report.messages==std::vector<std::string>({"The Deathbringer missile","exploded on B","No casualties."}));
    h.action=5; h.magnitude=65535; s.human_prepared_capacity=32768;
    report=prior_human_messages(s,65535,names,c,r);
    CHECK(report.messages[1]=="for -1 million:" && report.messages[2]=="-32768 megatons remain.");
    const auto seed=r.seed(); const auto context=c.at_initial_origin;
    h.action=1; unsupported([&] { prior_human_messages(s,65535,names,c,r); });
    CHECK(r.seed()==seed && c.at_initial_origin==context);
    h.target_actor=65535; unsupported([&] { prior_human_messages(s,0,names,c,r); });
    CHECK(r.seed()==seed && c.at_initial_origin==context);
    h.action=0; report=prior_human_messages(s,65535,names,c,r);
    CHECK(report.messages.empty() && report.draws==0 && r.seed()==seed);
});
group("all prior-action descriptions consume exact ordered message RNG",[] {
    constexpr Word actions[]{1,22,21,16,86,7,88,20,19,18,17,8,3,5,2,4};
    const std::array<std::string,5> names{"A","B","C","D","E"};
    for(auto action:actions) for(Word magnitude:{Word(0),Word(1),Word(65535)}) {
        State s; s.actors[4].action=action; s.actors[4].target_actor=0; s.actors[4].magnitude=magnitude;
        MessageSequenceRng context{true}, expected_context{true}; Rng rng(7), expected(7);
        auto report=prior_human_messages(s,0,names,context,rng); unsigned draws=0;
        for(const auto& text:report.messages) draws+=advance_message_sequence(expected_context,text,expected).draws;
        CHECK(!report.messages.empty() && report.draws==draws);
        CHECK(rng.seed()==expected.seed() && context.at_initial_origin==expected_context.at_initial_origin);
    }
});
group("human entry tail speech gate and unconditional target clear",[] {
    for(Word action:{Word(0),Word(1),Word(3),Word(5),Word(8),Word(88)})
    for(bool alive:{false,true}) {
        State s; s.actors[4].action=action; s.actors[4].target_actor=2;
        if(alive) s.actors[2].population[0]=1;
        // Hostile displayed bucket suppresses status RNG.
        s.actors[2].scores[4]=Word(-32767);
        Word welcome=0; MessageSequenceRng c{true}; Rng rng(1);
        auto report=prepare_human_entry_tail(s,welcome,c,rng);
        const bool spoke=alive && (action==1 || action==3 || action==5 || action==8);
        CHECK(s.actors[4].target_actor==65535 && welcome==0);
        CHECK(report.speech_roll==(spoke?Word(16838%69+1):Word(0)));
        CHECK(report.draws==(spoke?1u:0u) && report.messages.empty());
        CHECK(c.at_initial_origin && rng.seed()==(spoke?0x41c67ea6u:1u));
    }
});
group("human welcome original ordering and persistent message RNG",[] {
    State s; s.actors[4].target_actor=4; Word flag=7;
    MessageSequenceRng c{true}, expected_context{true}; Rng rng(1), expected(1);
    unsigned draws=advance_message_sequence(expected_context,"Welcome to Nuclear War.",expected).draws;
    const Word index=Word(expected.next()%48); ++draws;
    auto report=prepare_human_entry_tail(s,flag,c,rng);
    CHECK(report.welcome_index==index && report.messages.size()==3 && report.messages[2].empty());
    draws+=advance_message_sequence(expected_context,report.messages[1],expected).draws;
    draws+=advance_message_sequence(expected_context,"",expected).draws;
    CHECK(report.draws==draws && rng.seed()==expected.seed());
    CHECK(flag==0 && !c.at_initial_origin && s.actors[4].target_actor==65535);
    auto second=prepare_human_entry_tail(s,flag,c,rng);
    CHECK(second.draws==0 && second.messages.empty() && rng.seed()==expected.seed());
});
group("human tail invalid speech target rolls back third refresh",[] {
    State s; s.actors[0].population[0]=1; s.actors[4].action=1; s.actors[4].target_actor=65535;
    Word flag=1; MessageSequenceRng c{true}; Rng rng(1);
    unsupported([&] { prepare_human_entry_tail(s,flag,c,rng); });
    CHECK(rng.seed()==1 && c.at_initial_origin && flag==1 && s.actors[4].target_actor==65535);
});
group("complete human pre-input phase neutral and hostile status counts",[] {
    for(bool fresh:{false,true}) for(bool hostile:{false,true}) {
        State s; for(unsigned i=0;i<4;++i) {
            s.actors[i].population[0]=1;
            s.actors[i].scores[4]=hostile?Word(-32767):Word(0);
        }
        s.actors[4].target_actor=3; Word flag=0; MessageSequenceRng c{fresh}; Rng rng(1);
        const std::array<std::string,5> names{};
        auto report=prepare_human_entry(s,65535,names,flag,c,rng);
        const unsigned draws=4+(hostile?0:24)+(fresh?0:128);
        CHECK(report.draws==draws && report.prior.messages.empty());
        CHECK(s.actors[4].target_actor==65535 && !c.at_initial_origin);
        Rng expected(1); for(unsigned n=0;n<draws;++n) expected.next();
        CHECK(rng.seed()==expected.seed());
    }
});
group("complete human pre-input ordering and failure rollback",[] {
    State s; for(unsigned i=0;i<5;++i) s.actors[i].population[0]=10;
    s.actors[4].action=1; s.actors[4].target_actor=2; s.actors[4].magnitude=7;
    const std::array<std::string,5> names{"A","B","C","D","E"};
    auto expected=s; Word flag=1, expected_flag=1;
    MessageSequenceRng c{true}, expected_c{true}; Rng rng(3), expected_rng(3);
    auto p=prepare_human_entry_prefix(expected,expected_c,expected_rng);
    auto m=prior_human_messages(expected,2,names,expected_c,expected_rng);
    auto t=prepare_human_entry_tail(expected,expected_flag,expected_c,expected_rng);
    auto report=prepare_human_entry(s,2,names,flag,c,rng);
    CHECK(report.draws==p.total_draws+m.draws+t.draws);
    CHECK(report.prior.messages[1]=="exploded on C" && report.tail.speech_roll!=0);
    CHECK(rng.seed()==expected_rng.seed() && flag==expected_flag && c.at_initial_origin==expected_c.at_initial_origin);
    for(unsigned i=0;i<5;++i) CHECK(encode_actor(s.actors[i])==encode_actor(expected.actors[i]));
    s.actors[4].target_actor=2; const auto before=encode_actor(s.actors[4]); const auto seed=rng.seed();
    unsupported([&] { prepare_human_entry(s,65535,names,flag,c,rng); });
    CHECK(encode_actor(s.actors[4])==before && rng.seed()==seed);
});
group("human q rejection messages and exact noise draws",[] {
    for(bool fresh:{false,true}) for(Word pending:{Word(0),Word(1),Word(3),Word(5)}) {
        MessageSequenceRng c{fresh}; Rng rng(1); auto report=prepare_human_confirmation(pending,65535,c,rng);
        CHECK(!report.ready && report.messages.size()==3 && report.messages[2].empty());
        CHECK(report.messages[0]==(pending==0?"Select an action":"Choose a target city"));
        CHECK(report.messages[1]==(pending==0?"before ending turn.":"Before ending turn."));
        const unsigned characters=pending==0?31:34;
        CHECK(report.draws==128*(characters+(fresh?2:3)));
        Rng expected(1); for(unsigned n=0;n<report.draws;++n) expected.next();
        CHECK(rng.seed()==expected.seed() && !c.at_initial_origin);
    }
});
group("human q accepted guard does not prematurely commit or draw",[] {
    for(Word pending:{Word(1),Word(2),Word(3),Word(4),Word(5),Word(6),Word(7),Word(65535)}) {
        MessageSequenceRng c{true}; Rng rng(1);
        auto report=prepare_human_confirmation(pending,0,c,rng);
        CHECK(report.ready && report.messages.empty() && report.draws==0);
        CHECK(rng.seed()==1 && c.at_initial_origin);
    }
});
group("human p missing target retains pending3 and text RNG",[] {
    State s; s.actors[4].magnitude=99; HumanSelection fields;
    MessageSequenceRng c{true}; Rng rng(1); const std::array<std::string,5> names{};
    auto report=prepare_human_propaganda_command(s,fields,65535,names,c,rng);
    CHECK(!report.target_selected && fields.pending_action==3 && s.actors[4].magnitude==99);
    CHECK(report.messages==std::vector<std::string>({"Select a target city","for your propaganda.",""}));
    CHECK(report.draws==128*(17+18+2) && !c.at_initial_origin);
});
group("human p messages precede three direct rolls",[] {
    for(Word old:{Word(0),Word(2),Word(4)}) {
        State s; s.actors[4].action=old; HumanSelection fields;
        const std::array<std::string,5> names{"A","B","C","D","E"};
        MessageSequenceRng c{false}, expected_c{false}; Rng rng(1), expected(1);
        std::vector<std::string> text{"B targeted","for propoganada.",old==4?"This will junk a bomber":old==2?"This will junk a missile":""};
        unsigned draws=0; for(const auto& line:text) draws+=advance_message_sequence(expected_c,line,expected).draws;
        const Word cls=Word(expected.next()%4), discard=Word(expected.next()%100+1), magnitude=Word(expected.next()%10+1);
        auto report=prepare_human_propaganda_command(s,fields,1,names,c,rng);
        CHECK(report.target_selected && report.messages==text && report.draws==draws+3);
        CHECK(report.discarded_roll==discard && fields.selected_class==cls);
        CHECK(fields.selected_capacity==class_capacity(cls) && s.actors[4].magnitude==magnitude);
        CHECK(rng.seed()==expected.seed() && s.actors[4].action==old && fields.pending_action==3);
    }
    State s; HumanSelection fields; MessageSequenceRng c{true}; Rng rng(1);
    const std::array<std::string,5> names{};
    unsupported([&] { prepare_human_propaganda_command(s,fields,5,names,c,rng); });
    CHECK(fields.pending_action==0 && c.at_initial_origin && rng.seed()==1);
});
group("human s production selection preserves empty messages and warnings",[] {
    for(Word old:{Word(0),Word(2),Word(4),Word(6)}) for(bool fresh:{false,true}) {
        HumanSelection fields; fields.selected_class=2; fields.selected_capacity=50;
        MessageSequenceRng c{fresh}, expected_c{fresh}; Rng rng(1), expected(1);
        auto report=prepare_human_production_command(old,fields,c,rng);
        CHECK(fields.pending_action==6 && fields.selected_class==2 && fields.selected_capacity==50);
        CHECK(report.messages[0].empty() && report.messages[1].empty());
        CHECK(report.messages[2]=="Weapons will be built." && report.messages[3]=="Click on Nuclear war.");
        CHECK(report.messages.size()==((old==2 || old==4)?6u:5u));
        CHECK(report.messages.back()==(old==6?"NOTE: You're stockpiling":""));
        unsigned draws=0; for(const auto& text:report.messages) draws+=advance_message_sequence(expected_c,text,expected).draws;
        CHECK(report.draws==draws && rng.seed()==expected.seed() && !c.at_initial_origin);
    }
});
group("human q complete accepted field commit has zero added RNG",[] {
    State s; auto& h=s.actors[4]; h.action=2; h.warheads[1]=1; s.human_prepared_capacity=50;
    HumanSelection selection; selection.pending_action=1; selection.selected_class=1; selection.selected_capacity=20;
    MessageSequenceRng c{false}; Rng rng(17);
    auto report=confirm_human_command(s,selection,2,c,rng);
    CHECK(report.ready && report.draws==0 && rng.seed()==17 && !c.at_initial_origin);
    CHECK(h.action==1 && h.warheads[1]==0 && s.human_prepared_capacity==30);
    selection.pending_action=6; report=confirm_human_command(s,selection,65535,c,rng);
    CHECK(report.ready && h.action==6 && rng.seed()==17);
});
group("human q rejection and unsupported commit preserve resources",[] {
    State s; s.actors[4].action=2; s.actors[4].warheads[0]=4;
    HumanSelection fields; fields.pending_action=1; fields.selected_class=0; fields.selected_capacity=10;
    MessageSequenceRng c{true}; Rng rng(1); auto before=encode_actor(s.actors[4]);
    auto report=confirm_human_command(s,fields,65535,c,rng);
    CHECK(!report.ready && report.draws==4608 && encode_actor(s.actors[4])==before);
    const auto seed=rng.seed(); fields.pending_action=65535;
    unsupported([&] { confirm_human_command(s,fields,0,c,rng); });
    CHECK(encode_actor(s.actors[4])==before && rng.seed()==seed && !c.at_initial_origin);
});
group("human w accepted status and text RNG before resource confirmation",[] {
    for(Word old:{Word(2),Word(4),Word(5)}) {
        State s; s.actors[0].population[0]=20; s.actors[4].action=old;
        s.actors[4].warheads[1]=2; s.human_prepared_capacity=50;
        HumanSelection fields; MessageSequenceRng c{false}, expected_c{false}; Rng rng(1), expected(1);
        const std::array<std::string,5> names{"A","B","C","D","E"};
        const Word discarded=Word(expected.next()%4); auto refresh=attitude_refresh_rng(s,expected);
        const std::vector<std::string> text{"","A targeted",old==2?"by your missile.":"by your bomber:","20 megaton payload."};
        unsigned draws=1+refresh.draws;
        for(const auto& line:text) draws+=advance_message_sequence(expected_c,line,expected).draws;
        auto report=prepare_human_warhead_command(s,fields,2,0,names,c,rng);
        CHECK(report.check==HumanWarheadCheck::accepted && report.messages==text && report.discarded_roll==discarded);
        CHECK(report.draws==draws && rng.seed()==expected.seed());
        CHECK(fields.pending_action==(old==2?1:5) && s.actors[4].magnitude==5);
        CHECK(s.actors[4].action==old && s.actors[4].warheads[1]==2 && s.human_prepared_capacity==50);
        auto confirmation=confirm_human_command(s,fields,0,c,rng);
        CHECK(confirmation.ready && s.actors[4].warheads[1]==1 && s.human_prepared_capacity==30);
    }
});
group("human w rejection messages preserve partial selection writes",[] {
    State s; HumanSelection fields; fields.selected_class=3; fields.selected_capacity=100;
    MessageSequenceRng c{true}; Rng rng(1); const std::array<std::string,5> names{};
    auto report=prepare_human_warhead_command(s,fields,1,0,names,c,rng);
    CHECK(report.check==HumanWarheadCheck::no_warhead && report.messages[0]=="You have no warheads");
    CHECK(fields.selected_class==3 && fields.selected_capacity==100);
    report=prepare_human_warhead_command(s,fields,0,0,names,c,rng);
    CHECK(fields.selected_class==65535 && fields.selected_capacity==100 && report.messages[0]=="You have no warheads.");
    s.actors[4].warheads[0]=1; s.actors[4].action=4; s.human_prepared_capacity=0;
    report=prepare_human_warhead_command(s,fields,1,0,names,c,rng);
    CHECK(report.check==HumanWarheadCheck::no_capacity && report.messages.size()==3);
    CHECK(fields.selected_class==0 && fields.selected_capacity==10);
    s.human_prepared_capacity=65535;
    report=prepare_human_warhead_command(s,fields,1,0,names,c,rng);
    CHECK(report.check==HumanWarheadCheck::too_large && report.messages[2]=="drop -1 megatons.");
    s.actors[4].action=2;
    report=prepare_human_warhead_command(s,fields,1,0,names,c,rng);
    CHECK(report.messages[1]=="The missile can only" && report.messages[2]=="carry -1 megatons.");
    s.actors[4].action=0;
    report=prepare_human_warhead_command(s,fields,1,0,names,c,rng);
    CHECK(report.check==HumanWarheadCheck::no_prepared_delivery && report.messages.size()==2);
    s.actors[4].action=2; s.human_prepared_capacity=50;
    report=prepare_human_warhead_command(s,fields,1,65535,names,c,rng);
    CHECK(report.check==HumanWarheadCheck::no_target && report.messages.size()==1);
    CHECK(s.actors[4].magnitude==0 && s.actors[4].warheads[0]==1);
});
group("human m z l status placement and exact names",[] {
    for(Word action:{Word(2),Word(4),Word(7)}) {
        State s; s.actors[0].population[0]=20; auto& human=s.actors[4];
        human.action=4; human.magnitude=77; human.missiles[1]=2; human.bombers[1]=2; human.defenses[1]=2;
        HumanSelection fields; fields.selected_class=0; fields.selected_capacity=333;
        MessageSequenceRng c{false}, ec{false}; Rng rng(1), expected(1);
        const std::vector<std::string> text=action==2?std::vector<std::string>{"Gravedigger missile","20 megaton capacity.","This will junk a bomber"}:
            action==4?std::vector<std::string>{"Grim Reaper bomber","100 megaton capacity.","This will junk a bomber"}:
            std::vector<std::string>{"MegaCannon system","ready for action.",""};
        unsigned draws=0;
        if(action!=4) draws+=attitude_refresh_rng(s,expected).draws;
        for(const auto& line:text) draws+=advance_message_sequence(ec,line,expected).draws;
        if(action==4) draws+=attitude_refresh_rng(s,expected).draws;
        auto report=prepare_human_delivery_command(s,fields,action,2,c,rng);
        CHECK(report.accepted && report.messages==text && report.draws==draws);
        CHECK(rng.seed()==expected.seed() && fields.selected_class==1 && fields.pending_action==action);
        CHECK(fields.selected_capacity==(action==2?20:action==4?100:333));
        CHECK(human.magnitude==(action==2?2:action==4?77:1));
        CHECK(human.action==4 && human.missiles[1]==2 && human.bombers[1]==2 && human.defenses[1]==2);
    }
});
group("human delivery rejection priority exact messages and field preservation",[] {
    for(Word action:{Word(2),Word(4),Word(7)}) {
        State s; HumanSelection fields; MessageSequenceRng c{true}; Rng rng(1);
        auto report=prepare_human_delivery_command(s,fields,action,65535,c,rng);
        CHECK(!report.accepted && report.messages.size()==3 && fields.selected_class==65535);
        CHECK(report.refresh.draws==0);
        if(action==2) CHECK(report.messages[1]=="no missiles at this time.");
        if(action==4) CHECK(report.messages[0]=="Your arseneal contains");
        if(action==7) CHECK(report.messages[2]=="at this time.");
        fields.selected_class=0; s.actors[4].missiles[0]=1; s.actors[4].bombers[0]=1; s.actors[4].defenses[0]=1;
        report=prepare_human_delivery_command(s,fields,action,2,c,rng);
        CHECK(!report.accepted && fields.pending_action==0 && fields.selected_class==0);
        if(action==2) CHECK(report.messages[0]=="You have no missiles");
        if(action==4) CHECK(report.messages[0]=="Your have no bombers");
        if(action==7) CHECK(report.messages[1]=="no defensive weaponry.");
    }
});
group("human delivery auto signed quirk and unsupported rollback",[] {
    State s; s.actors[4].defenses={0,65535}; HumanSelection fields;
    MessageSequenceRng c{true}; Rng rng(1);
    auto report=prepare_human_delivery_command(s,fields,7,0,c,rng);
    CHECK(report.accepted && fields.selected_class==0 && report.messages[0]=="L.N.D.S system");
    const auto seed=rng.seed(); const auto before=encode_actor(s.actors[4]);
    unsupported([&] { prepare_human_delivery_command(s,fields,7,3,c,rng); });
    CHECK(rng.seed()==seed && encode_actor(s.actors[4])==before && fields.selected_class==0);
    s.actors[4].missiles={0,0,0,0};
    unsupported([&] { prepare_human_delivery_command(s,fields,2,0,c,rng); });
    CHECK(rng.seed()==seed && fields.selected_class==0);
});
group("human city logical view differs from display confirmation marker",[] {
    State s; s.actors[4].target_actor=3; s.actors[4].target_city=4; HumanSelection fields;
    MessageSequenceRng c{true}; Rng rng(1); const std::array<std::string,5> names{};
    auto report=prepare_human_city_target(s,fields,2,65535,1,names,c,rng);
    CHECK(report.accepted && report.target_marker==65535 && report.draws==0);
    CHECK(s.actors[4].target_actor==2 && s.actors[4].target_city==1 && rng.seed()==1 && c.at_initial_origin);
    fields.pending_action=1;
    CHECK(!human_confirmation_ready(fields.pending_action,report.target_marker));
    report=prepare_human_city_target(s,fields,1,3,0,names,c,rng);
    CHECK(report.accepted && report.target_marker==3 && s.actors[4].target_actor==1);
    const auto before=encode_actor(s.actors[4]);
    report=prepare_human_city_target(s,fields,4,1,65535,names,c,rng);
    CHECK(!report.accepted && report.messages==std::vector<std::string>({"You may not target","your own country!",""}));
    CHECK(encode_actor(s.actors[4])==before && !c.at_initial_origin);
});
group("human propaganda city text before rolls and target stores",[] {
    for(Word old:{Word(0),Word(2),Word(4)}) {
        State s; s.actors[4].action=old; HumanSelection fields; fields.pending_action=3;
        MessageSequenceRng c{false}, ec{false}; Rng rng(1), expected(1);
        const std::array<std::string,5> names{"A","B","C","D","E"};
        const std::vector<std::string> text{"","C targeted","for propaganada.",old==4?"This will junk a bomber":old==2?"This will junk a missile":""};
        unsigned draws=0; for(const auto& line:text) draws+=advance_message_sequence(ec,line,expected).draws;
        const Word cls=Word(expected.next()%4), discarded=Word(expected.next()%100+1), magnitude=Word(expected.next()%10+1);
        auto report=prepare_human_city_target(s,fields,2,0,4,names,c,rng);
        CHECK(report.accepted && report.messages==text && report.draws==draws+3 && report.target_marker==0);
        CHECK(fields.selected_class==cls && fields.selected_capacity==class_capacity(cls) && report.discarded_roll==discarded);
        CHECK(s.actors[4].magnitude==magnitude && s.actors[4].action==old && s.actors[4].target_actor==2 && s.actors[4].target_city==4);
        CHECK(rng.seed()==expected.seed());
    }
    State s; HumanSelection fields; fields.pending_action=3; MessageSequenceRng c{true}; Rng rng(1);
    const std::array<std::string,5> names{};
    unsupported([&] { prepare_human_city_target(s,fields,2,0,5,names,c,rng); });
    CHECK(rng.seed()==1 && c.at_initial_origin && fields.selected_class==65535);
});
group("normalized human actorview toggle and status draw count",[] {
    State s; for(unsigned i=0;i<4;++i) s.actors[i].population[0]=1;
    for(Word logical=0;logical<5;++logical)
    for(Word display:{Word(0),Word(1),Word(2),Word(3),Word(4),Word(65535)})
    for(Word requested=0;requested<5;++requested) {
        Rng rng(1); const Word candidate=requested==display?4:requested;
        auto report=change_human_actor_view(s,logical,display,requested,rng);
        CHECK(report.changed==(candidate!=display));
        CHECK(report.logical_view==(candidate==display?logical:candidate));
        CHECK(report.display_actor==(candidate==display?display:candidate));
        CHECK(report.draws==(candidate==display?0u:8u));
        Rng expected(1); for(unsigned n=0;n<report.draws;++n) expected.next();
        CHECK(rng.seed()==expected.seed());
    }
});
group("normalized human actorview hostile filtering and unsupported indices",[] {
    State s; s.actors[0].population[0]=1; s.actors[0].scores[4]=Word(-32767);
    Rng rng(1); auto report=change_human_actor_view(s,4,65535,2,rng);
    CHECK(report.changed && report.draws==0 && report.logical_view==2 && rng.seed()==1);
    unsupported([&] { change_human_actor_view(s,5,4,0,rng); });
    unsupported([&] { change_human_actor_view(s,4,5,0,rng); });
    unsupported([&] { change_human_actor_view(s,4,4,5,rng); });
    CHECK(rng.seed()==1);
});
group("human menu status occurs for accepted and invalid raw codes",[] {
    for(Word code:{Word(0),Word(32),Word(65535),Word(1),Word(64)}) {
        State s; for(unsigned i=0;i<4;++i) s.actors[i].population[0]=1;
        HumanSelection fields; fields.pending_action=6; Rng rng(1), expected(1);
        auto refresh=attitude_refresh_rng(s,expected);
        auto report=prepare_human_menu_command(s,fields,code,rng);
        CHECK(report.draws==refresh.draws && report.draws==8 && rng.seed()==expected.seed());
        CHECK(report.control_return==human_menu_control(code));
        CHECK(s.actors[4].action==(report.control_return==0?0:6));
    }
});
group("human menu bypasses q guard and rolls back unsupported pending",[] {
    State s; s.actors[0].population[0]=1; s.actors[4].action=2;
    HumanSelection fields; Rng rng(1);
    CHECK(!human_confirmation_ready(fields.pending_action,65535));
    auto report=prepare_human_menu_command(s,fields,32,rng);
    CHECK(report.control_return==2 && s.actors[4].action==0 && s.actors[0].scores[4]==13106);
    const auto seed=rng.seed(); auto before=encode_actor(s.actors[0]);
    fields.pending_action=65535;
    unsupported([&] { prepare_human_menu_command(s,fields,0,rng); });
    CHECK(rng.seed()==seed && encode_actor(s.actors[0])==before);
});
group("normalized human complete missile preparation turn golden RNG",[] {
    State s; for(auto& actor:s.actors) actor.population[0]=10;
    s.actors[4].missiles[0]=1; s.actors[4].warheads[0]=1;
    Word welcome=0; MessageSequenceRng c{true}; Rng rng(1);
    const std::array<std::string,5> names{"A","B","C","D","E"};
    auto start=start_human_commands(s,0,0,names,welcome,c,rng);
    auto session=start.command_state; unsigned draws=start.entry.draws;
    CHECK(draws==28 && session.active && session.target_marker==65535);
    auto report=apply_human_command(s,session,{HumanCommandKind::confirm},names,c,rng);
    draws+=report.draws; CHECK(!report.turn_finished && report.messages[0]=="Select an action");
    report=apply_human_command(s,session,{HumanCommandKind::actor_view,1},names,c,rng); draws+=report.draws;
    CHECK(session.logical_view==1 && session.display_actor==1);
    report=apply_human_command(s,session,{HumanCommandKind::city_target,2},names,c,rng); draws+=report.draws;
    CHECK(session.target_marker==1 && s.actors[4].target_actor==1 && s.actors[4].target_city==2);
    report=apply_human_command(s,session,{HumanCommandKind::missile,1},names,c,rng); draws+=report.draws;
    CHECK(session.selection.pending_action==2 && session.display_actor==65535 && session.logical_view==1);
    CHECK(s.actors[4].missiles[0]==1 && s.actors[4].action==0);
    report=apply_human_command(s,session,{HumanCommandKind::confirm},names,c,rng); draws+=report.draws;
    CHECK(report.turn_finished && report.control_return==0 && !session.active);
    CHECK(s.actors[4].action==2 && s.actors[4].missiles[0]==0 && s.human_prepared_capacity==10);
    CHECK(draws==8876 && rng.seed()==0x65351355);
    const auto seed=rng.seed(); unsupported([&] { apply_human_command(s,session,{HumanCommandKind::production},names,c,rng); });
    CHECK(rng.seed()==seed);
});
group("normalized human hidden display synthetic routing and classification",[] {
    State s; for(auto& actor:s.actors) actor.population[0]=10;
    s.actors[4].action=2; s.actors[4].target_actor=1;
    HumanCommandState session; session.active=true; session.logical_view=1; session.display_actor=65535; session.target_marker=1;
    session.selection.selected_class=0;
    MessageSequenceRng c{false}; Rng rng(1); const std::array<std::string,5> names{"A","B","C","D","E"};
    auto expected=s; auto ef=session.selection; auto ec=c; Rng er=rng;
    auto prefix=prepare_human_propaganda_command(expected,ef,1,names,ec,er);
    auto refresh=change_human_actor_view(expected,1,65535,1,er);
    auto report=apply_human_command(s,session,{HumanCommandKind::propaganda},names,c,rng);
    CHECK(report.draws==prefix.draws+refresh.draws && rng.seed()==er.seed());
    CHECK(session.logical_view==1 && session.display_actor==1 && session.target_marker==1 && session.selection.pending_action==3);
    session.display_actor=65535;
    report=apply_human_command(s,session,{HumanCommandKind::production},names,c,rng);
    CHECK(session.logical_view==4 && session.display_actor==4 && session.selection.pending_action==6);
    const auto seed=rng.seed(); report=apply_human_command(s,session,{HumanCommandKind::classification,2},names,c,rng);
    CHECK(s.actors[4].classifications[2]==255 && report.draws==0 && rng.seed()==seed);
    report=apply_human_command(s,session,{HumanCommandKind::confirm},names,c,rng);
    CHECK(report.turn_finished && s.actors[4].action==6);
});
group("normalized human unsupported coordinate route preserves whole transition",[] {
    State s; s.actors[4].population[0]=10;
    HumanCommandState session; session.active=true; session.logical_view=1; session.display_actor=65535; session.selection.pending_action=3;
    const auto before=encode_actor(s.actors[4]); MessageSequenceRng c{true}; Rng rng(1);
    const std::array<std::string,5> names{"A","B","C","D","E"};
    unsupported([&] { apply_human_command(s,session,{HumanCommandKind::city_target,0},names,c,rng); });
    CHECK(encode_actor(s.actors[4])==before && rng.seed()==1 && c.at_initial_origin);
    CHECK(session.selection.selected_class==65535 && session.target_marker==65535 && session.active);
    session.logical_view=4;
    auto report=apply_human_command(s,session,{HumanCommandKind::city_target,65535},names,c,rng);
    CHECK(!report.turn_finished && report.messages[0]=="You may not target" && !c.at_initial_origin);
    const auto seed=rng.seed();
    unsupported([&] { apply_human_command(s,session,{static_cast<HumanCommandKind>(255)},names,c,rng); });
    CHECK(rng.seed()==seed && session.active);
});
group("normalized human menu exit and dead human entry contracts",[] {
    State s; HumanCommandState session; session.active=true;
    session.selection.pending_action=6; MessageSequenceRng c{true}; Rng rng(1);
    const std::array<std::string,5> names{};
    auto report=apply_human_command(s,session,{HumanCommandKind::menu,32},names,c,rng);
    CHECK(report.turn_finished && report.control_return==2 && !session.active && s.actors[4].action==6);
    Word welcome=1; const auto seed=rng.seed();
    unsupported([&] { start_human_commands(s,0,0,names,welcome,c,rng); });
    CHECK(welcome==1 && c.at_initial_origin && rng.seed()==seed);
});
group("normalized human bomber defense and warhead completion paths",[] {
    const std::array<std::string,5> names{"A","B","C","D","E"};
    for(auto kind:{HumanCommandKind::bomber,HumanCommandKind::defense,HumanCommandKind::warhead}) {
        State s; auto& human=s.actors[4]; human.population[0]=10;
        human.bombers[1]=1; human.defenses[1]=1; human.warheads[1]=1;
        HumanCommandState session; session.active=true; session.selection.selected_class=0;
        session.selection.selected_capacity=777; session.target_marker=0;
        if(kind==HumanCommandKind::warhead) { human.action=4; s.human_prepared_capacity=50; }
        MessageSequenceRng c{false}; Rng rng(1);
        auto report=apply_human_command(s,session,{kind,2},names,c,rng);
        CHECK(!report.turn_finished && session.display_actor==65535);
        CHECK(human.bombers[1]==1 && human.defenses[1]==1 && human.warheads[1]==1);
        report=apply_human_command(s,session,{HumanCommandKind::confirm},names,c,rng);
        CHECK(report.turn_finished);
        if(kind==HumanCommandKind::bomber) CHECK(human.action==4 && human.bombers[1]==0 && s.human_prepared_capacity==100);
        if(kind==HumanCommandKind::defense) CHECK(human.action==7 && human.defenses[1]==0 && human.magnitude==1 && session.selection.selected_capacity==777);
        if(kind==HumanCommandKind::warhead) CHECK(human.action==5 && human.warheads[1]==0 && s.human_prepared_capacity==30);
    }
});
group("normalized hidden propaganda target restores old view but retains ffff marker",[] {
    State s; s.actors[4].population[0]=10;
    HumanCommandState session; session.active=true; session.logical_view=2; session.display_actor=65535;
    session.target_marker=0; session.selection.pending_action=3;
    const std::array<std::string,5> names{"A","B","C","D","E"};
    MessageSequenceRng c{false}; Rng rng(1);
    auto report=apply_human_command(s,session,{HumanCommandKind::city_target,1},names,c,rng);
    CHECK(!report.turn_finished && session.logical_view==0 && session.display_actor==0);
    CHECK(session.target_marker==65535 && s.actors[4].target_actor==2 && s.actors[4].target_city==1);
    report=apply_human_command(s,session,{HumanCommandKind::confirm},names,c,rng);
    CHECK(!report.turn_finished && report.messages[0]=="Choose a target city" && s.actors[4].action==0);
});
group("resolver startup captures cities and replaces RNG with explicit word",[] {
    State s; s.actors[4].population={0,1,32767,32768,65535};
    s.pass_processed={1,2,65535,4,5}; s.prepared_capacity={10,20,30,40,50};
    for(Word word:{Word(0),Word(1),Word(32768),Word(65535)}) {
        Rng rng(0xdeadbeef); auto entry=begin_resolver_entry(s,word,rng);
        CHECK(entry.human_cities_before==2 && entry.pass==0 && entry.control_return==0);
        CHECK(rng.seed()==word);
        Rng expected(word); CHECK(rng.next()==expected.next() && rng.seed()==expected.seed());
        CHECK((s.pass_processed==std::array<Word,5>({1,2,65535,4,5})));
        CHECK((s.prepared_capacity==std::array<Word,5>({10,20,30,40,50})));
    }
    s.actors[4].population={0,0,0,0,0}; Rng rng(1);
    auto entry=begin_resolver_entry(s,65535,rng);
    CHECK(entry.human_cities_before==0 && rng.seed()==65535);
});
group("ordinary resolver snapshot precedes transfer setup and handler capture",[] {
    State s; s.actors[0].action=3; s.actors[0].magnitude=25; s.actors[0].population[0]=20;
    s.actors[3].population[0]=5; s.pass_processed[0]=65535; Rng rng(123);
    auto report=prepare_resolver_action_prelude(s,0,0,rng);
    CHECK(report.eligibility.route==ResolverActorRoute::ordinary_action && report.magnitude_was_25);
    CHECK(report.magnitude_before_handler==14 && report.draws==4 && rng.seed()==0x33842957);
    CHECK(s.actors[0].action==14 && s.actors[0].population[0]==6 && s.actors[0].target_actor==3);
    CHECK(s.actors[3].population[0]==5 && s.pass_processed[0]==65535);
    auto init=initialize_event_handler(s.actors[0],rng);
    CHECK(init.effect==0x14 && init.draws==1 && report.magnitude_was_25);
    transfer_arrival_commit(s,0);
    CHECK(s.actors[3].population[0]==19 && s.actors[0].action==0x14);
});
group("ordinary resolver skips and undefined followup do not mutate or draw",[] {
    State s; Rng rng(1); s.actors[0].action=3;
    auto report=prepare_resolver_action_prelude(s,0,0,rng);
    CHECK(report.eligibility.route==ResolverActorRoute::no_cities && report.draws==0 && rng.seed()==1);
    s.actors[1].population[0]=10;
    report=prepare_resolver_action_prelude(s,0,0,rng);
    CHECK(report.eligibility.route==ResolverActorRoute::skip_actor && s.actors[0].action==3 && rng.seed()==1);
    const auto before=encode_actor(s.actors[0]);
    unsupported([&] { prepare_resolver_action_prelude(s,0,1,rng); });
    CHECK(encode_actor(s.actors[0])==before && s.pass_processed[0]==0 && rng.seed()==1);
    s.pass_processed[0]=1;
    report=prepare_resolver_action_prelude(s,0,1,rng);
    CHECK(report.eligibility.route==ResolverActorRoute::skip_actor && rng.seed()==1);
    s.actors[1].action=2; s.actors[1].magnitude=25;
    report=prepare_resolver_action_prelude(s,1,0,rng);
    CHECK(report.magnitude_was_25 && report.magnitude_before_handler==25 && report.draws==0 && rng.seed()==1);
    unsupported([&] { prepare_resolver_action_prelude(s,5,0,rng); });
});
group("resolver action start missile and transfer exact first-frame RNG",[] {
    State s; s.actors[0].population[0]=20; s.actors[0].action=1; s.actors[0].magnitude=25;
    Rng rng(1); auto report=start_resolver_action(s,0,0,rng);
    CHECK(report.initialized && report.dispatch.handler==0x117576 && report.handler.effect==8);
    CHECK(report.draws==3 && rng.seed()==0x2781e494 && report.prelude.magnitude_was_25);
    CHECK(s.actors[0].magnitude==25 && s.actors[0].population[0]==20 && s.pass_processed[0]==0);
    s.actors[0].action=3; s.actors[3].population[0]=5; rng=Rng(123);
    report=start_resolver_action(s,0,0,rng);
    CHECK(report.initialized && report.dispatch.handler==0x110f64 && report.handler.effect==0x14);
    CHECK(report.prelude.draws==4 && report.handler.draws==1 && report.draws==5);
    CHECK(report.prelude.magnitude_was_25 && report.prelude.magnitude_before_handler==14);
    CHECK(s.actors[0].population[0]==6 && s.actors[3].population[0]==5 && s.pass_processed[0]==0);
});
group("resolver action start preparation and known special routes",[] {
    for(Word action:{Word(2),Word(4),Word(5),Word(6),Word(7),Word(8),Word(9),Word(11),Word(12),Word(13),Word(14),Word(15),Word(16),Word(86)}) {
        State s; for(auto& a:s.actors) a.population[0]=20;
        s.actors[0].action=action; s.actors[0].target_city=0; s.actors[0].target_actor=1;
        auto expected=s; Rng rng(1), er(1); EventInitialization init;
        if(action==5) { auto roll=begin_attack_roll(action,er); init.effect=roll.effect; init.message_variant=roll.message_variant; init.draws=roll.draws; }
        else if(action==2 || action==4 || action==6 || action==7) init=initialize_preparation_handler(expected,0,er);
        else init=initialize_event_handler(expected.actors[0],er);
        auto report=start_resolver_action(s,0,0,rng);
        CHECK(report.initialized && report.handler.effect==init.effect && report.handler.message_variant==init.message_variant);
        CHECK(report.draws==init.draws && rng.seed()==er.seed());
        for(unsigned i=0;i<5;++i) CHECK(encode_actor(s.actors[i])==encode_actor(expected.actors[i]));
        CHECK(s.pass_processed==expected.pass_processed);
    }
});
group("resolver action start skips and unsupported handlers preserve state",[] {
    State s; s.actors[1].population[0]=10; s.actors[0].action=10; Rng rng(1);
    auto report=start_resolver_action(s,0,0,rng); CHECK(!report.initialized && report.draws==0 && rng.seed()==1);
    s.actors[0].population[0]=10; const auto before=encode_actor(s.actors[0]);
    unsupported([&] { start_resolver_action(s,0,0,rng); });
    CHECK(encode_actor(s.actors[0])==before && rng.seed()==1);
    s.actors[0].population[0]=0;
    unsupported([&] { start_resolver_action(s,0,1,rng); });
    CHECK(s.actors[0].action==10 && rng.seed()==1);
});
group("resolver frame tail exact boundary control transitions",[] {
    struct Case { Word frame, reverse, completion, suppress, next; bool render, repeat; };
    const Case cases[]{
        {0,0,1,0,1,true,true},{799,0,1,0,800,true,false},
        {0,1,1,65535,65535,false,false},{65535,0,1,1,0,false,true},
        {32768,65535,65535,0,32767,true,false},{32767,0,1,0,32768,true,true},
        {20,0,0,1,21,false,false},{800,1,1,0,799,true,true}};
    for(const auto& c:cases) {
        auto result=resolver_frame_tail_step(c.frame,c.reverse,c.completion,c.suppress);
        CHECK(result.next_frame==c.next && result.set_render_request==c.render && result.repeat_handler==c.repeat);
    }
});
group("resolver frame tail exhaustive word arithmetic and signed loop bound",[] {
    for(unsigned raw=0;raw<65536;++raw) for(int delta:{-1,1}) {
        const int mathematical=int(raw)+delta;
        const unsigned expected_word=mathematical<0?65535u:mathematical>65535?0u:unsigned(mathematical);
        const int expected_signed=expected_word>=32768?int(expected_word)-65536:int(expected_word);
        auto result=resolver_frame_tail_step(Word(raw),delta<0?Word(65535):Word(0),1,0);
        CHECK(result.next_frame==expected_word);
        CHECK(result.set_render_request);
        CHECK(result.repeat_handler==(expected_signed<800 && expected_word!=65535));
    }
});
group("bomber heading signed coordinate partition and exact contact",[] {
    const Word values[]{0,1,32767,32768,65535};
    const Word table[3][3]={{4,3,2},{5,0,1},{6,7,8}};
    const auto signed_value=[](Word w) { return w>=32768?int(w)-65536:int(w); };
    for(auto x:values) for(auto y:values) for(auto tx:values) for(auto ty:values) {
        const int sx=signed_value(x),sy=signed_value(y),stx=signed_value(tx),sty=signed_value(ty);
        const int xr=sx<stx?0:sx==stx?1:2, yr=sy<sty?0:sy==sty?1:2;
        CHECK(bomber_heading_to(x,y,tx,ty)==table[xr][yr]);
    }
    CHECK(bomber_heading_to(65535,0,0,0)==3);
    CHECK(bomber_heading_to(0,32768,0,32767)==5);
});
group("eight-direction movement wraps words and preserves default no-op",[] {
    const Word values[]{0,1,32767,32768,65535};
    const int dx[]{0,0,1,1,1,0,-1,-1,-1};
    const int dy[]{0,-1,-1,0,1,1,1,0,-1};
    for(auto original_x:values) for(auto original_y:values)
    for(Word heading:{Word(0),Word(1),Word(2),Word(3),Word(4),Word(5),Word(6),Word(7),Word(8),Word(9),Word(65535)}) {
        Word x=original_x,y=original_y; step_heading_word(x,y,heading);
        const unsigned index=heading<=8?heading:0;
        const unsigned ex=(unsigned(int(original_x)+dx[index])+65536u)%65536u;
        const unsigned ey=(unsigned(int(original_y)+dy[index])+65536u)%65536u;
        CHECK(x==ex && y==ey);
    }
});
group("eight-direction original pointer alias behavior",[] {
    const int total_delta[]{0,-1,0,1,2,1,0,-1,-2};
    for(Word start:{Word(0),Word(1),Word(32767),Word(32768),Word(65535)})
    for(Word heading=0;heading<9;++heading) {
        Word aliased=start; step_heading_word(aliased,aliased,heading);
        const unsigned expected=(unsigned(int(start)+total_delta[heading])+65536u)%65536u;
        CHECK(aliased==expected);
    }
});
group("bomber control exact arrival then return-trip marker timing",[] {
    BomberControlState s; s.heading=3; s.speed_count=1; s.turn_count=1;
    s.x=10; s.target_x=12;
    auto result=bomber_control_tick(s,1,10,0);
    CHECK(result.marker_written && result.outputs_written && result.moved && !result.emitted_arrival);
    CHECK(s.output_marker==0xd1 && s.output_x==10 && s.x==11 && s.turn_count==1);
    result=bomber_control_tick(s,2,10,0); CHECK(s.x==12 && s.heading==3 && !result.emitted_arrival);
    result=bomber_control_tick(s,3,10,0);
    CHECK(s.heading==0 && s.saved_heading==3 && !result.moved && !result.emitted_arrival);
    CHECK(s.output_marker==0xd1 && s.turn_count==1);
    result=bomber_control_tick(s,4,10,0);
    CHECK(result.emitted_arrival && result.moved && s.output_marker==0x5208 && s.return_flag==1);
    CHECK(s.target_x==10 && s.target_y==0 && s.output_x==12 && s.output_y==0);
    CHECK(s.heading==2 && s.rotation_step==65535 && s.predecessor_count==4 && s.successor_count==4);
    CHECK(s.x==13 && s.y==65535 && s.turn_count==1 && s.output_width==16 && s.output_height==16);
});
group("bomber control suppression leaves stale marker and output unchanged",[] {
    BomberControlState s; s.heading=0; s.return_flag=65535; s.output_marker=0x1234;
    s.x=20; s.y=30; s.speed_count=1; s.output_x=40; s.output_y=50; s.output_width=7; s.output_height=9;
    auto result=bomber_control_tick(s,5,0,0);
    CHECK(!result.marker_written && !result.outputs_written && !result.moved);
    CHECK(s.output_marker==0x1234 && s.output_x==40 && s.output_y==50 && s.output_width==7 && s.speed_count==1);
    s.heading=3;
    for(Word frame:{Word(800),Word(32767)}) {
        result=bomber_control_tick(s,frame,0,0);
        CHECK(!result.marker_written && s.output_marker==0x1234 && s.x==20 && s.speed_count==1);
    }
    result=bomber_control_tick(s,32768,0,0);
    CHECK(result.marker_written && s.x==21 && s.output_x==20);
});
group("bomber control countdown and original signed proximity bug",[] {
    for(Word speed:{Word(0),Word(2)}) {
        BomberControlState s; s.x=100; s.y=100; s.target_x=200; s.target_y=200;
        s.heading=3; s.speed_count=speed; s.turn_count=1;
        auto result=bomber_control_tick(s,1,0,0);
        CHECK(!result.moved && s.x==100 && s.turn_count==1 && s.speed_count==(speed==0?65535:1));
    }
    BomberControlState s; s.x=100; s.y=100; s.target_x=200; s.target_y=200;
    s.heading=3; s.speed_count=1; s.turn_count=1;
    bomber_control_tick(s,1,0,0);
    CHECK(s.heading==4 && s.turn_count==1 && s.x==101 && s.y==101 && s.output_marker==0xd1);
    s.x=192; s.y=100; s.heading=3; s.turn_count=1;
    bomber_control_tick(s,2,0,0);
    CHECK(s.heading==4 && s.turn_count==8); //x==targetX-8 suppresses buggy Y test
    s.x=100; s.y=32768; s.target_y=32760; s.heading=3; s.turn_count=1;
    bomber_control_tick(s,3,0,0);
    CHECK(s.heading==4 && s.turn_count==8 && s.y==32769); //signed word wrap in Y comparisons
});
group("bomber cyclic steering all headings and original tie direction",[] {
    const int dx[]{0,0,1,1,1,0,-1,-1,-1},dy[]{0,-1,-1,0,1,1,1,0,-1};
    for(Word heading=1;heading<=8;++heading) for(Word desired=1;desired<=8;++desired) {
        BomberControlState s; s.x=100; s.y=100; s.target_x=Word(100+10*dx[desired]); s.target_y=Word(100+10*dy[desired]);
        s.heading=heading; s.speed_count=1; s.turn_count=1;
        const unsigned forward=(unsigned(desired)+8-heading)%8, backward=(unsigned(heading)+8-desired)%8;
        const unsigned successor=forward==0?8:forward, predecessor=backward==0?8:backward;
        const bool clockwise=successor<predecessor;
        const Word expected=heading==desired?heading:clockwise?(heading==8?1:Word(heading+1)):(heading==1?8:Word(heading-1));
        bomber_control_tick(s,1,0,0);
        CHECK(s.desired_heading==desired && s.heading==expected);
        CHECK(s.successor_count==successor && s.predecessor_count==predecessor);
        CHECK(s.rotation_step==(clockwise?1:65535));
    }
    BomberControlState s; s.heading=65535; s.speed_count=1; s.turn_count=1; s.target_x=1;
    bomber_control_tick(s,1,0,0);
    CHECK(s.output_marker==0xcd && s.heading==8 && s.predecessor_count==6 && s.successor_count==4);
    CHECK(s.x==65535 && s.y==65535);
});
group("bomber numeric initialization preserves unwritten word globals",[] {
    State s; s.actors[2].target_actor=4; s.actors[2].target_city=3;
    s.actors[4].city_x[3]=32768; s.actors[4].city_y[3]=65535;
    BomberControlState b; b.return_flag=9; b.turn_count=3; b.heading=7;
    b.rotation_step=77; b.predecessor_count=88; b.successor_count=99; b.saved_heading=65535;
    b.output_marker=0xdead; b.output_x=9; b.output_y=10; b.output_width=7; b.output_height=8;
    initialize_bomber_control_fields(b,s,2,100,200);
    CHECK(b.return_flag==0 && b.turn_count==8 && b.heading==3 && b.desired_heading==3 && b.speed_count==1);
    CHECK(b.x==100 && b.y==200 && b.target_x==32768 && b.target_y==65535);
    CHECK(b.rotation_step==77 && b.predecessor_count==88 && b.successor_count==99 && b.saved_heading==65535);
    CHECK(b.output_marker==0xdead && b.output_x==9 && b.output_y==10 && b.output_width==7 && b.output_height==8);
    s.actors[2].target_city=5;
    unsupported([&] { initialize_bomber_control_fields(b,s,2,0,0); });
    CHECK(b.x==100 && b.turn_count==8 && b.output_marker==0xdead);
});
group("bomber action framezero composes dice initialization and first numeric tick",[] {
    State s; s.actors[0].action=5; s.actors[0].population[0]=20; s.actors[0].magnitude=25;
    s.actors[0].target_actor=1; s.actors[0].target_city=0;
    s.actors[1].population[0]=100; s.actors[1].city_x[0]=12;
    BomberControlState b; b.saved_heading=7; Rng rng(1);
    auto result=start_bomber_action(s,b,0,0,10,0,rng);
    CHECK(result.resolver.initialized && result.resolver.handler.effect==8 && result.resolver.handler.message_variant==2);
    CHECK(result.resolver.draws==3 && rng.seed()==0x2781e494 && result.resolver.prelude.magnitude_was_25);
    CHECK(result.first_tick.moved && !result.first_tick.emitted_arrival && b.output_marker==0xd1 && result.clear_seen_59d8);
    CHECK(b.x==11 && b.y==0 && b.output_x==10 && b.output_y==0 && b.turn_count==7 && b.saved_heading==7);
    CHECK(b.target_x==12 && b.target_y==0 && b.heading==3 && b.return_flag==0);
    CHECK(s.actors[0].magnitude==25 && s.actors[1].population[0]==100 && s.pass_processed[0]==0);
});
group("bomber action start unsupported targets roll back initialization draws",[] {
    State s; s.actors[0].action=5; s.actors[0].population[0]=20; s.actors[0].target_actor=5;
    BomberControlState b; b.heading=77; b.output_marker=0xbeef; Rng rng(1);
    const auto before=encode_actor(s.actors[0]);
    unsupported([&] { start_bomber_action(s,b,0,0,10,0,rng); });
    CHECK(rng.seed()==1 && b.heading==77 && b.output_marker==0xbeef && encode_actor(s.actors[0])==before);
    s.actors[0].population[0]=0; s.actors[1].population[0]=20;
    auto report=start_bomber_action(s,b,0,0,10,0,rng);
    CHECK(!report.resolver.initialized && !report.first_tick.moved && !report.clear_seen_59d8 && rng.seed()==1 && b.heading==77);
});
group("resolver ordinary tokens preserve primary clear and first rewind setup",[] {
    ResolverOrdinaryTokenState s; s.frame=100; s.saved_frame=55; s.frameflag_f4=7;
    s.primary_132418=123; s.primary_13241a=196; s.render_phase_13234e=9;
    auto step=apply_resolver_ordinary_token(0x5dc0,s);
    CHECK(step.state.primary_132418==0 && step.state.primary_13241a==0);
    CHECK(step.state.frame==100 && step.state.saved_frame==55 && step.state.frameflag_f4==7 && !step.first_59d8_setup && !step.reverse_setup);
    step=apply_resolver_ordinary_token(0x59d8,s);
    CHECK(step.first_59d8_setup && !step.reverse_setup && step.state.saved_frame==100 && step.state.seen_59d8==1);
    CHECK(step.state.primary_13241a==0x114 && step.state.primary_132418==123 && step.state.render_phase_13234e==2 && step.state.frameflag_f4==0);
    CHECK(step.state.frame==100 && step.state.reverse==0);
    s=step.state; s.frame=799;
    step=apply_resolver_ordinary_token(0x59d8,s);
    CHECK(!step.first_59d8_setup && step.reverse_setup && step.state.frame==100 && step.state.reverse==1);
    CHECK(step.state.render_phase_13234e==1 && step.state.primary_13241a==0x114 && step.state.seen_59d8==1);
    auto tail=resolver_frame_tail_step(step.state.frame,step.state.reverse,1,step.state.frameflag_f4);
    CHECK(tail.next_frame==99 && tail.repeat_handler && tail.set_render_request);
});
group("resolver ordinary token nonzero flags and frame799 exact gate",[] {
    for(Word seen:{Word(1),Word(65535)}) for(Word reverse:{Word(0),Word(1),Word(65535)})
    for(Word frame:{Word(798),Word(799),Word(800),Word(65535)}) {
        ResolverOrdinaryTokenState s; s.frame=frame; s.reverse=reverse; s.seen_59d8=seen;
        s.saved_frame=65535; s.frameflag_f4=8; s.render_phase_13234e=9;
        auto step=apply_resolver_ordinary_token(0x59d8,s);
        const bool rewind=reverse==0 && frame==799;
        CHECK(step.reverse_setup==rewind && !step.first_59d8_setup);
        CHECK(step.state.frame==(rewind?65535:frame) && step.state.reverse==(rewind?1:reverse));
        CHECK(step.state.frameflag_f4==8 && step.state.render_phase_13234e==(rewind?1:9));
    }
    ResolverOrdinaryTokenState s; s.frame=799; s.primary_13241a=196;
    auto first=apply_resolver_ordinary_token(0x59d8,s);
    CHECK(first.first_59d8_setup && !first.reverse_setup && first.state.frame==799);
    auto second=apply_resolver_ordinary_token(0x59d8,first.state);
    CHECK(second.reverse_setup && second.state.frame==799 && second.state.reverse==1);
    auto unrelated=apply_resolver_ordinary_token(0x5208,s);
    CHECK(!unrelated.first_59d8_setup && !unrelated.reverse_setup && unrelated.state.primary_13241a==196);
});
group("pending combat early render gate and final fallback commit exactly once",[] {
    State s; s.actors[0].action=1; s.actors[0].magnitude=10; s.actors[0].population[0]=10;
    s.actors[0].target_actor=1; s.actors[0].target_city=0;
    s.actors[1].population[0]=100; Rng rng(1);
    auto start=start_resolver_action(s,0,0,rng); CHECK(start.handler.effect==8);
    auto frame=classify_resolver_frame(0x5208,0,0,2); Word pending=frame.pending_value;
    CHECK(pending==15);
    evaluate_combat_impact(s,0,1,0,start.handler.effect,start.prelude.magnitude_was_25,rng);
    CHECK(s.actors[0].magnitude==14 && rng.seed()==0xc46b9b3d);
    auto report=commit_pending_combat(s,0,1,0,pending,195,false);
    CHECK(!report.committed && pending==15 && s.actors[1].population[0]==100);
    report=commit_pending_combat(s,0,1,0,pending,196,false);
    CHECK(report.committed && report.loss==14 && pending==0 && s.actors[1].population[0]==86);
    CHECK(s.actors[1].scores[0]==14600 && rng.seed()==0xc46b9b3d);
    const auto before=encode_actor(s.actors[1]);
    report=commit_pending_combat(s,5,5,5,pending,0,true);
    CHECK(!report.committed && encode_actor(s.actors[1])==before);
    pending=65535; s.actors[0].magnitude=65535;
    report=commit_pending_combat(s,0,1,0,pending,0,true);
    CHECK(report.committed && report.loss==65535 && s.actors[1].population[0]==87 && pending==0);
});
group("pending combat false gates ignore unused indices and unsupported commits roll back",[] {
    State s; s.actors[0].magnitude=20; s.actors[1].population[0]=5;
    Word pending=0;
    auto report=commit_pending_combat(s,65535,65535,65535,pending,196,false);
    CHECK(!report.committed && pending==0);
    pending=1;
    report=commit_pending_combat(s,65535,65535,65535,pending,65535,false);
    CHECK(!report.committed && pending==1);
    const auto before=encode_actor(s.actors[0]);
    unsupported([&] { commit_pending_combat(s,0,5,0,pending,196,false); });
    CHECK(pending==1 && encode_actor(s.actors[0])==before && s.actors[1].population[0]==5);
    report=commit_pending_combat(s,0,1,0,pending,65535,true);
    CHECK(report.committed && report.loss==5 && s.actors[0].magnitude==5 && pending==0);
});
group("resolver framezero latches handler-updated fields not prelude snapshot",[] {
    State s; auto& actor=s.actors[0]; actor.action=11; actor.population[3]=20; actor.population[4]=5;
    actor.magnitude=25; actor.target_actor=1; actor.target_city=4; Rng rng(1);
    auto start=start_resolver_action(s,0,0,rng);
    CHECK(start.prelude.magnitude_was_25 && start.prelude.magnitude_before_handler==25 && actor.magnitude==20);
    ResolverFrameContext prior; prior.reverse=65535; prior.magnitude=999; prior.completion=0;
    auto context=initialize_resolver_frame_context(s,0,0,start.handler.effect,prior);
    CHECK(context.target_actor==1 && context.target_city==3 && context.magnitude==20);
    CHECK(context.render_phase_13234e==2 && context.reverse==0 && context.completion==2);
    CHECK(actor.population[3]==20 && rng.seed()==0x41c67ea6);
    actor.target_actor=4; actor.target_city=0; actor.magnitude=7;
    context.reverse=65535; context.completion=0;
    auto next=initialize_resolver_frame_context(s,65535,1,0,context);
    CHECK(next.target_actor==1 && next.target_city==3 && next.magnitude==20);
    CHECK(next.render_phase_13234e==2 && next.reverse==65535 && next.completion==2);
});
group("resolver frame context raw target widths and exact selector reverse exceptions",[] {
    State s; s.actors[0].target_actor=65535; s.actors[0].target_city=32768; s.actors[0].magnitude=65535;
    ResolverFrameContext previous; previous.reverse=65535; previous.render_phase_13234e=99; previous.completion=9;
    for(Word selector:{Word(0),Word(1),Word(2),Word(65535),Word(3),Word(0x10),Word(0x11),Word(0x13),Word(0x14),Word(0x19),Word(0x56),Word(32768)}) {
        auto context=initialize_resolver_frame_context(s,0,0,selector,previous);
        const bool keep_reverse=selector==0 || selector==1 || selector==2 || selector==65535;
        const bool phase2=selector==0x10 || selector==0x11 || selector==0x13 || selector==0x14 || selector==0x19 || selector==0x56;
        CHECK(context.target_actor==65535 && context.target_city==32768 && context.magnitude==65535);
        CHECK(context.reverse==(keep_reverse?65535:0) && context.render_phase_13234e==(phase2?2:1) && context.completion==2);
    }
    unsupported([&] { initialize_resolver_frame_context(s,5,0,8,previous); });
    CHECK(previous.reverse==65535 && previous.completion==9);
    auto context=initialize_resolver_frame_context(s,5,65535,8,previous);
    CHECK(context.reverse==65535 && context.render_phase_13234e==99 && context.completion==2);
});
std::cout<<groups<<" groups, "<<checks<<" checks passed\n";
}
