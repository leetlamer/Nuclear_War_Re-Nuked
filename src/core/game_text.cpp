#include "core/game_text.hpp"
#include "core/game_text_data.hpp"
#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <vector>

namespace nuclear::text {
Entry lookup(std::uint32_t address) {
    const auto it=std::lower_bound(generated::rows.begin(),generated::rows.end(),address,
        [](const auto& row,auto value) { return row.address<value; });
    if(it==generated::rows.end() || it->address!=address) throw std::out_of_range("Original text address not catalogued");
    return {it->address,it->text};
}
Entry table(std::string_view name,unsigned index) {
    for(const auto& p:generated::pointers) if(p.table==name && p.index==index) {
        if(p.address==0) throw std::out_of_range("Original text table contains a null pointer");
        return lookup(p.address);
    }
    throw std::out_of_range("Original text table/index not catalogued");
}
namespace {
unsigned decimal(std::string_view text,std::size_t& i) {
    unsigned value=0;
    while(i<text.size() && text[i]>='0' && text[i]<='9') {
        value=value*10+unsigned(text[i++]-'0');
        if(value>4096) throw std::invalid_argument("Original text field width/precision too large");
    }
    return value;
}
}
std::string format(std::uint32_t address,std::initializer_list<Argument> arguments) {
    return format(address,std::vector<Argument>(arguments));
}
std::string format(std::uint32_t address,const std::vector<Argument>& args) {
    const auto text=lookup(address).text;
    std::size_t used=0;
    std::string output;
    for(std::size_t i=0;i<text.size();) {
        if(text[i]!='%') { output+=text[i++]; continue; }
        ++i;
        if(i<text.size() && text[i]=='%') { output+='%'; ++i; continue; }
        bool left=false,zero=false,plus=false,space=false;
        while(i<text.size()) {
            if(text[i]=='-') left=true; else if(text[i]=='0') zero=true;
            else if(text[i]=='+') plus=true; else if(text[i]==' ') space=true; else break;
            ++i;
        }
        const unsigned width=decimal(text,i); bool precision_set=false; unsigned precision=0;
        if(i<text.size() && text[i]=='.') { precision_set=true; ++i; precision=decimal(text,i); }
        if(i<text.size() && text[i]=='l') ++i; // 68000 long remains explicitly32bits
        if(i==text.size() || used==args.size()) throw std::invalid_argument("Missing original text argument or conversion");
        const char conversion=text[i++]; const auto& arg=args[used++]; std::string value;
        if(conversion=='s') {
            const auto p=std::get_if<std::string>(&arg); if(!p) throw std::invalid_argument("Original %s requires string");
            value=p->substr(0,p->find('\0'));
            if(precision_set && value.size()>precision) value.resize(precision);
        } else if(conversion=='c') {
            const auto p=std::get_if<char>(&arg); if(!p) throw std::invalid_argument("Original %c requires char");
            value.assign(1,*p);
        } else if(conversion=='d' || conversion=='i' || conversion=='u') {
            std::string sign;
            if(conversion=='u') {
                const auto p=std::get_if<std::uint32_t>(&arg); if(!p) throw std::invalid_argument("Original %u requires uint32");
                value=std::to_string(*p);
            } else {
                const auto p=std::get_if<std::int32_t>(&arg); if(!p) throw std::invalid_argument("Original %d requires int32");
                const std::int64_t n=*p;
                if(n<0) sign="-"; else if(plus) sign="+"; else if(space) sign=" ";
                value=std::to_string(n<0?-n:n);
            }
            if(precision_set && precision==0 && value=="0") value.clear();
            if(precision_set && value.size()<precision) value.insert(0,precision-value.size(),'0');
            if(zero && !precision_set && !left && value.size()+sign.size()<width)
                value.insert(0,width-value.size()-sign.size(),'0');
            value=sign+value;
        } else throw std::invalid_argument("Unsupported original text conversion; no approximate formatting");
        if(value.size()<width) {
            if(left) value.append(width-value.size(),' '); else value.insert(0,width-value.size(),' ');
        }
        output+=value;
    }
    if(used!=args.size()) throw std::invalid_argument("Extra original text arguments");
    return output;
}
std::string attack_announcement(std::uint16_t action,unsigned variant,
                                std::string actor,std::string target) {
    static constexpr std::uint32_t missile[]{0x117bc4,0x117be1,0x117c03};
    static constexpr std::uint32_t bomber[]{0x11698c,0x1169a5,0x1169bf,0x1169d8};
    std::uint32_t address;
    if(action==1 && variant>=1 && variant<=3) address=missile[variant-1];
    else if(action==5 && variant>=1 && variant<=4) address=bomber[variant-1];
    else throw std::out_of_range("Original attack announcement variant unavailable");
    return format(address,{std::move(actor),std::move(target)});
}
std::string action_announcement(std::uint16_t action,unsigned variant,
                                std::string actor,std::string target) {
    if(action==1 || action==5) return attack_announcement(action,variant,std::move(actor),std::move(target));
    std::vector<std::uint32_t> pool;
    bool two_names=false;
    switch(action) {
    case 2: pool={0x112bc2,0x112bdd,0x112bff,0x112c24}; break;
    case 3: pool={0x116f12,0x116f35,0x116f56,0x116f77}; two_names=true; break;
    case 4: pool={0x116c22,0x116c37,0x116c57,0x116c6d}; break;
    case 6: pool={0x1174ea,0x1174ff,0x117514,0x11753b,0x117553}; break;
    case 7: pool={0x112e80,0x112e9d,0x112ebd,0x112ed5}; break;
    case 8: // no message RNG draw; sole literal has two runtime names
        return format(0x110f48,{std::move(actor),std::move(target)});
    case 9: pool={0x111ef0,0x111f11,0x111f33,0x111f47}; break;
    case 11: pool={0x1123e6,0x11240a,0x112428,0x112444}; break;
    case 12: pool={0x117ed6,0x117ef9,0x117f24}; break;
    case 13: pool={0x111936,0x111956,0x111976,0x111995}; two_names=true; break;
    case 14: pool={0x1113da,0x111402,0x111424,0x111441}; break;
    case 15: // verified handler rolls only1..3; two additional source lines
             // are catalogued but not silently made reachable here.
        pool={0x1171fc,0x117220,0x117243}; break;
    case 16: return format(0x11817a,{std::move(actor)});
    case 86: pool={0x112878,0x112892,0x1128b0,0x1128ca}; break;
    default:throw std::out_of_range("Original action announcement not mapped");
    }
    if(variant<1 || variant>pool.size()) throw std::out_of_range("Original action variant not mapped");
    if(two_names) return format(pool[variant-1],{std::move(actor),std::move(target)});
    return format(pool[variant-1],{std::move(actor)});
}

}
