#include "nuclear/human_commands.hpp"
namespace nuclear {
HumanCommandStart start_human_commands(State& state, Word previous_class,
    Word previous_capacity, const std::array<std::string,5>& names,
    Word& welcome_flag, MessageSequenceRng& context, Rng& rng) {
    if(populated_cities(state.actors[4])==0)
        throw Unsupported("original human-entry caller requires a living human");
    auto copy=state; auto flag=welcome_flag; auto cursor=context; auto random=rng;
    HumanCommandStart result;
    result.entry=prepare_human_entry(copy,previous_class,names,flag,cursor,random);
    result.command_state.selection.selected_class=previous_class;
    result.command_state.selection.selected_capacity=previous_capacity;
    result.command_state.active=true;
    state=copy; welcome_flag=flag; context=cursor; rng=random; return result;
}
HumanCommandResult apply_human_command(State& state, HumanCommandState& command_state,
    HumanCommand command, const std::array<std::string,5>& names,
    MessageSequenceRng& context, Rng& rng) {
    if(!command_state.active) throw Unsupported("human command session is not active");
    auto copy=state; auto session=command_state; auto cursor=context; auto random=rng;
    HumanCommandResult result;
    const auto view=[&](Word actor) {
        const auto report=change_human_actor_view(copy,session.logical_view,session.display_actor,actor,random);
        session.logical_view=report.logical_view; session.display_actor=report.display_actor;
        result.draws+=report.draws;
    };
    switch(command.kind) {
    case HumanCommandKind::confirm: {
        auto report=confirm_human_command(copy,session.selection,session.target_marker,cursor,random);
        result.messages=report.messages; result.draws=report.draws;
        if(report.ready) session.active=false;
        break;
    }
    case HumanCommandKind::propaganda: {
        auto report=prepare_human_propaganda_command(copy,session.selection,session.target_marker,names,cursor,random);
        result.messages=report.messages; result.draws=report.draws;
        if(report.target_selected && session.display_actor==0xffff) view(session.target_marker);
        break;
    }
    case HumanCommandKind::production: {
        auto report=prepare_human_production_command(copy.actors[4].action,session.selection,cursor,random);
        result.messages=report.messages; result.draws=report.draws;
        if(session.display_actor==0xffff) view(4);
        break;
    }
    case HumanCommandKind::warhead: {
        auto report=prepare_human_warhead_command(copy,session.selection,command.argument,session.target_marker,names,cursor,random);
        result.messages=report.messages; result.draws=report.draws;
        if(report.check==HumanWarheadCheck::accepted) session.display_actor=0xffff;
        break;
    }
    case HumanCommandKind::missile:
    case HumanCommandKind::bomber:
    case HumanCommandKind::defense: {
        const Word action=command.kind==HumanCommandKind::missile?2:command.kind==HumanCommandKind::bomber?4:7;
        auto report=prepare_human_delivery_command(copy,session.selection,action,command.argument,cursor,random);
        result.messages=report.messages; result.draws=report.draws;
        if(report.accepted) session.display_actor=0xffff;
        break;
    }
    case HumanCommandKind::actor_view: view(command.argument); break;
    case HumanCommandKind::city_target: {
        const Word old_marker=session.target_marker;
        const bool hidden=session.display_actor==0xffff;
        auto report=prepare_human_city_target(copy,session.selection,session.logical_view,session.display_actor,
            command.argument,names,cursor,random);
        result.messages=report.messages; result.draws=report.draws;
        if(report.accepted) {
            session.target_marker=report.target_marker;
            if(session.selection.pending_action==3 && hidden) {
                if(old_marker==0xffff)
                    throw Unsupported("hidden propaganda city route requires original mouse coordinates");
                view(old_marker);
            }
        }
        break;
    }
    case HumanCommandKind::classification:
        cycle_human_classification(copy,command.argument); break;
    case HumanCommandKind::menu: {
        auto report=prepare_human_menu_command(copy,session.selection,command.argument,random);
        result.draws=report.draws;
        if(report.control_return!=0) {
            session.control_return=report.control_return; session.active=false;
        }
        break;
    }
    default: throw Unsupported("unknown normalized human command");
    }
    result.turn_finished=!session.active; result.control_return=session.control_return;
    state=copy; command_state=session; context=cursor; rng=random; return result;
}
} // namespace nuclear
