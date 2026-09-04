#include "engine/millennium_dos_title_child_compatibility_service.hpp"

#include "data/sha256.hpp"

#include <limits>
#include <stdexcept>

namespace eon {
namespace {
constexpr auto titles_sha =
    "3cc57f2b12a0da44dd43220f44f06a05b9e3f009bcf008b7bb87622a5988cbe6";
constexpr std::size_t titles_size = 7022;
}

MillenniumDosTitleChildCompatibilityService::
MillenniumDosTitleChildCompatibilityService(
    const std::span<const std::uint8_t> titles_executable,
    const MillenniumDosParagraphAllocation allocation)
    : allocation_(allocation) {
    if (titles_executable.size() != titles_size
        || to_hex(sha256(titles_executable)) != titles_sha
        || allocation.generation == 0 || allocation.allocation_id == 0
        || allocation.segment == 0
        || allocation.paragraph_count < required_paragraphs()
        || allocation.provenance
            != MillenniumDosParagraphProvenance::native_compatibility_arena
        || titles_executable.size()
            > std::numeric_limits<std::uint16_t>::max() - 0x0100U) {
        throw std::runtime_error("Unsupported Millennium DOS title child image allocation");
    }
    effects_.reserve(titles_executable.size());
    for (std::size_t i=0;i<titles_executable.size();++i) {
        effects_.push_back({static_cast<std::uint16_t>(0x0100U+i),
            titles_executable[i]});
    }
}

MillenniumDosTitleChildCompatibilityCheckpoint
MillenniumDosTitleChildCompatibilityService::checkpoint() const {
    return {allocation_,effects_.size(),0x0100,0x0100,0x1b80,true,
        false,false,false};
}

} // namespace eon
