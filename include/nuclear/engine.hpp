#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace nuclear {
using Word = std::uint16_t;
constexpr std::int32_t signed_word(Word x) noexcept {
    return x < 0x8000u ? std::int32_t(x) : std::int32_t(x) - 65536;
}
constexpr Word add_word(Word a, Word b) noexcept { return Word(std::uint32_t(a) + b); }
constexpr Word sub_word(Word a, Word b) noexcept { return Word(std::uint32_t(a) - b); }
constexpr std::int32_t signed_byte(std::uint8_t x) noexcept {
    return x < 0x80u ? std::int32_t(x) : std::int32_t(x) - 256;
}
constexpr std::size_t actor_count = 5, city_count = 5, human_actor = 4;
constexpr std::size_t actor_bytes = 0x7a;
struct Unsupported : std::runtime_error {
    using std::runtime_error::runtime_error;
};

// Bits are stored unsigned; signed_word implements signed 68000 comparisons.
// Host structure padding is irrelevant: use the explicit big-endian codec.
struct Actor {
    Word profile{}, action{}, target_actor{}, target_city{};
    std::array<std::uint32_t, 5> unknown_longs{};  // +08
    std::array<std::uint8_t, 5> classifications{}; // +1c; interpretation incomplete
    std::uint8_t unknown_21{};
    std::array<Word, 5> scores{}, city_x{}, city_y{}, population{};
    std::array<std::uint8_t, 20> unknown_4a{};
    std::array<Word, 4> missiles{}, warheads{};
    std::array<Word, 2> bombers{}, defenses{};
    Word magnitude{}, tally{};
};
using ActorImage = std::array<std::uint8_t, actor_bytes>;
Actor decode_actor(const ActorImage &);
ActorImage encode_actor(const Actor &);
struct State {
    // Modern roster occupancy; absent slots are neither living nor eliminated players.
    std::array<bool, actor_count> participating{{true, true, true, true, true}};
    std::array<Actor, actor_count> actors{};
    std::array<Word, actor_count> prepared_capacity{}; // 132600
    std::array<Word, actor_count> selected_opponent{}; // 13263a
    std::array<Word, actor_count> identity{}, remaining_total{}, pass_processed{};
    Word human_prepared_capacity{}; // 132616, separate from AI array at 132600
};
// Explicit seed required: original OS/time seeding is outside this core.
class Rng {
    std::uint32_t seed_;

