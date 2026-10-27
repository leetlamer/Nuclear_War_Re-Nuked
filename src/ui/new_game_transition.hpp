#pragma once
#include <algorithm>
#include <cmath>
namespace nuclear::player {
// User-requested new-game presentation only. .4s/leg is a host approximation,
// not recovered Amiga timing. Contains no SDL, simulation, RNG or session state.
enum class NewGamePhase {idle,fade_out,reset_pending,fade_in};
class NewGameTransition {
 NewGamePhase phase_{NewGamePhase::idle};double elapsed_{},duration_{.4};bool reset_taken_{};
 static double smooth(double t){t=std::clamp(t,0.,1.);return t*t*(3.-2.*t);}
public:
 static constexpr double default_leg_seconds=.4;
 bool duration(double seconds){if(!std::isfinite(seconds)||seconds<.05||seconds>2.)return false;duration_=seconds;return true;}
 double leg_seconds()const{return duration_;}
 NewGamePhase phase()const{return phase_;}bool active()const{return phase_!=NewGamePhase::idle;}
 // Call only for a NEW completed high-score click; ready must exclude editing
 // and unfinished score persistence. Duplicate requests never restart a fade.
 bool request(bool ready){if(!ready||active())return false;phase_=NewGamePhase::fade_out;elapsed_=0;reset_taken_=false;return true;}
 // Optional initial new-game treatment; no reset ticket because no old session.
 bool initial_fade_in(){if(active())return false;phase_=NewGamePhase::fade_in;elapsed_=0;reset_taken_=false;return true;}
 void tick(double dt){if(!std::isfinite(dt)||dt<=0)return;
  if(phase_==NewGamePhase::fade_out||phase_==NewGamePhase::fade_in){elapsed_=std::min(duration_,elapsed_+dt);if(elapsed_>=duration_){if(phase_==NewGamePhase::fade_out)phase_=NewGamePhase::reset_pending;else {phase_=NewGamePhase::idle;elapsed_=0;}}}
 }
 // Normalised BLACK overlay alpha. Keep rendering underlying scene normally.
 double alpha()const{if(phase_==NewGamePhase::reset_pending)return 1.;if(phase_==NewGamePhase::fade_out)return smooth(elapsed_/duration_);if(phase_==NewGamePhase::fade_in)return 1.-smooth(elapsed_/duration_);return 0.;}
 bool take_reset(){if(phase_!=NewGamePhase::reset_pending||reset_taken_)return false;reset_taken_=true;return true;}
 // Reset authoritatively while completely black, then acknowledge exactly once.
 bool reset_done(){if(phase_!=NewGamePhase::reset_pending||!reset_taken_)return false;phase_=NewGamePhase::fade_in;elapsed_=0;return true;}
 // Cancellation on process exit/error never leaves an owned overlay behind.
 void cancel(){phase_=NewGamePhase::idle;elapsed_=0;reset_taken_=false;}
};
}
