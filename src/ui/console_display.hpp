#pragma once
#include <algorithm>
#include <array>
#include <cstdint>
#include <cmath>
#include <string>
#include <vector>
namespace nuclear::player {
// Display wrapping only: source strings are retained by the event.
inline std::vector<std::string> wrap_text(std::string text,std::size_t width){
 if(width==0)width=1;std::vector<std::string> lines;
 do{auto newline=text.find('\n');std::string row=text.substr(0,newline);
  do{if(row.size()<=width){lines.push_back(row);break;}auto cut=row.rfind(' ',width);if(cut==0||cut==std::string::npos)cut=width;lines.push_back(row.substr(0,cut));row.erase(0,cut+(cut<row.size()&&row[cut]==' '));}while(true);
  if(newline==std::string::npos)break;text.erase(0,newline+1);
 }while(true);return lines;
}
class TextFlow {
 std::vector<std::string> pages_{""};std::size_t page_{};double seconds_{};bool forced_{};double cps_{35};
public:
 void speed(double cps){if(std::isfinite(cps)&&cps>0&&cps<=1000)cps_=cps;}
 void set(const std::string& text,std::size_t width=52,std::size_t rows=2){pages_.clear();auto lines=wrap_text(text,width);rows=std::max(std::size_t(1),rows);for(std::size_t i=0;i<lines.size();i+=rows){std::string page;for(std::size_t j=i;j<std::min(i+rows,lines.size());++j){if(j!=i)page+='\n';page+=lines[j];}pages_.push_back(page);}if(pages_.empty())pages_.push_back("");page_=0;seconds_=0;forced_=false;}
 void tick(double seconds){if(std::isfinite(seconds)&&seconds>0)seconds_=std::min(3600.0,seconds_+seconds);}
 std::size_t page()const{return page_;}
 bool complete()const{return forced_||seconds_*cps_>=pages_[page_].size();}
 void finish(){forced_=true;}
 std::string visible()const{return pages_[page_].substr(0,complete()?pages_[page_].size():std::size_t(seconds_*cps_));}
 // false means all text is complete; caller may advance to the next message/event.
 bool advance(){if(!complete()){finish();return true;}if(page_+1<pages_.size()){++page_;seconds_=0;forced_=false;return true;}return false;}
};
// One continuously typed stream; only the viewport scrolls. No page/click gates.
class ScrollingTextFlow {
 std::string content_;double elapsed_{},cps_{35};std::size_t rows_{3};
public:
 void set(const std::string& text,std::size_t width=27,std::size_t rows=3){content_.clear();for(const auto& line:wrap_text(text,width)){if(!content_.empty())content_+='\n';content_+=line;}rows_=std::max<std::size_t>(1,rows);elapsed_=0;}
 void speed(double cps){if(std::isfinite(cps)&&cps>0&&cps<=1000)cps_=cps;}
 void tick(double dt){if(std::isfinite(dt)&&dt>0)elapsed_=std::min(3600.,elapsed_+dt);}
 bool complete()const{return elapsed_*cps_>=content_.size();}
 double completion_time()const{return content_.size()/cps_;}
 std::string revealed()const{return content_.substr(0,std::min(content_.size(),std::size_t(elapsed_*cps_)));}
 std::string visible()const{auto prefix=revealed();std::size_t start=0;auto lines=std::count(prefix.begin(),prefix.end(),'\n')+1;while(lines>int(rows_)){start=prefix.find('\n',start)+1;--lines;}return prefix.substr(start);}

};

// Lifetime is the session, not a modal message. Two producer cursors retain
// reveal offsets when switching between main text and production presentation.
class ConsoleTranscript {
 struct Cursor{std::uint64_t generation{};std::size_t seen{};bool valid{};};
 std::array<Cursor,2> cursors_{};std::string history_;std::size_t writer_{};std::uint64_t generation_{};bool have_writer_{};
 void trim(){while(std::count(history_.begin(),history_.end(),'\n')>=64){auto n=history_.find('\n');history_.erase(0,n+1);}if(history_.size()>4096)history_.erase(0,history_.size()-4096);}
public:
 bool observe(std::size_t channel,std::uint64_t generation,const std::string& prefix){if(channel>=cursors_.size())return false;auto& cursor=cursors_[channel];if(!cursor.valid||cursor.generation!=generation){cursor={generation,0,true};}if(prefix.size()<cursor.seen)return false;auto suffix=prefix.substr(cursor.seen);cursor.seen=prefix.size();if(suffix.empty())return false;if((!have_writer_||writer_!=channel||generation_!=generation)&&!history_.empty()&&history_.back()!='\n'&&suffix.front()!='\n')history_+='\n';history_+=suffix;writer_=channel;generation_=generation;have_writer_=true;trim();return true;}
 const std::string& history()const{return history_;}
 std::string visible()const{std::size_t start=0;auto lines=std::count(history_.begin(),history_.end(),'\n')+1;while(lines>3){start=history_.find('\n',start)+1;--lines;}return history_.substr(start);}
};

}
