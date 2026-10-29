#include "engine/millennium_dos_title_p00_recompilation.hpp"

#include "data/millennium_dos_bitmap.hpp"
#include "data/millennium_dos_lib.hpp"
#include "data/sha256.hpp"

#include <stdexcept>
#include <string_view>

namespace eon {
namespace {

constexpr std::string_view titles_sha256 =
    "3cc57f2b12a0da44dd43220f44f06a05b9e3f009bcf008b7bb87622a5988cbe6";
constexpr std::string_view title_library_sha256 =
    "6bc6484fbea66a8e4eaf61b53d7eeab62a358b2c76a40897cca9f80c861b7678";

// Runtime addresses include the COM load bias $0100.
constexpr std::size_t load_bias = 0x100;
constexpr std::size_t p00_caller_begin = 0x1625;
constexpr std::size_t p00_caller_end_inclusive = 0x1640;
constexpr std::size_t codec_begin = 0x1390;
constexpr std::size_t codec_end_inclusive = 0x1488;

[[nodiscard]] bool matches_span(const std::span<const std::uint8_t> image,
    const std::size_t runtime_begin, const std::size_t runtime_end_inclusive,
    const std::string_view expected_sha256) {
    if (runtime_begin < load_bias || runtime_end_inclusive < runtime_begin) return false;
    const auto begin = runtime_begin - load_bias;
    const auto size = runtime_end_inclusive - runtime_begin + 1U;
    return begin <= image.size() && size <= image.size() - begin
        && to_hex(sha256(image.subspan(begin, size))) == expected_sha256;
}

} // namespace

NativeRuntimeEffectBatch make_millennium_dos_title_p00_recompilation_batch(
    const std::span<const std::uint8_t> titles_executable,
    const MillenniumDosLib& title_library, const std::uint16_t destination_segment) {
    if (destination_segment == 0 || to_hex(sha256(titles_executable)) != titles_sha256
        || !matches_span(titles_executable, p00_caller_begin, p00_caller_end_inclusive,
            "bc0648c4e1c84b33d8b556c619d8c59edbc576f5a858c45094920cbf59e50482")
        || !matches_span(titles_executable, codec_begin, codec_end_inclusive,
            "37d2116638e1ac3aadbe51d28a32032ecf443441869095b1e6c7a93b913f267a")
        || title_library.source_sha256() != title_library_sha256) {
        throw std::runtime_error("Unsupported Millennium DOS P00 recompilation profile");
    }
    const auto* p00 = title_library.find("P00");
    if (!p00 || p00 != &title_library.entries().front() || p00->offset != 6
        || p00->size != 10'555) {
        throw std::runtime_error("Millennium DOS P00 recompilation source is detached");
    }
    const auto bitmap = decode_millennium_dos_bitmap(title_library.read(*p00));
    if (bitmap.flags != 7 || bitmap.codec != 2 || bitmap.width != 320 || bitmap.height != 200
        || bitmap.pixels.size() != 64'000
        || to_hex(sha256(bitmap.pixels))
            != "85ec11c9f943672df2ba2a4e2837ce1f3158d61648ec07bcdc84b381bd24f4ee") {
        throw std::runtime_error("Millennium DOS P00 recompilation output is unsupported");
    }

    NativeRuntimeEffectBatch batch{"millennium-dos-title-p00-recompilation", true, {}};
    batch.effects.reserve(bitmap.pixels.size());
    for (const auto pixel : bitmap.pixels) {
        batch.effects.push_back({batch.effects.size() + 1U,
            {NativeRuntimeAddressSpace::dos_segmented, destination_segment,
                batch.effects.size()}, MemoryTransferElementWidth::byte,
            NativeRuntimeByteOrder::little_endian, pixel});
    }
    return batch;
}

} // namespace eon
