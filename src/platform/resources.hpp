#pragma once
#include <SDL.h>
#include <filesystem>
#include <string_view>
#include <vector>
namespace nuclear::assets {
const std::filesystem::path& root();
std::filesystem::path path(std::string_view relative);
struct Image { unsigned width{},height{};std::vector<unsigned char> rgba; };
Image image(std::string_view key,std::string_view mask={});
struct Wave { std::vector<unsigned char> samples;unsigned rate{}; };
// Returned samples are signed8 byte patterns, exactly matching the reference decoder.
Wave wave(std::string_view key);
}
