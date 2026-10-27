#pragma once
#include "ui/portrait_mood.hpp"
#include "ui/turn_presentation.hpp"
#include <fstream>
#include <iomanip>
namespace nuclear::player {
class MoodAudit {
    std::ofstream file_;
    const session::Coordinator *owner_{};
    std::size_t observed_{};
    bool have_{};
    State last_{};
    std::array<std::string, 4> score_reason_{}, intent_reason_{}, keys_{};

  public:
    void new_session() {
        owner_ = nullptr;
        observed_ = 0;
        have_ = false;
        keys_ = {};
        score_reason_ = {};
        intent_reason_ = {};
    }
    explicit MoodAudit(const std::string &path) : file_(path) {
        if (!file_)
            throw Unsupported("cannot open developer mood trace");
    }
    void observe(const Player &player) {
        if (!player.coordinator())
            return;
        auto c = player.coordinator();
        if (owner_ != c || c->events().size() < observed_) {
            owner_ = c;
            observed_ = 0;
            have_ = false;
            keys_ = {};
        }
        for (; observed_ < c->events().size(); ++observed_) {
            const auto &e = c->events()[observed_];
            auto reason = std::string(session::phase_name(e.from)) + ":" + e.operation;
            for (std::size_t i = 0; i < 4; ++i) {
                if (!have_ || e.state_after.actors[i].scores[4] != last_.actors[i].scores[4])
                    score_reason_[i] = reason;
                if (!have_ || e.state_after.actors[4].classifications[i] !=
                                  last_.actors[4].classifications[i])
                    intent_reason_[i] = reason;
            }
            last_ = e.state_after;
            have_ = true;
        }
    }
    void pose(const MoodPose &p, const State &state, const std::string &provenance) {
        auto i = p.actor;
        std::string key = std::to_string(p.identity) + ":" + std::to_string(p.profile) + ":" +
                          std::to_string(p.score) + ":" + std::to_string(p.frame) + ":" +
                          std::to_string(p.speaking) + ":" + std::to_string(int(p.animation)) +
                          ":" + std::to_string(state.actors[4].classifications[i]) + ":" +
                          provenance;
        if (keys_[i] == key)
            return;
        keys_[i] = key;
        file_ << "{\"actor\":" << i << ",\"identity\":" << p.identity
              << ",\"profile\":" << p.profile << ",\"score_toward_human\":" << signed_word(p.score)
              << ",\"raw_bucket\":" << p.raw_bucket << ",\"display_bucket\":" << p.display_bucket
              << ",\"expression\":" << std::quoted(expression_name(p.display_bucket))
              << ",\"base_frame\":" << p.base_frame
              << ",\"current_frame\":" << (p.animation == FaceState::eliminated ? -1 : p.frame)
              << ",\"temporary_speaking\":" << (p.speaking ? "true" : "false")
              << ",\"animation\":" << int(p.animation)
              << ",\"human_declaration\":" << int(signed_byte(state.actors[4].classifications[i]))
              << ",\"indicator_frame\":" << (p.indicator ? int(*p.indicator) : -1)
              << ",\"source\":[" << p.source.x << "," << p.source.y
              << ",32,32],\"provenance\":" << std::quoted(provenance) << ",\"latest_score_change\":"
              << std::quoted(provenance == "developer immutable preview"
                                 ? std::string("developer-only score fixture; no gameplay mutation")
                                 : score_reason_[i])
              << ",\"latest_declaration_change\":"
              << std::quoted(provenance == "developer immutable preview"
                                 ? std::string("developer-only declaration fixture")
                                 : intent_reason_[i])
              << "}\n";
        file_.flush();
    }
};
} // namespace nuclear::player