  public:
    explicit Rng(std::uint32_t seed) : seed_(seed) {}
    Word next() noexcept;
    void reseed_word(Word seed) noexcept { seed_ = seed; } // 11ab14: zero-extended word argument
    std::uint32_t seed() const noexcept { return seed_; }
};
struct Action3Decision {
    Word selected_action{}, gate_roll{}, event_roll{}, draws{};
    bool substituted{};
};
// Decision only: original event roll1..100 -> selected action code.
// No action commit or event-specific setup (including quake/transfer) occurs.
Word action3_event_for_roll(Word event_roll);
// Non-action3: zero draws. Action3: one gate draw, second only if gate<30.
// Unused report rolls are zero. RNG stops before any event-specific setup.
Action3Decision decide_action3_substitution(Word current_action, Rng &);
Word initial_city_population(Word profile, bool human, Word original_mode, Rng &);
// 10c502 only. Profiles/identities must already be chosen; preserves bytes and
// globals this routine does not write. original_mode is word at 131be2.
void initialize_state(State &, Word original_mode, Rng &);
Word populated_cities(const Actor &) noexcept; // 10b590
Word living_actors(const State &) noexcept;
struct EndScore {
    Word positive_cities{}, population_total{}, score{};
};
// 113b36 returns last living actor, or-1; it does not assert uniqueness.
int last_surviving_actor(const State &) noexcept;
// 113b6e: raw word population sum plus25*(positive city count-1), wrapped.
EndScore end_game_score(const Actor &) noexcept;
bool end_condition(const State &) noexcept;                // 10083c..10086c, not winner selection
Word clamp_loss(Word loss, Word population) noexcept;      // 104cd2
Word population_add(Word population, Word delta) noexcept; // 103b96
Word population_subtract(Word population, Word loss) noexcept; // 1051a6
Word class_capacity(Word index) noexcept;                      // 10cb9a
Word class_effect(Word index) noexcept;                        // 10cb62
int highest_missile(const Actor &) noexcept;                   // 10ca80
int highest_warhead(const Actor &) noexcept;                   // 10cb1c
int highest_bomber(const Actor &) noexcept;                    // 10ca3a
int fitting_warhead(const Actor &, Word capacity) noexcept;    // 10cac6
// Uses profile of chooser, city populations of target. Unsupported if the
// original random scan would never terminate (profiles 3/4, no positive city).
int choose_city(const Actor &chooser, const Actor &target, Rng &); // 10b5d0
extern const std::array<std::array<Word, 5>, 5> category_one_table;
extern const std::array<std::array<Word, 5>, 5> category_two_table;
// Row = relationship bucket + 2; column = profile (call site 10c06c..10c074).
Word ai_category_for_roll(Word relation_row, Word profile, Word roll); // 10b50e slice
Word ai_category(Word relation_row, Word profile, Rng &);              // consumes one draw
Word relationship_bucket(Word score) noexcept; // 10add0, signed result -2..2
Word relationship_tier(Word score) noexcept;   // 10ae40, result 0..2
// Raw table result from 10b00e; update_attitudes multiplies its low word by100.
Word attitude_table_value(Word tier, Word previous_classification, Word profile);
void update_attitudes(State &); // 10b0ce, both ordered passes
Word event_score_delta(Word profile, Word tier, Word event_type, Word magnitude); // 10ac8e
void apply_relationship_event(State &, std::size_t acting_actor, Word event_type,
                              Word affected_actor, Word magnitude); // 10ae90
void perturb_profile3_scores(State &, Rng &);                       // main slice 100744..10083a
void plan_propaganda(State &, std::size_t actor, std::size_t target, Rng &); // 10b79e
bool plan_defense(State &, std::size_t actor);                               // 10bde2
// Returns relationship bucket of the maximum-score candidate, even when the
// random selection stores the second-best candidate in selected_opponent.
Word select_opponent(State &, std::size_t actor, Rng &); // 10b238
// Transactional on unsupported uninitialized-local path in the original.
void plan_weapons(State &, std::size_t actor, std::size_t target, Rng &); // 10b832

enum class InventoryKind { Warhead, Missile, Bomber, Defense };
struct ProducedItem {
    InventoryKind kind;
    std::size_t index;
};
ProducedItem production_item(Word kind_roll, Word class_roll); // decision slice
struct Production {
    // Numbered 1..3 as in instructions; zero for non-human actors.
    Word human_message_variant{};
    std::array<Word, 4> warheads{}, missiles{};
    std::array<Word, 2> bombers{}, defenses{};
    Word draws{};
};
// 10ce4a AI paths only. Human actor4 requires explicit persistent message
//  context through produce_human; this context-free overload throws.
Production produce(State &, std::size_t actor, Rng &);
struct AiProductionPhase {
    std::array<Production, 4> production{};
    std::array<bool, 4> produced{};
    Word draws{};
};
// 105330..10536e: living AI0..3 with action6 only; human not included.
//  Caller establishes resolver-end phase; no round scheduling inferred.
AiProductionPhase finish_ai_production(State &, Rng &);
struct CombatRoll {
    Word effect{}, message_variant{};
    std::uint32_t draws{};
};
// Initial normal attack handler only: missile117596..1175d6/bomber1164e8..116528.
// Re-roll both six-sided remainders whenever their sum+2 is7; then message draw.
CombatRoll begin_attack_roll(Word action, Rng &);
struct CombatImpact {
    Word effect{}, loss{};
};
// 10455c..104644, entered after an established nonzero/non-ffff effect.
//  Adds one d6 to attacking magnitude, signed-upper-caps local loss, then
//  applies defender action7 interception. Population commit is separate.
CombatImpact prepare_combat_impact(State &, std::size_t attacker, std::size_t target,
                                   std::size_t city, Word effect, Rng &);
