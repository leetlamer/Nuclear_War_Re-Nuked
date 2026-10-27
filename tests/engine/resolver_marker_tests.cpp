#include "nuclear/resolver_markers.hpp"
#include <iostream>
#include <stdexcept>
using namespace nuclear;
int main() {
    unsigned checks = 0;
    const auto check = [&](bool p) {
        ++checks;
        if (!p)
            throw std::runtime_error("marker assertion");
    };
    for (unsigned value = 0; value < 65536; ++value) {
        const Word w = Word(value);
        const bool bit = (value & 0x400u) != 0;
        const auto fresh = consume_effect_marker(w, 0);
        check(fresh.frame_coordinate == Word(value - (bit ? 0x400u : 0u)));
        check(fresh.effect_applied == Word(bit ? 1 : 0));
        check(fresh.dispatch_effect == bit);
        for (Word applied : {Word(1), Word(0xffff)}) {
            const auto repeat = consume_effect_marker(w, applied);
            check(repeat.frame_coordinate == fresh.frame_coordinate);
            check(repeat.effect_applied == applied && !repeat.dispatch_effect);
        }
        const bool two = value == 0x19 || value == 0x56 || value == 0x10 || value == 0x13 ||
                         value == 0x11 || value == 0x14;
        check(resolver_frame_slots(w) == Word(two ? 2 : 1));
        const auto classified = classify_resolver_frame(w, 0, 0, 0);
        check(classified.completion_counter == Word(value == 0x5208 ? 0xffff : 0));
        check(classified.pending_value == Word(value == 0x5208 ? 15 : 0));
        check(classified.route == (value == 0x5208   ? ResolverFrameRoute::prepare_impact
                                   : value == 0x55f0 ? ResolverFrameRoute::end_frame
                                                     : ResolverFrameRoute::ordinary));
    }
    for (Word slot : {Word(0), Word(1), Word(2), Word(3), Word(4)}) {
        for (Word flag : {Word(0), Word(1), Word(15), Word(0xffff)}) {
            auto d = classify_resolver_frame(0x5208, slot, flag, 2);
            check(d.completion_counter == 1);
            check(d.pending_value == Word(flag ? 0 : 15));
            check(d.route ==
                  (flag ? ResolverFrameRoute::processed_slot : ResolverFrameRoute::prepare_impact));
        }
    }
    for (Word slot : {Word(5), Word(6), Word(32767)}) {
        auto d = classify_resolver_frame(0x5208, slot, 0, 1);
        check(d.route == ResolverFrameRoute::secondary_slot_cleanup && d.completion_counter == 0 &&
              d.pending_value == 0);
    }
    for (Word slot : {Word(0x8000), Word(0xffff)}) {
        bool rejected = false;
        try {
            (void)classify_resolver_frame(0x5208, slot, 0, 2);
        } catch (const Unsupported &) {
            rejected = true;
        }
        check(rejected);
        check(classify_resolver_frame(0x55f0, slot, 0, 2).route == ResolverFrameRoute::end_frame);
    }
    check(!consume_effect_marker(4, 0).dispatch_effect);
    // All branch combinations, including LONG class high bits and signed WORDs.
    for (bool present : {false, true})
        for (std::uint32_t cls : {0u, 8u, 0x2000u, 0x10008u, 0xffffffffu})
            for (Word code : {Word(0), Word(0x68), Word(0x69), Word(0xffff)})
                for (Word frame : {Word(0), Word(1), Word(0x8000), Word(0xffff)})
                    for (Word mode : {Word(0), Word(1), Word(0xffff)})
                        for (Word control : {Word(0), Word(1), Word(2), Word(0xffff)}) {
                            const ResolverInputControl previous{control, 0x1234, 0xffff};
                            const auto r =
                                process_resolver_input(present, cls, code, frame, mode, previous);
                            const bool suppress = present && cls == 8 && code == 0x68 && frame != 0;
                            check(r.state.slot_index == 0);
                            check(r.state.frameflag_f4 == Word(suppress ? 1 : 0x1234));
                            check(r.state.control == Word(suppress && mode != 0 ? 1 : control));
                            check(r.rewrite_reply_code == (present && cls == 0x2000));
                            check(r.reply_code == Word(present && cls == 0x2000 ? 2 : 0));
                            check(previous.control == control && previous.frameflag_f4 == 0x1234 &&
                                  previous.slot_index == 0xffff);
                        }
    for (unsigned slot = 0; slot < 65536; ++slot) {
        ResolverSlotWords primary{0x4400, 0x5208}, secondary{0xffff, 0x59d8};
        const auto saved = snapshot_resolver_slot(Word(slot), primary, secondary);
        // Later callbacks may mutate globals; the stack snapshot stays unchanged.
        primary = {};
        secondary = {};
        check(saved.coordinate == Word(slot == 0 ? 0x4400 : 0xffff));
        check(saved.marker == Word(slot == 0 ? 0x5208 : 0x59d8));
        const auto effect = consume_effect_marker(saved.coordinate, 0);
        check(effect.dispatch_effect);
        check(effect.frame_coordinate == Word(slot == 0 ? 0x4000 : 0xfbff));
    }
    for (Word direction : {Word(0), Word(1), Word(2), Word(0x8000), Word(0xffff)}) {
        Rng random(1), expected(1);
        const auto first = expected.next();
        const auto second = expected.next();
        const auto hit = human_animation_endpoint(0xffff, 0xffff, direction, 0x7777, random);
        check(hit.direction == Word(direction == 1 ? 0xffff : direction == 0xffff ? 1 : direction));
        check(hit.timer == Word(first % 150 + second % 150 + 2));
        check(hit.draws == 2 && random.seed() == expected.seed());
        const auto seed = random.seed();
        const auto miss = human_animation_endpoint(0, 0xffff, direction, 0x7777, random);
        check(miss.direction == direction && miss.timer == 0x7777 && miss.draws == 0);
        check(random.seed() == seed);
    }
    {
        Rng random(1), expected(1);
        std::array<HumanAnimationSlot, 4> slots{};
        for (auto &s : slots)
            s = {10, 1, 11, 11, 0};
        const auto r = advance_human_animation_tail(slots, random);
        for (unsigned i = 0; i < 8; ++i)
            (void)expected.next();
        check(r.draws == 8 && random.seed() == expected.seed());
        for (unsigned i = 0; i < 4; ++i) {
            check(r.render[i] && r.render_indices[i] == 10);
            // Both endpoints: two reversals and timer12, but two draws retained.
            check(r.slots[i].index == 11 && r.slots[i].direction == 1 && r.slots[i].timer == 12);
            check(slots[i].index == 10 && slots[i].timer == 0);
        }
        slots = {{{0xffff, 1, 0, 0, 5},
                  {3, 1, 4, 4, 1},
                  {0, 0xffff, 0xffff, 0xffff, 0},
                  {4, 2, 99, 6, 0}}};
        const auto mixed = advance_human_animation_tail(slots, random);
        check(!mixed.render[0] && mixed.slots[0].timer == 5);
        check(mixed.render[1] && mixed.slots[1].index == 3 && mixed.slots[1].timer == 0);
        check(mixed.slots[2].index == 0xffff && mixed.slots[2].direction == 0xffff &&
              mixed.slots[2].timer == 12);
        check(mixed.slots[3].index == 6 && mixed.slots[3].direction == 2 &&
              mixed.slots[3].timer == 12);
        check(mixed.draws == 2);
        (void)expected.next();
        (void)expected.next();
        check(random.seed() == expected.seed());
        const auto after = advance_human_animation_tail(mixed.slots, random);
        check(!after.render[2]); // wrappedffff is skipped NEXT invocation
    }
    std::cout << "7 groups, " << checks << " checks passed\n";
}
