#pragma once
#include <cstdint>
#include <initializer_list>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace nuclear::text {
using Argument = std::variant<std::int32_t, std::uint32_t, std::string, char>;
struct Entry {
    std::uint32_t address;
    std::string_view text;
};
Entry lookup(std::uint32_t address); // throws on unknown address; no invented fallback
Entry table(std::string_view name, unsigned index); // null/missing entry throws
std::string format(std::uint32_t address, std::initializer_list<Argument> args);
std::string format(std::uint32_t address, const std::vector<Argument> &args);
// Explicit word sign extension for 68000 signed message arguments.
constexpr std::int32_t signed16(std::uint16_t word) noexcept {
    return word < 0x8000 ? std::int32_t(word) : std::int32_t(word) - 65536;
}
// Original handler message variants are supplied, never selected with extra RNG.
std::string action_announcement(std::uint16_t action, unsigned variant, std::string actor,
                                std::string target);
std::string attack_announcement(std::uint16_t action, unsigned variant, std::string actor,
                                std::string target);
} // namespace nuclear::text