// 10511e..1051d0, only after caller establishes pending-damage gate13254e.
//  Signed upper cap, SUB.W population, then relationship event0; no RNG.
//  UI handshakes omitted; unsupported relationship inputs roll back everything.
Word combat_population_commit(State &, std::size_t attacker, std::size_t target, std::size_t city);
struct Damage {
    Word loss{}, action{}, prepared_capacity{};
};
// Applies only the verified effect-2..13 arithmetic branch. Caller supplies
// the existing local loss and magnitude==25 snapshot (actor magnitude == 25 at 102a98).
// Does not select an effect, resolve collisions, or apply population damage.
Damage damage_effect(Word effect, Word loss, Word population, Word action, Word prepared_capacity,
                     bool magnitude_was_25);
struct CombatEvaluation {
    Word effect{};
    Damage damage{};
};
// Bounded10455c..104d2a: impact draw/interception, effect arithmetic, final
// magnitude store. Caller supplies pre-action magnitude==25 snapshot and collision gate.
// Does not commit population or choose collision timing. Unsupported rolls back.
CombatEvaluation evaluate_combat_impact(State &, std::size_t attacker, std::size_t target,
                                        std::size_t city, Word effect, bool magnitude_was_25,
                                        Rng &);
// Isolated effect 0x45 slice, not a full action-16 resolver. Draws before the
// population mutation at 10432e belong to the handler and remain separate.
std::size_t double_largest_city(Actor &); // 104296..10433c
// Initial growth handler1119b8..111ee6: message draw then magnitude5..25.
Word growth_handler_initialize(Actor &, Rng &);
// Initial quake handler11245e..11286e: one message draw, clear own target city.
// Does not overwrite magnitude/action/profile; later notice remains separate.
Word quake_handler_initialize(Actor &, Rng &);
// Initial action11/12 handlers: first NONZERO own city, magnitude copy,
// target_city write; one message draw (mod4/mod3). No population clear yet.
// All-zero scan reaches out-of-city memory in original; host rejects it.
Word city_loss_handler_initialize(Actor &, Word action, Rng &);
void growth_commit(Actor &, std::size_t city);    // effect16, 103b2a..103b96
void radiation_population_commit(Actor &, Rng &); // effect0x15, 104118..104248
// Effect0x11 slice: clears this actor's target_city, sets action0x11 and
// consumes the original message draw. Returns host message variant1..3.
// Stored target_actor and magnitude are preserved; frame gate is external.
Word city_removal_commit(Actor &, Rng &); // 103bcc..103cc8
// Effect0x56 notice slice only: action and one message draw, no population
// write in the original block. Full quake damage resolution is unsupported.
Word quake_effect_notice(Actor &, Rng &);
// Selected quake branch102d06..102daa: one draw, circular NONZERO scan.
// Sets action86, self target and magnitude; does not change population.
// All-zero population is Unsupported without consuming RNG or changing state.
std::size_t quake_event_prelude(State &, std::size_t actor, Rng &); // 103cca..103d70
// Effect0x12: own selected city is cleared; action0x12 and one message draw.
// Original optional audio and frame/UI synchronization stay outside the core.
Word weight_drop_commit(Actor &, Rng &); // 103db4..103eac
struct CowLoss {
    Word loss{}, message_variant{};
}; // host report of original locals
// Effect0x13: signed upper cap only, SUB.W from stored target population,
// one message draw1..5. Original acting magnitude is not overwritten by cap.
CowLoss cow_population_commit(State &, std::size_t actor, Rng &); // 103eae..104036
// Effect0x14 arrival slice: add acting magnitude to stored target city with
// ADD.W, set action0x14. Source deduction/selection occurs earlier, not here.
void transfer_arrival_commit(State &, std::size_t actor); // 104038..104116
struct TransferPrelude {
    Word source_city{}, destination_actor{}, destination_city{}, magnitude{};
    std::uint32_t draws{}, candidate_draws{}; // host trace, excludes prior event-selection rolls
};
// Already-selected action3 substitution branch (second roll24 or25).
// Deducts source, sets action14 and destination; arrival remains a separate call.
// The first fixed-profile scan's overwritten result is proven unobservable;
// no unmodeled DATA-tail values are fabricated for it. Unsupported paths roll
// back state and RNG (invalid source/divide-by-zero/no surviving destination).
TransferPrelude transfer_event_prelude(State &, std::size_t actor, Word substitution_roll, Rng &);
struct Action3Prelude {
    Action3Decision decision{};
    std::uint32_t draws{};
};
// Complete bounded102c18..102f88 substitution/setup. No frame or effect commit.
// Unsupported setup rolls back state and RNG, including selection draws.
Action3Prelude apply_action3_prelude(State &, std::size_t actor, Rng &);
struct PropagandaTransfer {
    Word loss{}, source_city{}, destination_city{}, draws{};
};
// Effect0d branch103878..103a0e after effect marker: relationship event2,
// signed upper-only source cap/subtraction then own choose_city and addition.
// Human forces profile2 for destination and then CLEARS profile to0.
// Unsupported city/chooser/table inputs roll back state/RNG.
PropagandaTransfer propaganda_commit(State &, std::size_t actor, Rng &);
// Effect15 mutation slice: source is chosen from the acting actor's cities.
// Does not run the handler, schedule frames, or update relationship scores.
std::size_t defection_commit(State &, std::size_t actor, Rng &);
// 106716..10674c: four entry values stored at13260c, before UI calls.
std::array<Word, 4> human_entry_rolls(Rng &);
struct HumanAnimationEndpoint {
    Word direction{}, timer{};
    std::uint32_t draws{};
};
// 10a33c..10a3ce only: caller supplies NEW index after movement and endpointA.
//  External callback scheduling, timer decrement, endpointB overwrite excluded.
HumanAnimationEndpoint human_animation_endpoint(Word new_index, Word endpoint, Word direction,
                                                Word timer, Rng &) noexcept;
