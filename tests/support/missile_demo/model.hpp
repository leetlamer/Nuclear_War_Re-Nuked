#pragma once
#include "nuclear/single_missile.hpp"
#include <memory>
namespace nuclear::missile_prototype {
enum class Phase { selection, prepared, launch_ready, flight, effect, awaiting_ack, complete };
struct Timeline { double flight_seconds{4},effect_seconds{1.2}; };
struct Pose { double x{},y{},scale{},angle{}; };
// PROTOTYPE_APPROXIMATION: Bezier flight, scale/rotation and durations; zero RNG.
Pose visual_pose(double progress,double source_x,double source_y,double target_x,double target_y);
class Demo {
    State state_{}; Rng rng_; Phase phase_{Phase::selection}; Timeline timeline_;
    Word missile_{1},warhead_{1},city_{2},defense_{2}; // defense2 means no defense
    double elapsed_{};
    std::optional<resolver::MissilePrefix> prefix_;
    std::unique_ptr<resolver::DamageAcknowledgment> damage_;
    Word loss_{}; bool intercepted_{};
    unsigned impacts_{},commits_{},acknowledgments_{};
    std::string message_;
public:
    explicit Demo(std::uint32_t seed=1,Timeline timeline={});
    const State& state() const {return state_;} std::uint32_t seed()const{return rng_.seed();}
    Phase phase()const{return phase_;} unsigned turn()const{return phase_==Phase::selection||phase_==Phase::prepared?1:2;}
    Word missile()const{return missile_;} Word warhead()const{return warhead_;} Word city()const{return city_;}
    Word defense()const{return defense_;} Word loss()const{return loss_;} bool intercepted()const{return intercepted_;}
    unsigned impacts()const{return impacts_;} unsigned commits()const{return commits_;} unsigned acknowledgments()const{return acknowledgments_;}
    const std::string& message()const{return message_;}
    const std::optional<resolver::MissilePrefix>& prefix()const{return prefix_;}
    double progress()const; double effect_progress()const;
    bool choose_missile(Word); bool choose_warhead(Word); bool choose_city(Word); bool choose_defense(Word);
    bool prepare(); bool advance_turn(); bool launch();
    void tick(double seconds); // wall time only, never used as RNG input
    void after_present(); // PROTOTYPE_ASSUMPTION: SDL present -> supplied request0
};
const char* phase_name(Phase);
}
