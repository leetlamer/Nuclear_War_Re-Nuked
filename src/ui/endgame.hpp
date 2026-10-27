#pragma once
#include "ui/player_controller.hpp"
#include "nuclear/ending.hpp"
#include <fstream>
#include <filesystem>
namespace nuclear::player {
// Native11A33A/23933A precision limits emitted characters, including integers;
// unlike libc printf it does NOT zero-pad .3d. Isolated Hall formatting only.
inline std::string hall_field(std::string text,std::size_t width,std::size_t precision,bool left=false){text.resize(std::min(text.size(),precision));if(text.size()<width){std::string pad(width-text.size(),' ');text=left?text+pad:pad+text;}return text;}
inline std::string hall_numbers(HighScoreNumbers n){return hall_field(std::to_string(signed_word(n.population)),3,3)+"    "+hall_field(std::to_string(signed_word(n.cities)),1,1)+"   "+hall_field(std::to_string(signed_word(n.score)),3,3)+" Million";}
struct HallRow {std::string name;HighScoreNumbers numbers;};
class EndgamePresentation {
 bool active_{},editing_{},saved_{},audio_wait_{};double elapsed_{},jump_seconds_{1.2};std::optional<std::size_t> rank_;EndScore score_{};int winner_{-1};std::array<HallRow,10> rows_{};std::string path_,error_;
public:
 explicit EndgamePresentation(std::string path={}):path_(std::move(path)){load();}
 bool restart_ready()const{return active_&&hall()&&!editing_&&(!rank_||saved_);}
 bool active()const{return active_;}bool editing()const{return editing_;}bool hall()const{return elapsed_>=.8+(winner_>=0&&winner_<4?3*jump_seconds_:0)&&!audio_wait_;}
 bool ai_victory()const{return active_&&winner_>=0&&winner_<4&&!hall();}double elapsed()const{return elapsed_;}double jump_seconds()const{return jump_seconds_;}void jump_seconds(double s){jump_seconds_=s;}void audio_wait(bool value){audio_wait_=value;}int winner()const{return winner_;}const auto& rows()const{return rows_;}auto rank()const{return rank_;}EndScore score()const{return score_;}bool saved()const{return saved_;}const std::string& error()const{return error_;}
 static std::optional<std::array<HallRow,10>> parse(const std::vector<unsigned char>& bytes){if(bytes.size()!=170)return std::nullopt;std::array<HallRow,10> rows;for(int i=0;i<10;++i){auto p=bytes.data()+17*i;int n=0;while(n<11&&p[n])++n;if(n>10)return std::nullopt;rows[i].name=std::string(reinterpret_cast<const char*>(p),std::size_t(n));rows[i].numbers={Word(p[11]*256+p[12]),Word(p[13]*256+p[14]),Word(p[15]*256+p[16])};}return rows;}
 void load(){std::vector<unsigned char> bytes(player_assets::highscores.begin(),player_assets::highscores.end());if(!path_.empty()){std::ifstream f(path_,std::ios::binary);if(f){std::vector<unsigned char> candidate{std::istreambuf_iterator<char>(f),{}};if(parse(candidate))bytes=std::move(candidate);else error_="Invalid saved score file; original table used.";}}rows_=*parse(bytes);}
 bool begin(const Player& player,bool pending){if(active_||pending||!player.ended()||!end_condition(player.coordinator()->data().state))return false;active_=true;winner_=last_surviving_actor(player.coordinator()->data().state);if(winner_<0)return true;score_=end_game_score(player.coordinator()->data().state.actors[std::size_t(winner_)]);HighScoreTable table;for(int i=0;i<10;++i)table[i]=rows_[i].numbers;rank_=insert_high_score_numbers(table,{score_.score,score_.population_total,score_.positive_cities});if(rank_){for(std::size_t i=9;i>*rank_;--i)rows_[i]=rows_[i-1];std::string name=player.coordinator()->data().names[std::size_t(winner_)];name=name.substr(0,name.find(' '));rows_[*rank_]={winner_==4?"":name.substr(0,10),table[*rank_]};editing_=winner_==4;if(!editing_)save();}return true;}
 void tick(double dt){if(active_)elapsed_+=std::max(0.,dt);}
 void input(const std::string& input){if(!editing_||!hall())return;for(unsigned char c:input)if(c>=32&&c<127&&rows_[*rank_].name.size()<10)rows_[*rank_].name+=char(c);}
 void backspace(){if(editing_&&!rows_[*rank_].name.empty())rows_[*rank_].name.pop_back();}
 void finish_name(){if(editing_&&hall()){editing_=false;save();}}
 void save(){if(saved_||!rank_||editing_)return;if(path_.empty()){saved_=true;return;}std::vector<unsigned char> bytes(170);for(std::size_t i=0;i<10;++i){auto& row=rows_[i];for(int j=0;j<10;++j)bytes[i*17+std::size_t(j)]=32;for(std::size_t j=0;j<std::min<std::size_t>(10,row.name.size());++j)bytes[i*17+j]=static_cast<unsigned char>(row.name[j]);Word values[]{row.numbers.score,row.numbers.population,row.numbers.cities};for(int k=0;k<3;++k){bytes[i*17+11+2*k]=static_cast<unsigned char>(values[k]>>8);bytes[i*17+12+2*k]=static_cast<unsigned char>(values[k]);}}try{auto parent=std::filesystem::path(path_).parent_path();if(!parent.empty())std::filesystem::create_directories(parent);std::ofstream f(path_+".tmp",std::ios::binary);f.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());f.close();if(!f)throw std::runtime_error("score write failed");std::filesystem::rename(path_+".tmp",path_);saved_=true;}catch(const std::exception& ex){error_=ex.what();}}
};
}