struct HumanAnimationSlot {
    Word index{}, direction{}, endpoint_a{}, endpoint_b{}, timer{};
};
struct HumanAnimationTail {
    std::array<HumanAnimationSlot, 4> slots{}; // original indices5..8, in order
    std::array<bool, 4> render{};
    std::array<Word, 4> render_indices{}; // pre-movement index
    std::uint32_t draws{};
};
// 10a268..10a46c numeric tail only; caller schedules invocations explicitly.
//  Bitmap blit requested before movement; no actual graphics/OS work here.
HumanAnimationTail advance_human_animation_tail(const std::array<HumanAnimationSlot, 4> &,
                                                Rng &) noexcept;
struct AttitudeRefreshRng {
    std::array<Word, 4> random_periods{};
    std::array<bool, 4> randomized{};
    Word draws{};
};
// 10d45c..10d7fc: preserve status-display RNG separately from rendering.
//  Living AI with nonnegative displayed bucket consumes two %150 draws+2.
AttitudeRefreshRng attitude_refresh_rng(const State &, Rng &);
struct MessageRng {
    std::uint32_t noise_calls{}, draws{};
};
// 10da16 text-loop RNG coupling via113170:128 draws per nonspace byte,
//  plus one128-draw call if prior cursor !=(78,182). Stops at embedded NUL.
//  Bounded verified short-string domain<=32767 bytes. Generates no audio.
//  Cursor updates/font metrics remain presentation state, supplied by caller.
MessageRng advance_message_rng(std::string_view text, Word prior_cursor_x, Word prior_cursor_y,
                               Rng &);
