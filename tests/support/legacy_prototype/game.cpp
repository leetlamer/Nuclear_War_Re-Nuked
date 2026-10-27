#include "tests/support/legacy_prototype/game.hpp"
#include "core/game_text.hpp"
#include <algorithm>
#include <sstream>
#include <stdexcept>

namespace prototype {
using namespace nuclear;
const char* leader(std::size_t i) {
    // Fixed prototype identity slots are unchanged; exact executable name literals.
    static const char* names[]{nuclear::text::table("leader_names",0).text.data(),
        nuclear::text::table("leader_names",1).text.data(),nuclear::text::table("leader_names",2).text.data(),
        nuclear::text::table("leader_names",3).text.data(),nuclear::text::lookup(0x1028ba).text.data()};
    return i<5?names[i]:"Unknown";
}
const char* outcome_name(Outcome o) {
    switch(o) {
    case Outcome::ongoing:return "In progress"; case Outcome::victory:return "VICTORY";
    case Outcome::defeat:return "DEFEAT"; case Outcome::draw:return "Mutual destruction";
    case Outcome::round_limit:return "Round limit reached";
    }
    return "Unknown";
}
std::string action_name(Word a) {
    switch(a) {
    case 0:return "idle"; case 1:return "nuclear missile"; case 2:return "prepare missile";
    case 3:return "propaganda"; case 4:return "prepare bomber"; case 5:return "bomber strike";
    case 6:return "production"; case 7:return "defense"; case 8:return "defectors";
    case 9:return "population growth"; case 11:return "city disaster"; case 12:return "falling weight";
    case 13:return "falling cow"; case 14:return "population transfer"; case 15:return "radiation";
    case 16:return "space aliens"; case 86:return "earthquake"; default:return "completed event";
    }
}
Game::Game(std::uint32_t seed,unsigned limit):rng(seed),initial_seed(seed),max_rounds(limit) {
    if(limit==0 || limit>10000) throw std::invalid_argument("round limit must be1..10000");
    // PROTOTYPE_APPROXIMATION: fixed leaders/profiles; no Amiga setup UI.
    for(std::size_t i=0;i<4;++i) { state.actors[i].profile=Word(i); state.identity[i]=Word(i); }
    initialize_state(state,0,rng);
}
bool Game::alive(std::size_t i) const { return i<5 && populated_cities(state.actors[i])!=0; }
std::string Game::validate(const Command& c) const {
    if(outcome!=Outcome::ongoing) return "This game has ended. Start another game.";
    if(!alive(4)) return "You have no surviving cities.";
    const auto& a=state.actors[4];
    if(c.kind==CommandKind::missile || c.kind==CommandKind::bomber || c.kind==CommandKind::propaganda) {
        if(c.target>=4 || !alive(c.target)) return "Choose a living opponent (1-4).";
        if(c.city>=5 || signed_word(state.actors[c.target].population[c.city])<=0)
            return "Choose a city with positive population (1-5).";
    }
    if(c.kind==CommandKind::missile || c.kind==CommandKind::bomber) {
        if(c.warhead>=4 || a.warheads[c.warhead]==0) return "That warhead is unavailable.";
        if(c.kind==CommandKind::missile) {
            if(c.delivery>=4 || a.missiles[c.delivery]==0) return "That missile is unavailable.";
            if(class_capacity(Word(c.delivery))<class_capacity(Word(c.warhead))) return "Warhead exceeds missile capacity.";
        } else {
            if(c.delivery>=2 || a.bombers[c.delivery]==0) return "That bomber is unavailable.";
            if(c.delivery*50+50<class_capacity(Word(c.warhead))) return "Warhead exceeds bomber capacity.";
        }
    }
    if(c.kind==CommandKind::defense && (c.delivery>=2 || a.defenses[c.delivery]==0))
        return "That defense is unavailable.";
    switch(c.kind) {
    case CommandKind::missile: case CommandKind::bomber: case CommandKind::propaganda:
    case CommandKind::production: case CommandKind::defense:return {};
    }
    return "Unknown command.";
}
void Game::human_command(const Command& c) {
    auto& a=state.actors[4];
    if(c.kind==CommandKind::missile || c.kind==CommandKind::bomber) {
        // PROTOTYPE_APPROXIMATION: prepare and launch in one terminal turn.
        const Word capacity=c.kind==CommandKind::missile?class_capacity(Word(c.delivery)):Word(c.delivery*50+50);
        human_commit(state,c.kind==CommandKind::missile?2:4,Word(c.delivery),capacity);
        a.target_actor=Word(c.target); a.target_city=Word(c.city); a.magnitude=class_effect(Word(c.warhead));
        human_commit(state,c.kind==CommandKind::missile?1:5,Word(c.warhead),class_capacity(Word(c.warhead)));
    } else if(c.kind==CommandKind::propaganda) {
        a.target_actor=Word(c.target); a.target_city=Word(c.city);
        HumanSelection selection; selection.pending_action=3;
        (void)roll_human_propaganda_fields(state,selection,rng);
        human_commit(state,3,0,0);
    } else if(c.kind==CommandKind::defense) {
        a.magnitude=Word(c.delivery); human_commit(state,7,Word(c.delivery),0);
    } else human_commit(state,6,0,0);
}
void Game::plan_ai() {
    // PROTOTYPE_APPROXIMATION: living actors only; isolate an undefined planner
    // path per actor rather than discarding all four opponents' decisions.
    for(std::size_t i=0;i<4;++i) {
        if(!alive(i)) continue;
        auto copy=state; auto random=rng;
        try {
            Word row=add_word(select_opponent(copy,i,random),2);
            auto& a=copy.actors[i];
            bool defended=(a.defenses[0]!=0 || a.defenses[1]!=0) && plan_defense(copy,i);
            if(!defended) {
                Word category=(a.action==4 || a.action==5 || a.action==2)?2:ai_category(row,a.profile,random);
                if(copy.identity[i]==9) category=1;
                const auto target=std::size_t(copy.selected_opponent[i]);
                if(category==1) plan_propaganda(copy,i,target,random);
                else if(category==2) plan_weapons(copy,i,target,random);
                else a.action=6;
            }
            state=copy; rng=random;
        } catch(const Unsupported& e) {
            state.actors[i].action=6;
            log.push_back(std::string(leader(i))+" produces instead [PROTOTYPE_APPROXIMATION: "+e.what()+"].");
        }
    }
    update_attitudes(state);
}
void Game::production(std::size_t i) {
    if(i<4) (void)produce(state,i,rng);
    else {
        // PROTOTYPE_APPROXIMATION: reuse verified inventory production through
        // an AI slot copy to omit human text/header RNG, without engine edits.
        State scratch; scratch.actors[0]=state.actors[4];
        (void)produce(scratch,0,rng); state.actors[4]=scratch.actors[0];
    }
}
void Game::resolve_immediate(std::size_t i) {
    if(i>=5) throw std::invalid_argument("actor outside0..4");
    if(!alive(i)) return;
    const auto before=state;
    auto copy=state; auto random=rng;
    const Word planned=copy.actors[i].action;
    const bool stockpile=copy.actors[i].magnitude==25;
    std::ostringstream message; message<<leader(i)<<": ";
    try {
        auto& a=copy.actors[i];
        // PROTOTYPE_APPROXIMATION: retarget a destroyed target immediately,
        // choosing first living opponent/city, with no extra random draws.
        if(a.action==1 || a.action==5 || a.action==3) {
            if(a.target_actor>=5 || a.target_actor==i || !alive(a.target_actor)) {
                for(std::size_t t=0;t<5;++t) if(t!=i && alive(t)) { a.target_actor=Word(t); break; }
            }
            if(a.target_actor>=5 || a.target_actor==i || !alive(a.target_actor)) return;
            if(a.target_city>=5 || signed_word(copy.actors[a.target_actor].population[a.target_city])<=0) {
                for(std::size_t c=0;c<5;++c) if(signed_word(copy.actors[a.target_actor].population[c])>0) { a.target_city=Word(c); break; }
            }
        }
        (void)apply_action3_prelude(copy,i,random);
        const Word action=a.action;
        message<<action_name(action);
        if(action==1 || action==5) {
            const auto target=a.target_actor,city=a.target_city;
            const auto roll=begin_attack_roll(action,random);
            message<<"; "<<nuclear::text::attack_announcement(action,roll.message_variant,leader(i),leader(target));
            const auto impact=evaluate_combat_impact(copy,i,target,city,roll.effect,stockpile,random);
            const Word loss=combat_population_commit(copy,i,target,city);
            message<<" -> "<<leader(target)<<" city "<<city+1<<", "<<signed_word(loss)<<" million lost";
            if(impact.effect==13) message<<" (intercepted)";
        } else if(action==2 || action==4 || action==6 || action==7) {
            const auto announcement=initialize_preparation_handler(copy,i,random);
            message<<"; "<<nuclear::text::action_announcement(action,announcement.message_variant,leader(i),leader(a.target_actor));
        } else if(action!=0) {
            const auto event=initialize_event_handler(a,random);
            message<<"; "<<nuclear::text::action_announcement(action,event.message_variant,leader(i),leader(a.target_actor));
            // Immediate single effect: current targets are latched implicitly
            // by synchronous execution; there are no intervening callbacks.
            apply_special_event_effect(copy,i,event.effect,random);
            if(action!=planned) message<<" (substituted for "<<action_name(planned)<<")";
        }
        state=copy; rng=random;
        if(action==6) { production(i); message<<"; inventory replenished"; }
        if(action!=1 && action!=5) {
            for(std::size_t t=0;t<5;++t) for(std::size_t c=0;c<5;++c) {
                const auto delta=signed_word(state.actors[t].population[c])-signed_word(before.actors[t].population[c]);
                if(delta!=0) message<<"; "<<leader(t)<<" city "<<c+1<<" "<<(delta>0?"+":"")<<delta<<" million";
            }
        }
        log.push_back(message.str());
    } catch(const Unsupported& e) {
        // PROTOTYPE_APPROXIMATION: failed event is a cancelled action, rollback
        // its mutations/draws; paid command resources stay paid. No fake damage.
        log.push_back(std::string(leader(i))+": action cancelled [PROTOTYPE_APPROXIMATION: "+e.what()+"].");
        state.actors[i].action=0;
    }
}
void Game::finish_if_needed() {
    const Word living=living_actors(state);
    if(living==0) outcome=Outcome::draw;
    else if(!alive(4)) outcome=Outcome::defeat;
    else if(living==1) outcome=Outcome::victory;
    else if(round>=max_rounds) outcome=Outcome::round_limit;
}
bool Game::play(const Command& c,std::string& error) {
    error=validate(c); if(!error.empty()) return false;
    Game next=*this; next.log.clear();
    next.human_command(c); next.plan_ai();
    // PROTOTYPE_APPROXIMATION: one ordinary action per living actor, index0..4.
    // No animation frames, dead-actor follow-up strikes, or processed flags.
    for(std::size_t i=0;i<5;++i) next.resolve_immediate(i);
    perturb_profile3_scores(next.state,next.rng);
    ++next.round; next.finish_if_needed(); *this=next; return true;
}
Command Game::automatic_command() const {
    Command c;
    const auto& a=state.actors[4];
    std::size_t target=0;
    for(std::size_t i=0;i<4;++i) if(alive(i)) { target=i; break; }
    c.target=target;
    for(std::size_t i=0;i<5;++i) if(signed_word(state.actors[target].population[i])>0) { c.city=i; break; }
    for(int w=3;w>=0;--w) if(a.warheads[std::size_t(w)]!=0) {
        for(std::size_t d=0;d<4;++d) if(a.missiles[d]!=0 && class_capacity(Word(d))>=class_capacity(Word(w))) {
            c.kind=CommandKind::missile; c.delivery=d; c.warhead=std::size_t(w); return c;
        }
        for(std::size_t d=0;d<2;++d) if(a.bombers[d]!=0 && d*50+50>=class_capacity(Word(w))) {
            c.kind=CommandKind::bomber; c.delivery=d; c.warhead=std::size_t(w); return c;
        }
    }
    c.kind=round%3==2?CommandKind::propaganda:CommandKind::production;
    return c;
}
std::string state_digest(const Game& g) {
    // Stable textual serialization for deterministic tests; no platform padding.
    std::ostringstream s; s<<g.initial_seed<<':'<<g.rng.seed()<<':'<<g.round<<':'<<int(g.outcome);
    for(const auto& a:g.state.actors) for(auto b:encode_actor(a)) s<<':'<<unsigned(b);
    for(auto w:g.state.prepared_capacity) s<<':'<<w;
    for(auto w:g.state.selected_opponent) s<<':'<<w;
    s<<':'<<g.state.human_prepared_capacity;
    return s.str();
}
}
