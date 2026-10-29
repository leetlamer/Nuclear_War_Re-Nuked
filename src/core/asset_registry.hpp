#pragma once
#include <array>
#include <string_view>
namespace nuclear::assets {
struct ImageSpec{std::string_view key,mask,file;unsigned width,height;};
inline constexpr std::array<ImageSpec,16> images{{
    {"character_selection","","images/backgrounds/character_selection.png",320,200},
    {"main_window","","images/interface/main_window.png",320,200},
    {"leader_portraits","","images/characters/leader_portraits.png",320,160},
    {"world_map","","images/backgrounds/world_map.png",320,200},
    {"sprite_tiles","","images/interface/control_tiles.png",320,272},
    {"sprite_tiles","masked","images/effects/small_sprites.png",320,272},
    {"flight_effects","masked","images/missiles/flight_and_effects.png",320,288},
    {"large_sprites","masked","images/effects/large_sprites.png",320,432},
    {"large_sprites","","images/interface/selection_backdrops.png",320,432},
    {"victory_body","masked","images/characters/victory_body.png",320,112},
    {"space_ending","","images/backgrounds/space_ending.png",320,200},
    {"desolate_ending","","images/backgrounds/desolate_ending.png",320,200},
    {"bitmap_font","","fonts/original_bitmap.png",576,8},
    {"expmap","","images/effects/expmap.png",320,200},
    {"smallexp","","images/effects/smallexp.png",320,176},
    {"bigexp","","images/effects/bigexp.png",320,432},
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