// Compressed original130d3c/130d3e predicate, NOT a renderer/font model.
// true only for original fresh DATA origin(78,182); after any10da16 message
// trackedY becomes174, so later entries always consume initial128 draws.
// Persist across game restart unless original DATA globals are reloaded.
struct MessageSequenceRng {
    bool at_initial_origin;
};
MessageRng advance_message_sequence(MessageSequenceRng &, std::string_view text, Rng &);
struct HumanProduction {
    Production inventory{};            // draws counts direct inventory/header draws only
    std::vector<std::string> messages; // original ordered text, no UI output
    std::uint32_t total_draws{};       // includes every message RNG draw
};
// 10d104..10d37a original summary formatting/order, up to6 produced items.
std::vector<std::string> production_summary_messages(const Production &);
// 10ce4a HUMAN path with REQUIRED persistent tracked-message context.
//  Header text RNG occurs before inventory rolls; summaries after all items.
//  No graphics/audio/OS calls. State/context/RNG commit transactionally.
HumanProduction produce_human(State &, MessageSequenceRng &, Rng &);
struct HumanEntryPrefix {
    std::array<Word, 4> entry_rolls{};
    AttitudeRefreshRng first_refresh{}, second_refresh{};
    MessageRng empty_message{};
    bool produced{};
    HumanProduction production{};
    std::uint32_t total_draws{};
};
// 106716..106c14 only:4 entrydraws, status refresh, EMPTY message, second
//  status refresh, then humanproduction if previous action6. Intervening
//  icon/window calls have no retained direct/transitive LCG path or actor writes.
//  Returns captured13260c values; full prior-action text/input loop is separate.
HumanEntryPrefix prepare_human_entry_prefix(State &, MessageSequenceRng &, Rng &);
struct HumanSelection {
    Word pending_action{}, selected_class{0xffff}, selected_capacity{};
};
// Verified gameplay field writes from m/z/l branches107aaa..1081ae only.
// action2/4/7, choice0=auto, otherwise original one-based inventory class.
// false is original rejected command; Unsupported marks unmodeled/OOB paths.
// No resource consumption or actor.action commit. UI calls (including status
// refresh RNG) remain separate; this is not a whole input-event transition.
bool select_human_delivery_fields(State &, HumanSelection &, Word action, Word choice);
enum class HumanWarheadCheck {
    accepted,
    no_warhead,
    no_capacity,
    too_large,
    no_prepared_delivery,
    no_target
};
// 107574..10777e guard prefix only; retains class/capacity writes on later
//  rejection exactly. target_marker is original local-36, tested againstffff.
HumanWarheadCheck select_human_warhead_fields(const State &, HumanSelection &, Word choice,
                                              Word target_marker);
// 1078b0..1079c0 final fields, AFTER discarded draw and UI calls handled by caller.
//  Original actor.action/inventory remain untouched until human_commit.
void finish_human_attack_fields(State &, HumanSelection &);
// 10743e..10744a: pending3 is stored even if target marker isffff.
bool begin_human_propaganda_fields(HumanSelection &, Word target_marker) noexcept;
// 10750a..107550 /108336..10837c: class%4, discarded%100+1,
//  magnitude%10+1. Original messages occur BEFORE these three direct draws.
//  Returns discarded local roll for evidence; no actor target/action writes.
Word roll_human_propaganda_fields(State &, HumanSelection &, Rng &);
// 1043a2..1043e6 special-effect dispatch after established once-only marker.
//  Supports0d,0f,10..15,45,56 hex using verified commit slices; no scheduling.
//  Transactional for Unsupported paths. Does not initialize handlers or rounds.
void apply_special_event_effect(State &, std::size_t actor, Word effect, Rng &);
// 1073ba..107430 q guard; only pending1/3/5 require target_marker!=ffff.
bool human_confirmation_ready(Word pending_action, Word target_marker) noexcept;
// Pure guard+108c34 final fields; original UI cleanup/message RNG is external.
bool confirm_human_selection_fields(State &, const HumanSelection &, Word target_marker);
// 1079ee production choice gameplay field only; message/UI work external.
void select_human_production_fields(HumanSelection &) noexcept;
// 1081de..108200 a..d command byte cycle in HUMAN classifications[0..3].
//  SUBQ.B then signed<-2 reset2, including original overflow patterns.
std::uint8_t cycle_human_classification(State &, std::size_t target);
// 107318..107356 after menu eventclass100 and UI refresh: raw menu code.
// 0=no control return,1/2=original human loop return values. No UI RNG here.
Word human_menu_control(Word menu_code) noexcept;
// 10825a/108492/108498 target fields from a city gadget in viewed actor0..3.
//  Viewed human4 rejects before city indexing. Gadget availability/UI/RNG is
//  external; no new population or self-target rule added to the field stores.
bool select_human_target_fields(State &, Word viewed_actor, Word city);
// Final commit slice only: input validation/selection happens before this in
// 106712. Includes score penalties and resource arithmetic, no menu emulation.
void human_commit(State &, Word pending_action, Word selected_class, Word selected_capacity);
// 106c14..107194 prior-action descriptions; exact signed formatting and order.
//  Names are explicit runtime DATA1303f2 values. Unknown actions have no messages.
//  Invalid referenced indices/oversized text roll back context and RNG.
struct PriorHumanMessages {
    std::vector<std::string> messages;
    std::uint32_t draws{};
};
PriorHumanMessages prior_human_messages(const State &, Word previous_selected_class,
                                        const std::array<std::string, 5> &names,
                                        MessageSequenceRng &, Rng &);
