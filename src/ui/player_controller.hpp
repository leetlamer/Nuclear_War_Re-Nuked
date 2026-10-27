#pragma once
#include "nuclear/session.hpp"
#include "game/turn_resolver_adapter.hpp"
#include <memory>
#include "rendering/asset_layout.hpp"
namespace nuclear::player {
struct Rect{int x,y,w,h;bool contains(int px,int py)const{return px>=x&&py>=y&&px<x+w&&py<y+h;}};
Rect portrait_rect(std::size_t identity);bool selection_header(int x,int y);
struct Completion{std::array<Word,4> identity{};unsigned fill_draws{},assignment_draws{};};
Completion complete_selection(std::array<Word,10>&,Rng&);
struct Selection {
 std::array<Word,10> flags{},timers{}; Word count{};Rng rng;
 explicit Selection(std::uint32_t seed):rng(seed){for(auto& timer:timers){auto first=rng.next();timer=Word(first%75+rng.next()%75+2);}}
 bool toggle(Word identity);
};
enum class Screen{title,selection,main}; // presentation navigation only, not turn state
class StaticSchedule:public session::CallbackSchedule {
public:void apply(session::CallbackPoint,session::Data&) override {} // explicit no native animation callbacks
};
class Player {
 Screen screen_{Screen::title};Selection selection_;TurnResolverAdapter setup_;StaticSchedule schedule_;
 State committed_quantities_{};
 std::unique_ptr<session::Coordinator> coordinator_;std::vector<std::string> messages_;
 std::uint64_t bleep_serial_{};unsigned bleep_period_{700};
 bool resolution_failed_{};unsigned round_{1};std::vector<std::string> turn_log_,activity_;
 std::string diagnostic_;std::uint64_t payload_visual_serial_{},selection_visual_serial_{};Word selection_visual_action_{};bool accepted_action_{};bool complete_selection_screen();
public:
 explicit Player(std::uint32_t seed=1):selection_(seed){}
 std::uint64_t bleep_serial()const{return bleep_serial_;}unsigned bleep_period()const{return bleep_period_;}
 static Player developer_session(session::Data);
 bool human_alive()const{return coordinator_&&populated_cities(coordinator_->data().state.actors[4])!=0;}
 bool ended()const{return coordinator_&&coordinator_->phase()==session::Phase::exited;}
 bool accepted_action()const{return accepted_action_;}
 Word selection_visual_action()const{return selection_visual_action_;}
 std::uint64_t selection_visual_serial()const{return selection_visual_serial_;}
 Screen screen()const{return screen_;}const Selection& selection()const{return selection_;}
 State display_state()const{auto s=coordinator_->data().state;for(std::size_t i=0;i<5;++i){auto& a=s.actors[i];const auto& old=committed_quantities_.actors[i];a.population=old.population;a.missiles=old.missiles;a.warheads=old.warheads;a.bombers=old.bombers;a.defenses=old.defenses;}return s;}
 const session::Coordinator* coordinator()const{return coordinator_.get();}
 const std::vector<std::string>& messages()const{return messages_;}const std::string& diagnostic()const{return diagnostic_;}
 void enter_selection(){if(screen_==Screen::title)screen_=Screen::selection;}
 std::uint64_t payload_visual_serial()const{return payload_visual_serial_;}
 unsigned round()const{return round_;}const std::vector<std::string>& turn_log()const{return turn_log_;}
 const std::vector<std::string>& activity()const{return activity_;}
 bool advance_turn(Word microseconds_low16);
 bool click(int x,int y);bool command(HumanCommand);bool confirm_banner();
 Rect city_rect(std::size_t actor,std::size_t city)const;
};
Word city_frame(Word population);
}
