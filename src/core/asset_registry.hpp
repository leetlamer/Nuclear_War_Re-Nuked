#pragma once
#include <array>
#include <string_view>
namespace nuclear::assets {
struct ImageSpec{std::string_view key,mask,file;unsigned width,height;};
inline constexpr std::array<ImageSpec,13> images{{
    {"start","","images/backgrounds/character_selection.png",320,200},
    {"dip","","images/interface/main_window.png",320,200},
    {"leaders","","images/characters/leader_portraits.png",320,160},
    {"world_map","","images/backgrounds/world_map.png",320,200},
    {"smalltiles","","images/interface/control_tiles.png",320,272},
    {"smalltiles","mask_smallmask_z_m","images/effects/small_sprites.png",320,272},
    {"big512","mask_big512mask","images/missiles/flight_and_effects.png",320,288},
    {"bigtiles","mask_bigmask","images/effects/large_sprites.png",320,432},
    {"bigtiles","","images/interface/selection_backdrops.png",320,432},
    {"endchar","mask_endchar_mask","images/characters/victory_body.png",320,112},
    {"spacemap","","images/backgrounds/space_ending.png",320,200},
    {"desolate_map","","images/backgrounds/desolate_ending.png",320,200},
    {"font","","fonts/original_bitmap.png",576,8},
}};
struct AudioSpec{std::string_view key,file;unsigned samples,rate;};
inline constexpr std::array<AudioSpec,13> audio{{
    {"16ton","audio/effects/falling_weight.wav",5406,4961},
    {"build","audio/effects/factory.wav",20032,27284},
    {"door.snd","audio/effects/shutter.wav",8820,8547},
    {"engine","audio/effects/bomber_engine.wav",7818,5911},
    {"exp","audio/effects/explosion.wav",4998,17734},
    {"feet","audio/effects/defector_steps.wav",5060,5067},
    {"iwon","audio/effects/ai_victory.wav",5914,9586},
    {"missile","audio/effects/missile_launch.wav",6468,4961},
    {"moo","audio/effects/cow.wav",11862,4961},
    {"ooo","audio/effects/warhead_exclamation.wav",12904,5358},
    {"prop","audio/effects/propaganda.wav",5400,3550},
    {"selection_bleep","audio/effects/selection_bleep.wav",506,5067},
    {"ticker","audio/effects/typewriter.wav",128,27928},
}};
}