struct HumanEntryTail {
    AttitudeRefreshRng refresh{};
    Word speech_roll{}; // 0=no speech, otherwise original%69+1
    Word welcome_index{0xffff};
    std::vector<std::string> messages;
    std::uint32_t draws{};
};
// 107194..107234: third status, conditional speech RNG, target clear, welcome.
//  Explicit132620 flag; no UI, speech output or event-loop behavior.
HumanEntryTail prepare_human_entry_tail(State &, Word &welcome_flag, MessageSequenceRng &, Rng &);
struct HumanEntry {
    HumanEntryPrefix prefix{};
    PriorHumanMessages prior{};
    HumanEntryTail tail{};
    std::uint32_t draws{};
};
// 106716..107234 complete verified pre-input sequence only; no commands.
HumanEntry prepare_human_entry(State &, Word previous_selected_class,
                               const std::array<std::string, 5> &actor_names, Word &welcome_flag,
                               MessageSequenceRng &, Rng &);
struct HumanConfirmation {
    bool ready{};
    std::vector<std::string> messages;
    std::uint32_t draws{};
};
// 1073ba..107434 q guard and rejection messages. Accepted cleanup/commit separate.
HumanConfirmation prepare_human_confirmation(Word pending_action, Word target_marker,
                                             MessageSequenceRng &, Rng &);
struct HumanPropagandaCommand {
    bool target_selected{};
    Word discarded_roll{};
    std::vector<std::string> messages;
    std::uint32_t draws{};
};
// 10743e..107554 p prefix/messages/direct rolls. No later event routing or commit.
HumanPropagandaCommand
prepare_human_propaganda_command(State &, HumanSelection &, Word target_marker,
                                 const std::array<std::string, 5> &actor_names,
                                 MessageSequenceRng &, Rng &);
// 1079d2..107a82 s selection text/pending6 only; later event routing separate.
PriorHumanMessages prepare_human_production_command(Word previous_action, HumanSelection &,
                                                    MessageSequenceRng &, Rng &);
// q guard/text -> verified zero-RNG accepted cleanup ->108c34 human_commit.
// true ready means commit completed. Unsupported leaves state/context/RNG intact.
HumanConfirmation confirm_human_command(State &, const HumanSelection &, Word target_marker,
                                        MessageSequenceRng &, Rng &);
struct HumanWarheadCommand {
    HumanWarheadCheck check{HumanWarheadCheck::accepted};
    Word discarded_roll{};
    AttitudeRefreshRng refresh{};
    std::vector<std::string> messages;
    std::uint32_t draws{};
};
// 10757e..1079c4 w field guards, original messages, discarded draw/status,
//  and final attack fields. Resource/action commit remains at accepted q.
HumanWarheadCommand prepare_human_warhead_command(State &, HumanSelection &, Word choice,
                                                  Word target_marker,
                                                  const std::array<std::string, 5> &actor_names,
                                                  MessageSequenceRng &, Rng &);
