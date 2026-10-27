#include "nuclear/session.hpp"
#include "nuclear/round_plan.hpp"
#include <algorithm>
#include <cstdlib>
#include <iostream>
using namespace nuclear;
using namespace nuclear::session;
namespace {
unsigned checks=0,groups=0;
void check(bool b,int line) { ++checks; if(!b) { std::cerr<<"Failure line "<<line<<'\n'; std::exit(1); } }
#define CHECK(x) check(bool(x),__LINE__)
void group(const char* name,const std::function<void()>& run) { run(); ++groups; std::cout<<"PASS "<<name<<'\n'; }
// Explicit test-only schedule. Zero callback events is a supplied fixture,
// not the library's default and not a recovered native interrupt frequency.
struct Schedule:CallbackSchedule {
    std::function<void(CallbackPoint,Data&)> script=[](CallbackPoint,Data&){};
    void apply(CallbackPoint p,Data& d) override { script(p,d); }
};
struct Script:Boundaries {
    std::function<Word(Data&)> on_setup=[](Data&){return Word(0);};
    std::function<Word(Data&,const ResolverRequest&,CallbackSchedule&)> on_resolve=
        [](Data&,const ResolverRequest&,CallbackSchedule&){return Word(0);};
    std::function<Word(Data&)> on_ending=[](Data&){return Word(0);};
    unsigned setup_calls{},resolver_calls{},ending_calls{};
    Word setup(Data& d,CallbackSchedule&) override { ++setup_calls; return on_setup(d); }
    Word resolve(Data& d,const ResolverRequest& q,CallbackSchedule& c) override { ++resolver_calls; return on_resolve(d,q,c); }
    Word ending(Data& d,CallbackSchedule&) override { ++ending_calls; return on_ending(d); }
};
Data fixture() {
    Data d(1,true); for(auto& actor:d.state.actors) actor.population[0]=10;
    d.names={"A","B","C","D","E"}; return d;
}
void same_state(const State& a,const State& b) {
    for(std::size_t i=0;i<5;++i) CHECK(encode_actor(a.actors[i])==encode_actor(b.actors[i]));
    CHECK(a.prepared_capacity==b.prepared_capacity); CHECK(a.selected_opponent==b.selected_opponent);
    CHECK(a.identity==b.identity); CHECK(a.remaining_total==b.remaining_total);
    CHECK(a.pass_processed==b.pass_processed); CHECK(a.human_prepared_capacity==b.human_prepared_capacity);
}
void same_data(const Data& a,const Data& b) {
    same_state(a.state,b.state); CHECK(a.rng.seed()==b.rng.seed()); CHECK(a.names==b.names);
    CHECK(a.welcome_flag==b.welcome_flag); CHECK(a.text.at_initial_origin==b.text.at_initial_origin);
    CHECK(a.human.active==b.human.active); CHECK(a.human.logical_view==b.human.logical_view);
    CHECK(a.human.display_actor==b.human.display_actor); CHECK(a.human.target_marker==b.human.target_marker);
    CHECK(a.human.control_return==b.human.control_return);
    CHECK(a.human.selection.pending_action==b.human.selection.pending_action);
    CHECK(a.human.selection.selected_class==b.human.selection.selected_class);
    CHECK(a.human.selection.selected_capacity==b.human.selection.selected_capacity);
}
void reach(Coordinator& c,Adapters& a,Phase p) {
    unsigned steps=0;
    while(c.phase()!=p && steps++<25) CHECK(c.advance(a,c.phase()==Phase::resolver_handoff?std::optional<Word>(123):std::nullopt));
    CHECK(c.phase()==p);
}
void ready(Coordinator& c,Adapters& a) { reach(c,a,Phase::human_selection); }
void build(Coordinator& c,Adapters& a) {
    CHECK(c.select({HumanCommandKind::production,0},a));
    CHECK(c.phase()==Phase::human_selection);
    CHECK(c.request_banner_confirmation()); CHECK(c.phase()==Phase::banner_confirmation);
    CHECK(c.advance(a)); CHECK(c.phase()==Phase::ai_planning);
}
unsigned inventory(const Actor& a) {
    unsigned n=0; for(auto v:a.missiles)n+=v;for(auto v:a.warheads)n+=v;
    for(auto v:a.bombers)n+=v;for(auto v:a.defenses)n+=v;return n;
}
}
int main() {
 group("ordinary round phases and independent confirmation",[] {
    Script s; Schedule cb; Adapters a{s,cb}; Coordinator c(fixture()); ready(c,a);
    const auto seed=c.data().rng.seed(); CHECK(c.request_banner_confirmation());
    CHECK(c.data().rng.seed()==seed); CHECK(c.advance(a));
    CHECK(c.phase()==Phase::human_selection && c.events().back().human_result);
    CHECK(c.events().back().human_result->messages[0]=="Select an action");
    CHECK(!c.select({HumanCommandKind::confirm,0},a)); CHECK(c.diagnostic());
    build(c,a); reach(c,a,Phase::resolver_handoff);
    const auto retained=c.retained_control(); CHECK(c.advance(a,0x8001));
    CHECK(c.phase()==Phase::ai_production && retained==0 && c.retained_control()==0);
    CHECK(c.events().back().resolver_request->actor_order==(std::array<std::size_t,5>{0,1,2,3,4}));
    CHECK(c.events().back().resolver_request->entry.human_cities_before==1);
    reach(c,a,Phase::living_human_entry); CHECK(s.resolver_calls==1);
    std::vector<Phase> suffix;
    for(const auto& e:c.events()) if(e.diagnostic.empty() && e.from>=Phase::ai_planning) suffix.push_back(e.from);
    for(std::size_t i=1;i<c.events().size();++i)
        CHECK(c.events()[i].seed_before==c.events()[i-1].seed_after);
    same_state(c.events().back().state_after,c.data().state);
    for(const auto& event:c.events())
        std::cout<<"TRACE "<<phase_name(event.from)<<" -> "<<phase_name(event.to)
          <<" control="<<event.retained_control<<" resolver="<<event.resolver_result
          <<" seed_before="<<event.seed_before<<" seed_after="<<event.seed_after
          <<" blocked="<<!event.diagnostic.empty()<<'\n';
    CHECK(suffix==(std::vector<Phase>{Phase::ai_planning,Phase::attitudes,Phase::human_control,
      Phase::resolver_handoff,Phase::ai_production,Phase::resolver_control,
      Phase::post_resolver_relationships,Phase::survivor_counting}));
 });
 group("setup any nonzero exits including signed word boundaries",[] {
    for(Word code:{Word(1),Word(2),Word(3),Word(0x8000),Word(0xffff)}) {
        Script s; s.on_setup=[=](Data&){return code;};Schedule cb;Adapters a{s,cb};Coordinator c(fixture());
        CHECK(c.advance(a));CHECK(c.phase()==Phase::exited && c.retained_control()==code);
        CHECK(s.resolver_calls==0);CHECK(!c.advance(a));CHECK(s.setup_calls==1);
    }
 });
 group("human cancellation still plans all AI and attitudes before exact2 exit or1 restart",[] {
    for(Word menu:{Word(0),Word(32)}) {
        Script s;Schedule cb;Adapters a{s,cb};Coordinator c(fixture());ready(c,a);
        CHECK(c.select({HumanCommandKind::menu,menu},a));CHECK(c.phase()==Phase::ai_planning);
        const Word code=menu==0?1:2; CHECK(c.retained_control()==code);
        auto expected=c.data().state;auto rng=c.data().rng;
        const auto plan=prepare_round_plan(expected,code,rng);CHECK(!plan.should_resolve);
        CHECK(c.advance(a));CHECK(c.phase()==Phase::attitudes);
        CHECK(c.advance(a));same_state(c.data().state,expected);CHECK(c.data().rng.seed()==rng.seed());
        CHECK(c.phase()==Phase::human_control);CHECK(c.advance(a));CHECK(c.phase()==Phase::return_routing);
        CHECK(c.advance(a));CHECK(c.phase()==(code==2?Phase::exited:Phase::restart));
        CHECK(s.resolver_calls==0 && s.ending_calls==0);
        if(code==1) { const bool cursor=c.data().text.at_initial_origin;CHECK(c.advance(a));
            CHECK(c.phase()==Phase::setup);CHECK(c.data().text.at_initial_origin==cursor);
            CHECK(c.advance(a));CHECK(s.setup_calls==2 && c.retained_control()==0); }
    }
 });
 group("resolver nonzero never overwrites retained human0 and restarts even on2",[] {
    for(Word code:{Word(1),Word(2),Word(0x8000),Word(0xffff)}) {
        Script s;s.on_resolve=[=](Data&,const ResolverRequest&,CallbackSchedule&){return code;};
        Schedule cb;Adapters a{s,cb};Coordinator c(fixture());ready(c,a);build(c,a);
        reach(c,a,Phase::resolver_handoff);CHECK(c.advance(a,9));CHECK(c.resolver_result()==code);
        CHECK(c.retained_control()==0);CHECK(c.phase()==Phase::ai_production);CHECK(c.advance(a));
        CHECK(c.events().back().ai_production);CHECK(c.advance(a));CHECK(c.phase()==Phase::return_routing);
        CHECK(c.advance(a));CHECK(c.phase()==Phase::restart && s.ending_calls==0);
    }
 });
 group("ending exact2 exits and all other tested words restart full setup",[] {
    for(Word code:{Word(0),Word(1),Word(2),Word(0x8000),Word(0xffff)}) {
        Script s;s.on_resolve=[](Data& d,const ResolverRequest&,CallbackSchedule&){
            for(std::size_t i=0;i<4;++i)d.state.actors[i].population={};return Word(0);};
        s.on_ending=[=](Data&){return code;};Schedule cb;Adapters a{s,cb};Coordinator c(fixture());
        ready(c,a);build(c,a);reach(c,a,Phase::ending);CHECK(s.ending_calls==0);
        CHECK(c.advance(a));CHECK(c.retained_control()==code);CHECK(c.phase()==Phase::return_routing);
        CHECK(c.advance(a));CHECK(c.phase()==(code==2?Phase::exited:Phase::restart));CHECK(s.ending_calls==1);
    }
 });
 group("survivor decision for all32 alive masks after resolver0",[] {
    for(unsigned mask=0;mask<32;++mask) {
        Script s;s.on_resolve=[=](Data& d,const ResolverRequest&,CallbackSchedule&){
            for(unsigned i=0;i<5;++i) { d.state.actors[i].population={};if(mask&(1u<<i))d.state.actors[i].population[0]=1; }
            return Word(0);};Schedule cb;Adapters a{s,cb};Coordinator c(fixture());ready(c,a);build(c,a);
        reach(c,a,Phase::survivor_counting);unsigned n=0;for(unsigned i=0;i<5;++i)if(mask&(1u<<i))++n;
        CHECK(c.advance(a));CHECK(c.events().back().survivors==n);
        CHECK(c.phase()==(n<=1?Phase::ending:Phase::living_human_entry));
    }
 });
 group("AI-only continuation skips dead human entry and still plans dead AI",[] {
    auto d=fixture(); d.state.actors[4].population={};d.state.actors[4].action=6;
    d.state.actors[0].population={}; d.state.actors[0].action=2;d.state.prepared_capacity[0]=10;
    Script s;Schedule cb;Adapters a{s,cb};Coordinator c(d);CHECK(c.advance(a));
    const auto before=c.data();CHECK(c.advance(a));CHECK(c.phase()==Phase::ai_planning);
    same_data(c.data(),before); CHECK(c.events().back().operation=="dead_human_entry_skipped");
    auto expected=c.data().state;auto er=c.data().rng;ai_turn(expected,er);
    CHECK(c.advance(a));same_state(c.data().state,expected);CHECK(c.data().rng.seed()==er.seed());
    CHECK(c.data().state.actors[0].action==1); // dead actor was planned, not skipped
    reach(c,a,Phase::survivor_counting);CHECK(c.advance(a));CHECK(c.phase()==Phase::living_human_entry);
    CHECK(c.advance(a));CHECK(c.phase()==Phase::ai_planning && c.retained_control()==0);
    CHECK(s.ending_calls==0 && inventory(c.data().state.actors[4])==0);
 });
 group("deferred human manufacture occurs once at next living entry not resolver exit",[] {
    Script s;Schedule cb;Adapters a{s,cb};Coordinator c(fixture());ready(c,a);
    CHECK(inventory(c.data().state.actors[4])==0);build(c,a);
    CHECK(inventory(c.data().state.actors[4])==0);reach(c,a,Phase::living_human_entry);
    CHECK(inventory(c.data().state.actors[4])==0);
    auto expected=c.data();auto entry=start_human_commands(expected.state,expected.human.selection.selected_class,
        expected.human.selection.selected_capacity,expected.names,expected.welcome_flag,expected.text,expected.rng);
    expected.human=entry.command_state;CHECK(c.advance(a));same_data(c.data(),expected);
    CHECK(c.events().back().human_entry->prefix.produced && inventory(c.data().state.actors[4])==2);
    CHECK(!c.advance(a)); CHECK(inventory(c.data().state.actors[4])==2);
 });
 group("no next human entry at ending means no deferred human production",[] {
    Script s;s.on_resolve=[](Data& d,const ResolverRequest&,CallbackSchedule&){
        for(std::size_t i=0;i<4;++i)d.state.actors[i].population={};return Word(0);};
    Schedule cb;Adapters a{s,cb};Coordinator c(fixture());ready(c,a);build(c,a);reach(c,a,Phase::ending);
    CHECK(inventory(c.data().state.actors[4])==0);
 });
 group("abort AI production uses current survivors once and excludes human",[] {
    Script s;s.on_resolve=[](Data& d,const ResolverRequest&,CallbackSchedule&){
        for(unsigned i=0;i<4;++i) { d.state.actors[i].action=6; d.state.actors[i].population={}; }
        d.state.actors[0].population[0]=1;d.state.actors[1].population[0]=1;d.state.actors[1].population[4]=1;
        return Word(2);};Schedule cb;Adapters a{s,cb};Coordinator c(fixture());ready(c,a);build(c,a);
    reach(c,a,Phase::ai_production);auto expected=c.data().state;auto er=c.data().rng;
    auto report=finish_ai_production(expected,er);CHECK(c.advance(a));same_state(c.data().state,expected);
    CHECK(c.data().rng.seed()==er.seed());CHECK(c.events().back().ai_production->produced==report.produced);
    CHECK(inventory(c.data().state.actors[0])==2 && inventory(c.data().state.actors[1])==3);
    CHECK(inventory(c.data().state.actors[2])==0 && inventory(c.data().state.actors[4])==0);
    const auto after=c.data();CHECK(c.advance(a));CHECK(c.advance(a));same_data(c.data(),after);
 });
 group("only resolver adapter commits population and coordinator does not duplicate it",[] {
    Script s;s.on_resolve=[](Data& d,const ResolverRequest&,CallbackSchedule&){
        d.state.actors[0].magnitude=3;CHECK(combat_population_commit(d.state,0,4,0)==3);return Word(0);};
    Schedule cb;Adapters a{s,cb};Coordinator c(fixture());ready(c,a);build(c,a);
    reach(c,a,Phase::living_human_entry);CHECK(c.data().state.actors[4].population[0]==7);
    CHECK(c.advance(a));CHECK(c.data().state.actors[4].population[0]==7 && s.resolver_calls==1);
 });
 group("external reseed accepted only at handoff and exactly word zero-extended",[] {
    for(Word word:{Word(0),Word(0x8000),Word(0xffff)}) {
        Script s;s.on_resolve=[=](Data& d,const ResolverRequest& q,CallbackSchedule& cb){
            CHECK(q.microseconds_low16==word && d.rng.seed()==word);
            cb.apply(CallbackPoint::resolver_owned,d);return Word(0);};
        Schedule cb;cb.script=[](CallbackPoint p,Data& d){if(p==CallbackPoint::resolver_owned)(void)d.rng.next();};
        Adapters a{s,cb};Coordinator c(fixture());const auto initial=c.data();CHECK(!c.advance(a,word));same_data(c.data(),initial);
        ready(c,a);build(c,a);reach(c,a,Phase::resolver_handoff);const auto before=c.data();
        CHECK(!c.advance(a));same_data(c.data(),before);CHECK(s.resolver_calls==0);
        CHECK(c.advance(a,word));Rng er(word);(void)er.next();CHECK(c.data().rng.seed()==er.seed());
    }
 });
 group("explicit callback schedule preserves supplied boundary RNG ordering",[] {
    Script s;Schedule cb;Adapters a{s,cb};Coordinator c(fixture());ready(c,a);
    cb.script=[](CallbackPoint p,Data& d){if(p==CallbackPoint::before_human_command || p==CallbackPoint::after_human_command)(void)d.rng.next();};
    auto expected=c.data();(void)expected.rng.next();auto result=apply_human_command(expected.state,expected.human,
        {HumanCommandKind::production,0},expected.names,expected.text,expected.rng);(void)expected.rng.next();
    CHECK(c.select({HumanCommandKind::production,0},a));same_data(c.data(),expected);
    CHECK(c.events().back().human_result->messages==result.messages);
 });
 group("unknown callbacks default to Unsupported with transactional human entry",[] {
    Script s;CallbackSchedule cb;Adapters a{s,cb};Coordinator c(fixture());CHECK(c.advance(a));
    const auto before=c.data();CHECK(!c.advance(a));same_data(c.data(),before);
    CHECK(c.phase()==Phase::living_human_entry && c.diagnostic()->message.find("callback")!=std::string::npos);
 });
 group("default native setup resolver and ending produce diagnostics",[] {
    Boundaries unknown;Schedule cb;Adapters u{unknown,cb};Coordinator missing(fixture());
    CHECK(!missing.advance(u));CHECK(missing.phase()==Phase::setup);
    Script s;Adapters a{s,cb};Coordinator c(fixture());ready(c,a);build(c,a);reach(c,a,Phase::resolver_handoff);
    const auto before=c.data();CHECK(!c.advance(u,99));same_data(c.data(),before);
    CHECK(c.diagnostic()->message.find("full resolver/FFP")!=std::string::npos);
    CHECK(c.retained_control()==0 && c.phase()==Phase::resolver_handoff);
    s.on_resolve=[](Data& d,const ResolverRequest&,CallbackSchedule&){for(auto& actor:d.state.actors)actor.population={};return Word(0);};
    reach(c,a,Phase::ending);CHECK(!c.advance(u));CHECK(c.phase()==Phase::ending);
    CHECK(c.diagnostic()->message.find("ending")!=std::string::npos);
 });
 group("bomber fallback does not strand the session in AI planning",[] {
    auto d=fixture();d.state.actors[0].population={};auto& actor=d.state.actors[0];
    actor.warheads[0]=3;actor.bombers[0]=3;actor.profile=1;
    Script s;Schedule cb;Adapters a{s,cb};Coordinator c(d);ready(c,a);build(c,a);CHECK(c.phase()==Phase::ai_planning);
    CHECK(c.advance(a));CHECK(!c.diagnostic());
    CHECK(c.phase()==Phase::attitudes && c.data().state.actors[0].action==0);
 });
 group("undefined followup uses existing guard diagnostic with no guessed result",[] {
    Script s;s.on_resolve=[](Data& d,const ResolverRequest&,CallbackSchedule&){
        (void)prepare_resolver_action_prelude(d.state,0,1,d.rng);return Word(0);};
    auto d=fixture();d.state.actors[0].population={};Schedule cb;Adapters a{s,cb};Coordinator c(d);
    ready(c,a);build(c,a);reach(c,a,Phase::resolver_handoff);const auto before=c.data();
    CHECK(!c.advance(a,12));same_data(c.data(),before);CHECK(c.phase()==Phase::resolver_handoff);
    CHECK(c.diagnostic()->message.find("follow")!=std::string::npos);
 });
 group("callback rejection after command rolls back command inventory and trace reports",[] {
    Script s;Schedule cb;Adapters a{s,cb};Coordinator c(fixture());ready(c,a);build(c,a);
    reach(c,a,Phase::living_human_entry);CHECK(c.advance(a));const auto before=c.data();
    cb.script=[](CallbackPoint p,Data& d){if(p==CallbackPoint::after_human_command){d.state.actors[4].population[0]=999;(void)d.rng.next();throw Unsupported("intra-slice callback ordering unknown");}};
    CHECK(!c.select({HumanCommandKind::production,0},a));same_data(c.data(),before);
    CHECK(!c.events().back().human_result && !c.events().back().diagnostic.empty());
    same_state(c.events().back().state_after,before.state);
 });
 group("post-resolver perturbation uses verified profile3 slice only on zero return",[] {
    Script s;s.on_resolve=[](Data& d,const ResolverRequest&,CallbackSchedule&){
        for(auto& actor:d.state.actors)actor.action=0;d.state.actors[0].profile=3;return Word(0);};
    Schedule cb;Adapters a{s,cb};Coordinator c(fixture());ready(c,a);build(c,a);reach(c,a,Phase::post_resolver_relationships);
    auto expected=c.data().state;auto er=c.data().rng;perturb_profile3_scores(expected,er);
    CHECK(c.advance(a));same_state(c.data().state,expected);CHECK(c.data().rng.seed()==er.seed());
    CHECK(c.phase()==Phase::survivor_counting);
 });
 group("failed attitude update preserves previously completed planning and can retry",[] {
    auto d=fixture();d.state.actors[4].classifications[0]=127;
    Script s;Schedule cb;Adapters a{s,cb};Coordinator c(d);ready(c,a);build(c,a);
    CHECK(c.advance(a));const auto before=c.data();CHECK(c.phase()==Phase::attitudes);
    CHECK(!c.advance(a));same_data(c.data(),before);CHECK(c.phase()==Phase::attitudes);
    CHECK(s.resolver_calls==0);CHECK(!c.advance(a));same_data(c.data(),before);
 });
 group("normalized target selection and missile banner commit retain golden RNG",[] {
    auto d=fixture();d.state.actors[4].missiles[0]=1;d.state.actors[4].warheads[0]=1;
    d.human.selection.selected_class=0; // explicit prior global, as in engine golden fixture
    Script s;Schedule cb;Adapters a{s,cb};Coordinator c(d);ready(c,a);
    CHECK(c.select({HumanCommandKind::actor_view,1},a));
    CHECK(c.select({HumanCommandKind::city_target,2},a));
    CHECK(c.data().human.target_marker==1 && c.data().state.actors[4].target_city==2);
    CHECK(c.select({HumanCommandKind::missile,1},a));
    CHECK(c.data().state.actors[4].missiles[0]==1 && c.data().state.actors[4].action==0);
    CHECK(c.request_banner_confirmation());CHECK(c.data().state.actors[4].missiles[0]==1);
    CHECK(c.advance(a));CHECK(c.data().state.actors[4].action==2);
    CHECK(c.data().state.actors[4].missiles[0]==0 && c.data().state.human_prepared_capacity==10);
    // Same instruction-derived fixture as preserved engine_tests (no earlier rejected q here).
    auto expected=d;auto start=start_human_commands(expected.state,0,0,expected.names,expected.welcome_flag,expected.text,expected.rng);
    expected.human=start.command_state;
    for(const HumanCommand command: {HumanCommand{HumanCommandKind::actor_view,1},
        HumanCommand{HumanCommandKind::city_target,2},HumanCommand{HumanCommandKind::missile,1},
        HumanCommand{HumanCommandKind::confirm,0}})
        (void)apply_human_command(expected.state,expected.human,command,expected.names,expected.text,expected.rng);
    same_data(c.data(),expected);CHECK(!c.request_banner_confirmation());
    CHECK(c.data().state.actors[4].missiles[0]==0);
 });
 group("propaganda banner missing-target rejection returns to selection without AI",[] {
    Script s;Schedule cb;Adapters a{s,cb};Coordinator c(fixture());ready(c,a);
    CHECK(c.select({HumanCommandKind::propaganda,0},a));
    CHECK(c.request_banner_confirmation());CHECK(c.advance(a));
    CHECK(c.phase()==Phase::human_selection && !c.events().back().human_result->turn_finished);
    CHECK(c.events().back().human_result->messages[0]=="Choose a target city");
    CHECK(c.data().state.actors[4].action==0 && s.resolver_calls==0);
 });
 group("after-entry callback rejection retries without duplicate human manufacture",[] {
    auto d=fixture();d.state.actors[4].action=6;Script s;Schedule cb;Adapters a{s,cb};Coordinator c(d);
    CHECK(c.advance(a));const auto before=c.data();
    cb.script=[](CallbackPoint p,Data&){if(p==CallbackPoint::after_human_entry)throw Unsupported("unknown native wait interleaving");};
    CHECK(!c.advance(a));same_data(c.data(),before);CHECK(!c.events().back().human_entry);
    cb.script=[](CallbackPoint,Data&){};CHECK(c.advance(a));
    CHECK(inventory(c.data().state.actors[4])==2 && c.events().back().human_entry->prefix.produced);
    CHECK(!c.advance(a));CHECK(inventory(c.data().state.actors[4])==2);
 });
 group("invalid phase commands and banner requests preserve setup state",[] {
    Script s;Schedule cb;Adapters a{s,cb};Coordinator c(fixture());const auto before=c.data();
    CHECK(!c.select({HumanCommandKind::production,0},a));CHECK(!c.request_banner_confirmation());
    same_data(c.data(),before);CHECK(c.phase()==Phase::setup && s.setup_calls==0);
 });
 std::cout<<groups<<" groups, "<<checks<<" checks passed\n";
}
