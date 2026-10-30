// Diagnostic only: exercise the production renderer with deterministic ending/event state.
#include "rendering/renderer.hpp"
#include "tests/support/session_fixtures.hpp"
int main(int argc, char **argv) {
    if (argc != 2) return 2;
    std::filesystem::create_directories(argv[1]);
    Renderer display;
    Player roster;
    roster.enter_selection();
    auto r = portrait_rect(0);
    roster.click(r.x+20,r.y+15);
    roster.click(110,20);
    display.render(roster,false);
    display.capture(std::string(argv[1])+"/one-opponent.bmp");
    auto player = Player::developer_session(endgame_fixture(false,0));
    for (int i=0;i<30 && !player.ended();++i) player.advance_turn(37);
    EndgamePresentation ending;
    if (!ending.begin(player,false)) return 3;
    display.endgame=&ending;
    double previous=0;
    for (int stage=-1;stage<=10;++stage) {
        double time= stage<0 ? 0 : extinction_earth_seconds;
        for(int i=0;i<stage && i<10;++i) time+=extinction_frames[std::size_t(i)].seconds;
        if(stage==10) time=extinction_duration()+.01;
        else if(stage>=0) time+=extinction_frames[std::size_t(stage)].seconds/2;
        while(ending.elapsed()+.000001<time) ending.tick(std::min(.005,time-ending.elapsed()));
        display.render(player,false);
        display.capture(std::string(argv[1])+"/earth-"+std::to_string(stage)+".bmp");
        previous=time;
    }
    display.endgame=nullptr;
    for(double time : {0.0,1.0,2.8}) {
        auto state=roster.coordinator()->data().state;
        ActionEvent event;
        event.actor=0; event.target=0; event.city=0; event.action=16;
        event.media.supported=true;
        auto scene=ActionPresentation::replay_event(event,state);
        scene.tick(time);
        display.render(roster,false,&scene);
        display.capture(std::string(argv[1])+"/alien-"+std::to_string(time)+".bmp");
    }
    auto before = roster.coordinator()->data().state;
    auto after = before;
    after.actors[4].population[0] = 0;
    for (double time : {1., 2.9, 3., 3.2}) {
        ActionEvent event;
        event.region = EventRegion::action;
        event.actor = event.target = 4; event.city = 0; event.action = 11;
        event.media.supported = true;
        event.media.population_before = before.actors[4].population[0];
        event.before = before; event.after = after;
        auto scene = ActionPresentation::replay_event(event,before);
        scene.tick(time);
        display.render(roster,false,&scene);
        display.capture(std::string(argv[1])+"/city-lift-"+std::to_string(time)+".bmp");
    }

}
