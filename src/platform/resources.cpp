#include "platform/resources.hpp"
#include "core/asset_registry.hpp"
#include <cstdlib>
#include <memory>
#include <png.h>
#include <stdexcept>
namespace nuclear::assets {
const std::filesystem::path &root() {
    static const auto value = []() {
        if (const char *override = std::getenv("NWR_ASSETS")) {
            auto p = std::filesystem::u8path(override);
            if (std::filesystem::is_regular_file(p / "data/manifest.json"))
                return p;
            throw std::runtime_error("NWR_ASSETS has no data/manifest.json");
        }
        std::unique_ptr<char, decltype(&SDL_free)> base(SDL_GetBasePath(), SDL_free);
        if (!base)
            throw std::runtime_error(SDL_GetError());
        const auto exe = std::filesystem::u8path(base.get());
        for (const auto &p :
             {exe / "assets", exe / "../share/nuclearwar/assets", exe / "../Resources/assets"})
            if (std::filesystem::is_regular_file(p / "data/manifest.json"))
                return p;
        throw std::runtime_error(
            "Game assets not found beside executable or in installation share directory");
    }();
    return value;
}
std::filesystem::path path(std::string_view relative) {
    return root() / std::filesystem::u8path(std::string(relative));
}
Image image(std::string_view key, std::string_view mask) {
    for (const auto &entry : images)
        if (entry.key == key && entry.mask == mask) {
            png_image png{};
            png.version = PNG_IMAGE_VERSION;
            std::size_t file_size = 0;
            std::unique_ptr<void, decltype(&SDL_free)> file(
                SDL_LoadFile(path(entry.file).u8string().c_str(), &file_size), SDL_free);
            if (!file)
                throw std::runtime_error(SDL_GetError());
            if (!png_image_begin_read_from_memory(&png, file.get(), file_size))
                throw std::runtime_error(png.message);
            struct Guard {
                png_image *value;
                ~Guard() { png_image_free(value); }
            } guard{&png};
            png.format = PNG_FORMAT_RGBA;
            Image out{png.width, png.height, std::vector<unsigned char>(PNG_IMAGE_SIZE(png))};
            if (!png_image_finish_read(&png, nullptr, out.rgba.data(), 0, nullptr))
                throw std::runtime_error(png.message);
            if (out.width != entry.width || out.height != entry.height)
                throw std::runtime_error("PNG dimensions disagree with manifest");
            return out;
        }
    throw std::runtime_error("Unknown image asset: " + std::string(key));
}
Wave wave(std::string_view key) {
    for (const auto &entry : audio)
        if (entry.key == key) {
            SDL_AudioSpec spec{};
            Uint8 *data = nullptr;
            Uint32 size = 0;
            if (!SDL_LoadWAV(path(entry.file).u8string().c_str(), &spec, &data, &size))
                return {};
            std::unique_ptr<Uint8, decltype(&SDL_FreeWAV)> guard(data, SDL_FreeWAV);
            if (spec.channels != 1 || spec.format != AUDIO_U8 || size != entry.samples ||
                unsigned(spec.freq) != entry.rate)
                throw std::runtime_error("WAV format disagrees with manifest");
            Wave out{{data, data + size}, unsigned(spec.freq)};
            for (auto &sample : out.samples)
                sample = static_cast<unsigned char>(sample + 128);
            return out;
        }
    return {};
}
} // namespace nuclear::assets
