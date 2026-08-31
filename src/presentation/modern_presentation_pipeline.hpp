#pragma once

#include "data/modern_pixel_reconstruction.hpp"

#include <optional>
#include <span>
#include <string>

namespace eon {

// Owns the CPU-side, transient result of one Modern pixel reconstruction.
// This deliberately has no SDL, filesystem, media-reader, save, input, or
// simulation API. The caller supplies already decoded original pixels and
// may upload the returned surface to an SDL texture for the current process
// only. A key mismatch revokes the old derived surface before any new one can
// be returned; a malformed source fails closed for that exact request.
class ModernPresentationPipeline {
public:
    [[nodiscard]] bool matches(const ModernReconstructionCacheKey& key) const;
    [[nodiscard]] const ModernReconstructedSurface* resolve(
        const ModernReconstructionCacheKey& key, std::span<const std::uint8_t> rgba,
        int width, int height);
    void reset();

    [[nodiscard]] const std::optional<ModernReconstructionCacheKey>& attempted_key() const {
        return attempted_key_;
    }
    [[nodiscard]] const std::optional<std::string>& failure() const { return failure_; }

private:
    std::optional<ModernReconstructionCacheKey> attempted_key_;
    std::optional<ModernReconstructedSurface> surface_;
    std::optional<std::string> failure_;
};

} // namespace eon
