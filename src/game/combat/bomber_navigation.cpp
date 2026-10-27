#include "nuclear/bomber_navigation.hpp"
namespace nuclear {
Word bomber_heading_to(Word x, Word y, Word tx, Word ty) noexcept {
    if (x == tx && y == ty)
        return 0;
    if (x == tx)
        return signed_word(y) < signed_word(ty) ? 5 : 1;
    if (y == ty)
        return signed_word(x) < signed_word(tx) ? 3 : 7;
    if (signed_word(x) < signed_word(tx))
        return signed_word(y) < signed_word(ty) ? 4 : 2;
    return signed_word(y) < signed_word(ty) ? 6 : 8;
}
void step_heading_word(Word &x, Word &y, Word heading) noexcept {
    switch (heading) {
    case 1:
        y = sub_word(y, 1);
        break;
    case 2:
        y = sub_word(y, 1);
        x = add_word(x, 1);
        break;
    case 3:
        x = add_word(x, 1);
        break;
    case 4:
        x = add_word(x, 1);
        y = add_word(y, 1);
        break;
    case 5:
        y = add_word(y, 1);
        break;
    case 6:
        y = add_word(y, 1);
        x = sub_word(x, 1);
        break;
    case 7:
        x = sub_word(x, 1);
        break;
    case 8:
        y = sub_word(y, 1);
        x = sub_word(x, 1);
        break;
    default:
        break;
    }
}
BomberControlTick bomber_control_tick(BomberControlState &s, Word frame, Word launch_x,
                                      Word launch_y) noexcept {
    BomberControlTick result;
    if (signed_word(frame) >= 800)
        return result;
    if (s.heading != 0) {
        s.output_marker = add_word(s.heading, 0xce);
        result.marker_written = true;
    } else {
        if (s.return_flag != 0)
            return result;
        s.output_marker = 0x5208;
        result.marker_written = true;
        result.emitted_arrival = true;
        s.target_x = launch_x;
        s.target_y = launch_y;
        s.return_flag = 1;
        s.heading = s.saved_heading;
    }
    s.output_x = s.x;
    s.output_y = s.y;
    s.output_width = 16;
    s.output_height = 16;
    result.outputs_written = true;
    s.speed_count = sub_word(s.speed_count, 1);
    if (s.speed_count != 0)
        return result;
    s.speed_count = 1;
    s.turn_count = sub_word(s.turn_count, 1);
    if (s.turn_count == 0) {
        s.desired_heading = bomber_heading_to(s.x, s.y, s.target_x, s.target_y);
        if (s.desired_heading != 0) {
            s.predecessor_count = 0;
            s.successor_count = 0;
            Word candidate = s.heading;
            for (unsigned attempt = 0; attempt < 8; ++attempt) {
                candidate = sub_word(candidate, 1);
                if (signed_word(candidate) < 1)
                    candidate = 8;
                s.predecessor_count = add_word(s.predecessor_count, 1);
                if (candidate == s.desired_heading)
                    break;
            }
            candidate = s.heading;
            for (unsigned attempt = 0; attempt < 8; ++attempt) {
                candidate = add_word(candidate, 1);
                if (signed_word(candidate) > 8)
                    candidate = 1;
                s.successor_count = add_word(s.successor_count, 1);
                if (candidate == s.desired_heading)
                    break;
            }
            s.rotation_step =
                signed_word(s.successor_count) < signed_word(s.predecessor_count) ? 1 : Word(65535);
            if (s.desired_heading != s.heading) {
                s.heading = add_word(s.heading, s.rotation_step);
                if (signed_word(s.heading) > 8)
                    s.heading = 1;
                if (signed_word(s.heading) < 1)
                    s.heading = add_word(s.heading, 8);
                s.turn_count = 8;
            } else
                s.turn_count = 1;
        } else {
            s.saved_heading = s.heading;
            s.heading = 0;
            s.turn_count = 8;
        }
        // 1168f8 subtracts8 from CURRENT Y, not targetY. Preserve the bug.
        if (s.x != sub_word(s.target_x, 8) &&
            (signed_word(s.y) > signed_word(sub_word(s.y, 8)) ||
             signed_word(s.y) < signed_word(add_word(s.target_y, 8))))
            s.turn_count = 1;
    }
    const Word old_x = s.x, old_y = s.y;
    step_heading_word(s.x, s.y, s.heading);
    result.moved = s.x != old_x || s.y != old_y;
    return result;
}
void initialize_bomber_control_fields(BomberControlState &motion, const State &state,
                                      std::size_t actor, Word launch_x, Word launch_y) {
    if (actor >= actor_count)
        throw Unsupported("bomber actor outside0..4");
    const auto &attacker = state.actors[actor];
    if (attacker.target_actor >= actor_count || attacker.target_city >= city_count)
        throw Unsupported("bomber target outside actor/city slots");
    const auto &target = state.actors[attacker.target_actor];
    motion.return_flag = 0;
    motion.turn_count = 8;
    motion.heading = 3;
    motion.desired_heading = 3;
    motion.x = launch_x;
    motion.y = launch_y;
    motion.target_x = target.city_x[attacker.target_city];
    motion.target_y = target.city_y[attacker.target_city];
    motion.speed_count = 1;
}
BomberActionStart start_bomber_action(State &state, BomberControlState &motion, std::size_t actor,
                                      Word pass, Word launch_x, Word launch_y, Rng &rng) {
    if (actor >= actor_count || state.actors[actor].action != 5)
        throw Unsupported("bomber action start requires indexed action5");
    auto copy = state;
    auto control = motion;
    auto random = rng;
    BomberActionStart result;
    result.resolver = start_resolver_action(copy, actor, pass, random);
    if (result.resolver.initialized) {
        initialize_bomber_control_fields(control, copy, actor, launch_x, launch_y);
        result.clear_seen_59d8 = true;
        result.first_tick = bomber_control_tick(control, 0, launch_x, launch_y);
    }
    state = copy;
    motion = control;
    rng = random;
    return result;
}
} // namespace nuclear
