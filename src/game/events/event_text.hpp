#pragma once
#include "rendering/action_media.hpp"
namespace nuclear::player {
inline bool audited_special_event(Word action){return action==8||action==9||action==11||action==12||action==13||action==14||action==15||action==16||action==86;}
// Variants come from recorded handler draws. Invalid/missing data is not repaired
// with cosmetic RNG or an invented default variant.
inline std::optional<std::string> special_event_announcement(const MediaEvidence& e,
 const std::string& actor,const std::string& target){
 if(!e.supported||!audited_special_event(e.action))return std::nullopt;
 try{return nuclear::text::action_announcement(e.action,e.variant,actor,target);}
 catch(const std::out_of_range&){return std::nullopt;}
}
// The amount is the recorded result Context::magnitude, not a re-derived
// population delta. Native message arguments sign-extend the original word.
inline std::optional<std::string> special_event_result(const MediaEvidence& e,
 Word result_variant,Word amount,const std::string& actor,const std::string& target){
 if(!e.supported||!e.amount_known||!audited_special_event(e.action))return std::nullopt;
 std::uint32_t address=0;bool named=false;std::string name;
 switch(e.action){
 case 8:address=0x1053c3;named=true;name=target;break;
 case 9:address=0x1053d8;named=true;name=actor;break;
 case 11:{constexpr std::uint32_t pool[]{0x1053ed,0x105416,0x105435};if(result_variant<1||result_variant>3)return std::nullopt;address=pool[result_variant-1];break;}
 case 12:{constexpr std::uint32_t pool[]{0x1054e4,0x105512,0x105540};if(result_variant<1||result_variant>3)return std::nullopt;address=pool[result_variant-1];break;}
 case 13:{constexpr std::uint32_t pool[]{0x10556b,0x105594,0x1055b2,0x1055d8,0x105604};if(result_variant<1||result_variant>5)return std::nullopt;address=pool[result_variant-1];break;}
 case 14:address=0x105630;named=true;name=target;break;
 case 15:address=0x105653;break;
 case 16:address=0x105673;break;
 case 86:{constexpr std::uint32_t pool[]{0x105457,0x105485,0x1054b9};if(result_variant<1||result_variant>3)return std::nullopt;address=pool[result_variant-1];break;}
 default:return std::nullopt;
 }
 auto count=nuclear::text::signed16(amount);
 return named?nuclear::text::format(address,{name,count}):nuclear::text::format(address,{count});
}
}
