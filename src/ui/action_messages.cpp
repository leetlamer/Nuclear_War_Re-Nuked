#include "nuclear/engine.hpp"
namespace nuclear {
HumanAnimationEndpoint human_animation_endpoint(Word index, Word endpoint,
    Word direction, Word timer, Rng& rng) noexcept {
    HumanAnimationEndpoint result{direction,timer,0};
    if(index!=endpoint) return result;
    if(direction==1) result.direction=0xffff;
    else if(direction==0xffff) result.direction=1;
    const Word first=Word(rng.next()%150);
    result.timer=Word(first+rng.next()%150+2);
    result.draws=2;
    return result;
}
HumanAnimationTail advance_human_animation_tail(
    const std::array<HumanAnimationSlot,4>& slots, Rng& rng) noexcept {
    HumanAnimationTail report; report.slots=slots;
    for(std::size_t i=0;i<slots.size();++i) {
        auto& s=report.slots[i];
        if(s.index==0xffff) continue;
        report.render[i]=true; report.render_indices[i]=s.index;
        if(s.timer!=0) { s.timer=sub_word(s.timer,1); continue; }
        s.index=add_word(s.index,s.direction);
        s.timer=12;
        const auto endpoint=human_animation_endpoint(s.index,s.endpoint_a,s.direction,s.timer,rng);
        s.direction=endpoint.direction; s.timer=endpoint.timer;
        report.draws+=endpoint.draws;
        if(s.index==s.endpoint_b) {
            if(s.direction==1) s.direction=0xffff;
            else if(s.direction==0xffff) s.direction=1;
            s.timer=12;
        }
    }
    return report;
}
PriorHumanMessages prior_human_messages(const State& state, Word selected_class, const std::array<std::string,5>& names, MessageSequenceRng& context, Rng& rng) {
    const auto& human=state.actors[4];
    PriorHumanMessages report;
    const auto magnitude=std::to_string(signed_word(human.magnitude));
    const auto capacity=std::to_string(signed_word(state.human_prepared_capacity));
    const auto label=[&]() -> std::string {
        static const std::array<std::string,4> labels{"Carnage","Gravedigger","Deathbringer","Pacifier"};
        if(selected_class>=4) throw Unsupported("prior message class outside0..3");
        return labels[selected_class];
    };
    const auto target=[&]() -> std::string {
        if(human.target_actor>=5) throw Unsupported("prior message target outside0..4");
        const auto& name=names[human.target_actor];
        return name.substr(0,name.find(char(0)));
    };
    if(human.action==1) { // 0x106c14
        report.messages.push_back("The " + label() + " missile"); // 0x108f1a
        report.messages.push_back("exploded on " + target() + ""); // 0x108f29
        if(human.magnitude!=0) report.messages.push_back("" + magnitude + " million dead."); // 0x108f38
        if(human.magnitude==0) report.messages.push_back("No casualties."); // 0x108f49
    }
    if(human.action==22) { // 0x106cba
        report.messages.push_back("" + magnitude + " million Alien"); // 0x108f58
        report.messages.push_back("Immigrants Landed"); // 0x108f69
        report.messages.push_back("In your Fine Nation."); // 0x108f7b
    }
    if(human.action==21) { // 0x106cfc
        report.messages.push_back("" + magnitude + " million killed by"); // 0x108f90
        report.messages.push_back("radiation from a"); // 0x108fa5
        report.messages.push_back("nuclear meltdown."); // 0x108fb6
    }
    if(human.action==16) { // 0x106d3e
        report.messages.push_back("" + magnitude + " million gained"); // 0x108fc8
        report.messages.push_back("when Mr. Stork flew"); // 0x108fda
        report.messages.push_back("over your city."); // 0x108fee
    }
    if(human.action==86) { // 0x106d80
        report.messages.push_back("" + magnitude + " million killed by"); // 0x108ffe
        report.messages.push_back("a savage earthquake"); // 0x109013
        report.messages.push_back("striking your nation"); // 0x109027
    }
    if(human.action==7) { // 0x106dc2
        report.messages.push_back("You deployed a defense"); // 0x10903c
        report.messages.push_back("system to protect"); // 0x109053
        report.messages.push_back("your population."); // 0x109065
    }
    if(human.action==88) { // 0x106dee
        report.messages.push_back("Your bomber crashed"); // 0x109076
        report.messages.push_back("enroute to its target."); // 0x10908a
        report.messages.push_back(""); // 0x1090a1
    }
    if(human.action==20) { // 0x106e1a
        report.messages.push_back("ejected " + magnitude + " million"); // 0x1090a2
        report.messages.push_back("libelous liberals."); // 0x1090b5
    }
    if(human.action==20) { // 0x106e50
        report.messages.push_back("Your Human Catapult"); // 0x1090c8
        report.messages.push_back("ejected " + magnitude + " million"); // 0x1090dc
        report.messages.push_back("libelous liberals."); // 0x1090ef
    }
    if(human.action==19) { // 0x106e92
        report.messages.push_back("The flying cows of"); // 0x109102
        report.messages.push_back("CattleTech killed"); // 0x109115
        if(human.magnitude!=0) report.messages.push_back("" + magnitude + " million carnivores."); // 0x109127
        if(human.magnitude==0) report.messages.push_back("absolutely no one."); // 0x10913e
    }
    if(human.action==18) { // 0x106eec
        report.messages.push_back("A 16 ton weight from"); // 0x109151
        report.messages.push_back("Hell splattered"); // 0x109166
        report.messages.push_back("" + magnitude + " million people."); // 0x109176
    }
    if(human.action==17) { // 0x106f2e
        report.messages.push_back("" + magnitude + " million space cadets"); // 0x109189
        report.messages.push_back("joined the L-5 space"); // 0x1091a1
        report.messages.push_back("colony."); // 0x1091b6
    }
    if(human.action==8) { // 0x106f70
        report.messages.push_back("Your propaganda strike"); // 0x1091be
        report.messages.push_back("failed " + magnitude + " million"); // 0x1091d5
        report.messages.push_back("defected to " + target() + "."); // 0x1091e7
    }
    if(human.action==3) { // 0x106fd4
        report.messages.push_back("After a propaganda"); // 0x1091f7
        if(human.magnitude!=0) report.messages.push_back("strike, " + magnitude + " million left"); // 0x10920a
        if(human.magnitude==0) report.messages.push_back("strike, No one left"); // 0x109222
        report.messages.push_back("" + target() + " for you."); // 0x109236
    }
    if(human.action==5) { // 0x107050
        report.messages.push_back("You bombed " + target() + ""); // 0x109242
        if(human.magnitude!=0) report.messages.push_back("for " + magnitude + " million:"); // 0x109250
        if(human.magnitude==0) report.messages.push_back("for no casualties."); // 0x109260
        report.messages.push_back("" + capacity + " megatons remain."); // 0x109273
    }
    if(human.action==2) { // 0x1070ea
        report.messages.push_back("" + label() + " missile"); // 0x109287
        report.messages.push_back("" + capacity + " Megatons capacity"); // 0x109292
        report.messages.push_back("awaits orders."); // 0x1092a7
    }
    if(human.action==4) { // 0x10714e
        report.messages.push_back("Bomber standing by,"); // 0x1092b6
        report.messages.push_back("" + capacity + " megatons remain."); // 0x1092ca
    }
    auto cursor=context; auto random=rng;
    for(const auto& message:report.messages)
        report.draws+=advance_message_sequence(cursor,message,random).draws;
    context=cursor; rng=random; return report;
}
HumanEntryTail prepare_human_entry_tail(State& state, Word& welcome_flag,
    MessageSequenceRng& context, Rng& rng) {
    auto copy=state; auto random=rng; auto cursor=context; auto flag=welcome_flag;
    HumanEntryTail report; auto& human=copy.actors[4];
    report.refresh=attitude_refresh_rng(copy,random); report.draws=report.refresh.draws;
    if(human.action==1 || human.action==5 || human.action==8 || human.action==3) {
        if(human.target_actor>=5) throw Unsupported("human speech target outside0..4");
        if(populated_cities(copy.actors[human.target_actor])!=0) {
            report.speech_roll=Word(random.next()%69+1); ++report.draws;
        }
    }
    human.target_actor=0xffff;
    if(flag!=0) {
        flag=0;
        static const std::array<std::string,48> welcome{
            "Good Luck!",
            "Best Wishes!",
            "Let the game begin!",
            "The end is near.",
            "The coin is tossed.",
            "The die is cast.",
            "Survive, if you can.",
            "Diplomacy Terminal #1.",
            "Your opponents await.",
            "Let the contest begin.",
            "Dawn is in 32 minutes.",
            "Everyone is here.",
            "All eyes are upon you.",
            "Do you have insurance?",
            "Your destiny awaits.",
            "Thermo Nuclear war?",
            "Alert stats: RED.",
            "All wings standing by.",
            "All systems are go.",
            "Are you ready?",
            "Can you take the heat.",
            "Got what it takes?",
            "Think you can surive?",
            "Go for the Hill!",
            "You have 1 credit.",
            "You don't have a chance.",
            "Can you get Highscore?",
            "Millions count on you.",
            "No turning back now.",
            "Think you can win?",
            "Can you save Cron?",
            "Good Morning.",
            "Good Afternoon.",
            "Good Evening.",
            "Good Day.",
            "Good Bye.",
            "I am fully functional.",
            "Terminal mode enabled.",
            "Terminal active.",
            "Everything is lit.",
            "Power up complete.",
            "Status: Green.",
            "This space for lease.",
            "Sequence initiated.",
            "Welcome aboard, Captain.",
            "All stations ready.",
            "You have the Con.",
            "Nuke em till they glow."};
        report.messages.push_back("Welcome to Nuclear War.");
        report.draws+=advance_message_sequence(cursor,report.messages.back(),random).draws;
        report.welcome_index=Word(random.next()%48); ++report.draws;
        report.messages.push_back(welcome[report.welcome_index]);
        report.draws+=advance_message_sequence(cursor,report.messages.back(),random).draws;
        report.messages.push_back("");
        report.draws+=advance_message_sequence(cursor,"",random).draws;
    }
    state=copy; rng=random; context=cursor; welcome_flag=flag; return report;
}
HumanEntry prepare_human_entry(State& state, Word previous_selected_class,
    const std::array<std::string,5>& names, Word& welcome_flag,
    MessageSequenceRng& context, Rng& rng) {
    auto copy=state; auto flag=welcome_flag; auto cursor=context; auto random=rng;
    HumanEntry report;
    report.prefix=prepare_human_entry_prefix(copy,cursor,random);
    report.prior=prior_human_messages(copy,previous_selected_class,names,cursor,random);
    report.tail=prepare_human_entry_tail(copy,flag,cursor,random);
    report.draws=report.prefix.total_draws+report.prior.draws+report.tail.draws;
    state=copy; welcome_flag=flag; context=cursor; rng=random; return report;
}
HumanConfirmation prepare_human_confirmation(Word pending, Word target,
    MessageSequenceRng& context, Rng& rng) {
    HumanConfirmation report;
    report.ready=human_confirmation_ready(pending,target);
    if(report.ready) return report;
    if(pending==0) report.messages={"Select an action","before ending turn.",""};
    else report.messages={"Choose a target city","Before ending turn.",""};
    auto cursor=context; auto random=rng;
    for(const auto& text:report.messages)
        report.draws+=advance_message_sequence(cursor,text,random).draws;
    context=cursor; rng=random; return report;
}
HumanPropagandaCommand prepare_human_propaganda_command(State& state,
    HumanSelection& selection, Word target, const std::array<std::string,5>& names,
    MessageSequenceRng& context, Rng& rng) {
    auto copy=state; auto fields=selection; auto cursor=context; auto random=rng;
    HumanPropagandaCommand report;
    report.target_selected=begin_human_propaganda_fields(fields,target);
    if(!report.target_selected) {
        report.messages={"Select a target city","for your propaganda.",""};
    } else {
        if(target>=5) throw Unsupported("propaganda message target outside0..4");
        auto name=names[target].substr(0,names[target].find(char(0)));
        report.messages={name+" targeted","for propoganada."};
        const Word old_action=copy.actors[4].action;
        report.messages.push_back(old_action==4?"This will junk a bomber":
                                  old_action==2?"This will junk a missile":"");
    }
    for(const auto& text:report.messages)
        report.draws+=advance_message_sequence(cursor,text,random).draws;
    if(report.target_selected) {
        report.discarded_roll=roll_human_propaganda_fields(copy,fields,random);
        report.draws+=3;
    }
    state=copy; selection=fields; context=cursor; rng=random; return report;
}
PriorHumanMessages prepare_human_production_command(Word previous_action,
    HumanSelection& selection, MessageSequenceRng& context, Rng& rng) {
    auto fields=selection; auto cursor=context; auto random=rng;
    PriorHumanMessages report;
    //1079d2 formats an unused buffer;1079e2 displays an empty literal.
    report.messages={"","","Weapons will be built.","Click on Nuclear war."};
    if(previous_action==4) report.messages.push_back("This will junk a bomber");
    if(previous_action==2) report.messages.push_back("This will junk a missile");
    report.messages.push_back(previous_action==6?"NOTE: You're stockpiling":"");
    for(std::size_t i=0;i<report.messages.size();++i) {
        report.draws+=advance_message_sequence(cursor,report.messages[i],random).draws;
        if(i==0) select_human_production_fields(fields);
    }
    selection=fields; context=cursor; rng=random; return report;
}
HumanConfirmation confirm_human_command(State& state, const HumanSelection& selection,
    Word target, MessageSequenceRng& context, Rng& rng) {
    auto copy=state; auto cursor=context; auto random=rng;
    auto report=prepare_human_confirmation(selection.pending_action,target,cursor,random);
    if(report.ready)
        human_commit(copy,selection.pending_action,selection.selected_class,selection.selected_capacity);
    state=copy; context=cursor; rng=random; return report;
}
HumanWarheadCommand prepare_human_warhead_command(State& state, HumanSelection& selection,
    Word choice, Word target, const std::array<std::string,5>& names,
    MessageSequenceRng& context, Rng& rng) {
    auto copy=state; auto fields=selection; auto cursor=context; auto random=rng;
    HumanWarheadCommand report;
    report.check=select_human_warhead_fields(copy,fields,choice,target);
    switch(report.check) {
    case HumanWarheadCheck::no_warhead:
        report.messages=choice==0?std::vector<std::string>{"You have no warheads.","Try to build some.",""}:
            std::vector<std::string>{"You have no warheads","of that size.",""}; break;
    case HumanWarheadCheck::no_capacity:
        report.messages={"Your bomber has","used all its bombs,","deploy a new weapon."}; break;
    case HumanWarheadCheck::too_large:
        report.messages={"Warhead is too large.",copy.actors[4].action==2?"The missile can only":"The bomber can only",
            std::string(copy.actors[4].action==2?"carry ":"drop ")+std::to_string(signed_word(copy.human_prepared_capacity))+" megatons."}; break;
    case HumanWarheadCheck::no_prepared_delivery:
        report.messages={"You must first deploy","A delivery system."}; break;
    case HumanWarheadCheck::no_target:
        report.messages={"Select a target first!"}; break;
    case HumanWarheadCheck::accepted: {
        if(target>=5) throw Unsupported("warhead message target outside0..4");
        report.discarded_roll=Word(random.next()%4); ++report.draws;
        report.refresh=attitude_refresh_rng(copy,random); report.draws+=report.refresh.draws;
        const auto name=names[target].substr(0,names[target].find(char(0)));
        fields.pending_action=copy.actors[4].action==2?1:5;
        report.messages={"",name+" targeted",copy.actors[4].action==2?"by your missile.":"by your bomber:",
            std::to_string(signed_word(fields.selected_capacity))+" megaton payload."};
        break;
    }
    }
    for(const auto& text:report.messages)
        report.draws+=advance_message_sequence(cursor,text,random).draws;
    if(report.check==HumanWarheadCheck::accepted) finish_human_attack_fields(copy,fields);
    state=copy; selection=fields; context=cursor; rng=random; return report;
}
HumanDeliveryCommand prepare_human_delivery_command(State& state, HumanSelection& selection,
    Word action, Word choice, MessageSequenceRng& context, Rng& rng) {
    auto copy=state; auto fields=selection; auto cursor=context; auto random=rng;
    HumanDeliveryCommand report;
    report.accepted=select_human_delivery_fields(copy,fields,action,choice);
    if(!report.accepted) {
        const auto& human=state.actors[4];
        if(action==2) {
            report.messages=selection.selected_class==0xffff?
                std::vector<std::string>{"Your arsenal contains","no missiles at this time.",""}:
                std::vector<std::string>{"You have no missiles","of that size.",""};
        } else if(action==4) {
            report.messages=(human.bombers[0]==0 && human.bombers[1]==0)?
                std::vector<std::string>{"Your arseneal contains","no bombers at this time.",""}:
                std::vector<std::string>{"Your have no bombers","of that class.",""};
        } else {
            report.messages=(human.defenses[0]==0 && human.defenses[1]==0)?
                std::vector<std::string>{"Your arsenal contains","no defensive weaponry","at this time."}:
                std::vector<std::string>{"Your stockpile contains","no defensive weaponry.","of that type."};
        }
    } else {
        static const std::array<std::string,4> missiles{"Carnage","Gravedigger","Deathbringer","Pacifier"};
        static const std::array<std::string,2> bombers{"Nuclear Postman","Grim Reaper"}, defenses{"L.N.D.S","MegaCannon"};
        if(action==7) report.messages={defenses[fields.selected_class]+" system","ready for action.",""};
        else {
            report.messages={(action==2?missiles[fields.selected_class]+" missile":bombers[fields.selected_class]+" bomber"),
                std::to_string(signed_word(fields.selected_capacity))+" megaton capacity."};
            const auto old=state.actors[4].action;
            report.messages.push_back(old==4?"This will junk a bomber":old==2?"This will junk a missile":"");
        }
        if(action!=4) {
            report.refresh=attitude_refresh_rng(copy,random); report.draws+=report.refresh.draws;
        }
    }
    for(const auto& text:report.messages)
        report.draws+=advance_message_sequence(cursor,text,random).draws;
    if(report.accepted && action==4) {
        report.refresh=attitude_refresh_rng(copy,random); report.draws+=report.refresh.draws;
    }
    state=copy; selection=fields; context=cursor; rng=random; return report;
}
HumanCityTarget prepare_human_city_target(State& state, HumanSelection& selection,
    Word view, Word display, Word city, const std::array<std::string,5>& names,
    MessageSequenceRng& context, Rng& rng) {
    auto copy=state; auto fields=selection; auto cursor=context; auto random=rng;
    HumanCityTarget report; report.target_marker=display;
    if(view==4) report.messages={"You may not target","your own country!",""};
    else {
        if(view>=4 || city>=5) throw Unsupported("city target outside verified actor/city slots");
        if(fields.pending_action==3) {
            const auto name=names[view].substr(0,names[view].find(char(0)));
            report.messages={"",name+" targeted","for propaganada."};
            const auto old=copy.actors[4].action;
            report.messages.push_back(old==4?"This will junk a bomber":old==2?"This will junk a missile":"");
        }
    }
    for(const auto& text:report.messages)
        report.draws+=advance_message_sequence(cursor,text,random).draws;
    if(view!=4) {
        if(fields.pending_action==3) {
            report.discarded_roll=roll_human_propaganda_fields(copy,fields,random); report.draws+=3;
        }
        report.accepted=select_human_target_fields(copy,view,city);
    }
    state=copy; selection=fields; context=cursor; rng=random; return report;
}
HumanViewChange change_human_actor_view(const State& state, Word logical,
    Word display, Word requested, Rng& rng) {
    if(logical>=5 || (display>=5 && display!=0xffff) || requested>=5)
        throw Unsupported("human view outside verified actor slots");
    HumanViewChange report; report.logical_view=logical; report.display_actor=display;
    const Word candidate=requested==display?4:requested;
    if(candidate==display) return report;
    auto random=rng; report.logical_view=candidate; report.display_actor=candidate; report.changed=true;
    report.refresh=attitude_refresh_rng(state,random); report.draws=report.refresh.draws;
    rng=random; return report;
}
HumanMenuCommand prepare_human_menu_command(State& state, const HumanSelection& fields,
    Word menu_code, Rng& rng) {
    auto copy=state; auto random=rng; HumanMenuCommand report;
    report.refresh=attitude_refresh_rng(copy,random); report.draws=report.refresh.draws;
    report.control_return=human_menu_control(menu_code);
    if(report.control_return!=0)
        human_commit(copy,fields.pending_action,fields.selected_class,fields.selected_capacity);
    state=copy; rng=random; return report;
}
} // namespace nuclear
