#pragma once

#include <filesystem>
#include <memory>
#include <string_view>
#include <vector>

struct SDL_Renderer;

namespace eon {

// Renderer-only launcher text. It consumes Project Eon's bundled UI font
// assets and has no connection to original game media or in-game text.
class UnicodeTextRenderer {
public:
    static std::unique_ptr<UnicodeTextRenderer> create(SDL_Renderer* renderer,
        const std::filesystem::path& font_directory);
    ~UnicodeTextRenderer();

    UnicodeTextRenderer(const UnicodeTextRenderer&) = delete;
    UnicodeTextRenderer& operator=(const UnicodeTextRenderer&) = delete;

    [[nodiscard]] bool draw(float x, float y, std::string_view utf8) const;

private:
    UnicodeTextRenderer(void* engine, void* font, std::vector<void*> fallback_fonts);
    void* engine_ = nullptr;
    void* font_ = nullptr;
    std::vector<void*> fallback_fonts_;
};

} // namespace eon
