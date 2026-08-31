#include "presentation/modern_presentation_pipeline.hpp"

#include <exception>

namespace eon {

bool ModernPresentationPipeline::matches(const ModernReconstructionCacheKey& key) const {
    return attempted_key_ && *attempted_key_ == key;
}

const ModernReconstructedSurface* ModernPresentationPipeline::resolve(
    const ModernReconstructionCacheKey& key, const std::span<const std::uint8_t> rgba,
    const int width, const int height) {
    if (!matches(key)) {
        // A changed original decoded source, release or renderer control can
        // never keep a prior Modern result alive. Record the attempted key so
        // a malformed source does not cause unbounded retry work every frame.
        attempted_key_ = key;
        surface_.reset();
        failure_.reset();
        try {
            surface_ = key.reconstruction == ModernPixelReconstruction::scale4x
                ? reconstruct_rgba_scale4x(rgba, width, height)
                : reconstruct_rgba_scale2x(rgba, width, height);
        } catch (const std::exception& error) {
            failure_ = error.what();
        }
    }
    return surface_ ? &*surface_ : nullptr;
}

void ModernPresentationPipeline::reset() {
    attempted_key_.reset();
    surface_.reset();
    failure_.reset();
}

} // namespace eon
