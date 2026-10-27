#pragma once
#include "rendering/animations/special_events.hpp"
namespace nuclear::player {
inline Rect selection_background(Word action){return action==2?Rect{192,111,124,80}:Rect{192,31,124,80};}
// Original full-memory atlas. Host durations approximate native callback order.
inline std::vector<DrawSprite> selection_sprites(Word action,double seconds,double dish_frame_seconds=.12){
 std::vector<DrawSprite> out;if(seconds<0||!std::isfinite(seconds))return out;
 if(action==2){constexpr int pads[]{120,124,128,140,144,148},smoke[]{42,44,52,54};int pad=pads[std::min(5,int(seconds/.2))];out.push_back({Atlas::large,{pad%20*16,pad/20*16,64,16},{110,116,64,16}});if(seconds>=1.2){int frame=smoke[int(std::fmod((seconds-1.2)*8,4))];out.push_back({Atlas::large,{frame%10*32,frame/10*32,32,32},{136,99,32,32}});}}
 if(action==4&&seconds<3){double x=160,y=139;if(seconds<1.2){double t=seconds/1.2;x-=68*t;y-=68*t;}else if(seconds>=1.8)x=92+std::pow((seconds-1.8)/1.2,2)*132;else {x=92;y=71;}if(seconds>=1.2)y=71;int frame=seconds<1.2?0:std::min(6,1+int((seconds-1.2)*5));out.push_back({Atlas::large,{frame*32,0,32,32},{int(x),int(y),32,32}});}
 if(action==7){constexpr int x[]{128,192,240,0,48,96,144},y[]{288,288,288,368,368,368,368},width[]{64,48,48,48,48,48,64},dest_x[]{121,132,136,141,145,146,147};int step=int(seconds/dish_frame_seconds)%12;int frame=step<=6?step:12-step;int h=seconds<dish_frame_seconds?64:61;out.push_back({Atlas::large,{x[frame],y[frame],width[frame],h},{dest_x[frame],73,width[frame],h}});}

 return out;
}
inline std::string action_line(std::string text){for(auto& c:text)if(c=='\n'||c=='\r')c=' ';return text;}
inline double action_glyph_scale(std::size_t characters){return characters?std::min(1.,316./(characters*6.)):1.;}
}