struct HumanDeliveryCommand {
    bool accepted{};
    AttitudeRefreshRng refresh{};
    std::vector<std::string> messages;
    std::uint32_t draws{};
};
// 107ab4..1081ae m/z/l selection with exact status/text RNG order.
HumanDeliveryCommand prepare_human_delivery_command(State &, HumanSelection &, Word action,
                                                    Word choice, MessageSequenceRng &, Rng &);
struct HumanCityTarget {
    bool accepted{};
    Word target_marker{}; // valid only if accepted;108406 copy of caller's prior display actor
    Word discarded_roll{};
    std::vector<std::string> messages;
    std::uint32_t draws{};
};
// 10825a..10849e normalized city gadget slice only; excludes later view routing.
//  logical_view=local-38; prior_display=local-26 (may differ, includingffff).
HumanCityTarget prepare_human_city_target(State &, HumanSelection &, Word logical_view,
                                          Word prior_display, Word city,
                                          const std::array<std::string, 5> &actor_names,
                                          MessageSequenceRng &, Rng &);
struct HumanViewChange {
    Word logical_view{}, display_actor{};
    bool changed{};
    AttitudeRefreshRng refresh{};
    std::uint32_t draws{};
};
// 108534..108b10 after normalized actor selection; no pixel/event mapping.
//  UI and fixed speech use noLCG/10da16; changed view has one statusrefresh.
HumanViewChange change_human_actor_view(const State &, Word logical_view, Word display_actor,
                                        Word requested_actor, Rng &);
struct HumanMenuCommand {
    Word control_return{}; // 0=continue,1/2=completed original menu return
    AttitudeRefreshRng refresh{};
    std::uint32_t draws{};
};
// 1072bc..10735a eventclass100 only; status BEFORE decode, includinginvalid.
//  Accepted menu returns commit pending fields through zero-RNG exitcleanup.
HumanMenuCommand prepare_human_menu_command(State &, const HumanSelection &, Word menu_code, Rng &);
struct AlienPrelude {
    Word message_variant{}, loss{}, initial_x{};
};
AlienPrelude alien_prelude(Actor &, Rng &); // 117f48 frame zero random draws only

struct Dispatch {
    Word action;
    std::uint32_t handler;
};
// Complete handler-selection map; zero means no handler in this dispatch.
struct EventInitialization {
    Word effect{}, message_variant{}, alien_initial_x{};
    std::uint32_t draws{};
};
// Direct RNG and gameplay writes of initial-frame special handlers only.
// Supports3,8,9,11..16,86; no effect commit, frame progression or round timing.
EventInitialization initialize_event_handler(Actor &, Rng &);
// Initial action2/4/6/7 handlers only.6 applies event5,7 event4 BEFORE
// one message RNG draw;2/4 draw message only. Returns selector16 for2,
// 255 for4/6/7. Does not assert a later effect marker or produce inventory.
EventInitialization initialize_preparation_handler(State &, std::size_t actor, Rng &);
Dispatch action_dispatch(Word action) noexcept;
// Complete round and human event loop remain explicitly unsupported.
void resolve_action(State &, std::size_t actor, Rng &);
void human_turn(State &);
void ai_turn(State &, Rng &); // 10bfb8 planner only; excludes 10b0ce attitude update
// Boundary for future integration. No UI, audio, animation, or OS implementation.
struct Presentation {
    virtual ~Presentation() = default;
    virtual void request_animation(std::uint32_t original_handler, Word actor) = 0;
    virtual void request_text(std::uint32_t original_string_address) = 0;
    virtual void request_audio(std::uint32_t original_call_site) = 0;
};
struct Input {
    virtual ~Input() = default;
    virtual Word next_command() = 0; // protocol intentionally not defined yet
};
} // namespace nuclear
