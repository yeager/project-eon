#include "launcher_text.hpp"

#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>

#include <array>

namespace eon {

UnicodeTextRenderer::UnicodeTextRenderer(void* engine, void* font,
    std::vector<void*> fallback_fonts)
    : engine_(engine), font_(font), fallback_fonts_(std::move(fallback_fonts)) {}

UnicodeTextRenderer::~UnicodeTextRenderer() {
    for (auto* fallback : fallback_fonts_) TTF_CloseFont(static_cast<TTF_Font*>(fallback));
    if (font_) TTF_CloseFont(static_cast<TTF_Font*>(font_));
    if (engine_) TTF_DestroyRendererTextEngine(static_cast<TTF_TextEngine*>(engine_));
    TTF_Quit();
}

std::unique_ptr<UnicodeTextRenderer> UnicodeTextRenderer::create(SDL_Renderer* renderer,
    const std::filesystem::path& font_directory) {
    // The complete, explicitly ordered bundle is required. Never consult a
    // host font, because that would make a locale's glyph coverage dependent
    // on the workstation rather than on the reviewed package bytes.
    constexpr std::array<std::string_view, 6> names{{
        "NotoSans-Regular.ttf", "NotoSansArabic-Regular.ttf",
        "NotoSansDevanagari-Regular.ttf", "NotoSansJP-Regular.otf",
        "NotoSansKR-Regular.otf", "NotoSansSC-Regular.otf",
    }};
    if (!renderer) return {};
    for (const auto name : names) {
        if (!std::filesystem::is_regular_file(font_directory / name)) return {};
    }
    if (!TTF_Init()) return {};
    auto* engine = TTF_CreateRendererTextEngine(renderer);
    auto* font = engine ? TTF_OpenFont((font_directory / names.front()).string().c_str(), 18.0F) : nullptr;
    if (!engine || !font) {
        if (font) TTF_CloseFont(font);
        if (engine) TTF_DestroyRendererTextEngine(engine);
        TTF_Quit();
        return {};
    }
    std::vector<void*> fallback_fonts;
    for (std::size_t index = 1; index < names.size(); ++index) {
        auto* fallback = TTF_OpenFont((font_directory / names[index]).string().c_str(), 18.0F);
        if (!fallback || !TTF_AddFallbackFont(font, fallback)) {
            if (fallback) TTF_CloseFont(fallback);
            for (auto* opened : fallback_fonts) TTF_CloseFont(static_cast<TTF_Font*>(opened));
            TTF_CloseFont(font);
            TTF_DestroyRendererTextEngine(engine);
            TTF_Quit();
            return {};
        }
        fallback_fonts.push_back(fallback);
    }
    return std::unique_ptr<UnicodeTextRenderer>(
        new UnicodeTextRenderer(engine, font, std::move(fallback_fonts)));
}

bool UnicodeTextRenderer::draw(float x, float y, std::string_view utf8) const {
    if (!engine_ || !font_ || utf8.empty()) return utf8.empty();
    auto* text = TTF_CreateText(static_cast<TTF_TextEngine*>(engine_),
        static_cast<TTF_Font*>(font_), utf8.data(), utf8.size());
    if (!text) return false;
    // Arabic launcher strings need an explicit right-to-left shaping
    // direction: the base font is Latin and would otherwise default to LTR.
    // HarfBuzz remains responsible for glyph selection and joining.
    const auto first_arabic = utf8.find_first_of("\xD8\xD9\xDA\xDB") != std::string_view::npos;
    if (first_arabic) TTF_SetTextDirection(text, TTF_DIRECTION_RTL);
    // Devanagari's OpenType shaping requires the script tag when the base
    // font is Noto Sans rather than its Devanagari fallback.
    const auto devanagari = utf8.find("\xE0\xA4") != std::string_view::npos
        || utf8.find("\xE0\xA5") != std::string_view::npos;
    if (devanagari) TTF_SetTextScript(text, TTF_StringToTag("Deva"));
    const bool drawn = TTF_DrawRendererText(text, x, y);
    TTF_DestroyText(text);
    return drawn;
}

} // namespace eon
