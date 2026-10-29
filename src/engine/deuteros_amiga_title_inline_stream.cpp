#include "engine/deuteros_amiga_title_inline_stream.hpp"

#include "data/sha256.hpp"

#include <array>
#include <string>

namespace eon {
namespace {
constexpr std::string_view release_sha256 =
    "f4dc8dd1c27c5d389837783becd9b95ab09b78baf40e94e39e2b7e590e470e04";
constexpr std::string_view stage_sha256 =
    "48d65260e9b5f5cbf8d8b3675a178c81b8764810b61a6a2539a56dcb40a8de03";
constexpr std::uint32_t stage_runtime = 0x13000;
constexpr std::uint32_t stream_address = 0x4079a;
constexpr std::uint32_t return_address = 0x4079a;
constexpr std::uint32_t next_address = 0x407a2;
constexpr std::uint32_t entry_address = 0x1f9a4;
constexpr std::uint32_t stack_data_address = 0x1f974;
constexpr std::uint32_t high_table_pointer_address = 0x1f96c;
constexpr std::uint32_t low_table_pointer_address = 0x1f970;

std::uint32_t read_be32(const std::span<const std::uint8_t> bytes) {
    return (std::uint32_t{bytes[0]} << 24U) | (std::uint32_t{bytes[1]} << 16U)
        | (std::uint32_t{bytes[2]} << 8U) | std::uint32_t{bytes[3]};
}

NativeRuntimeLocation linear(const std::uint32_t address) {
    return {NativeRuntimeAddressSpace::linear, std::nullopt, address};
}

bool has_hash(const NativeRuntimeMemory& memory, const std::uint32_t address,
    const std::size_t size, const std::string_view expected) {
    const auto bytes = memory.read_linear_range(address, size);
    return bytes && to_hex(sha256(*bytes)) == expected;
}

} // namespace

std::optional<DeuterosAmigaTitleInlineStreamAdvance>
advance_deuteros_amiga_title_inline_stream(
    const NativeRuntimeMemory& memory,
    const std::span<const std::uint8_t> original_title_stage,
    const DeuterosAmigaTitleInlineStreamBoundary& boundary) {
    constexpr std::array<std::uint8_t, 8> expected_stream{
        0x16, 0x00, 0x0a, 0x10, 0x0f, 0x11, 0x00, 0x00};
    if (boundary.sequence == 0 || boundary.release_sha256 != release_sha256
        || boundary.program_counter != entry_address
        || boundary.return_address != return_address
        || boundary.stack_pointer > 0xfffffffaU
        || original_title_stage.size() != 0x6ca00U
        || to_hex(sha256(original_title_stage)) != stage_sha256) {
        return std::nullopt;
    }

    const auto source_offset = [](const std::uint32_t address) {
        return static_cast<std::size_t>(address - stage_runtime);
    };
    const auto code_matches = [&](const std::uint32_t address, const std::size_t size,
        const std::string_view hash) {
        if (address < stage_runtime || address - stage_runtime > original_title_stage.size()
            || size > original_title_stage.size() - (address - stage_runtime)) {
            return false;
        }
        const auto source = original_title_stage.subspan(source_offset(address), size);
        return to_hex(sha256(source)) == hash && has_hash(memory, address, size, hash);
    };

    // These spans are the only dispatcher branches and helpers reached by the
    // literal $16/$10/$11/zero stream. Unrelated command handlers are excluded.
    if (!code_matches(0x40794, 6,
            "1a076e7dd8072e69efd029cb629730993ed19cb02a1664ecdaf2a69aa7240872")
        || !code_matches(stream_address, expected_stream.size(),
            "013fd77090f8c910da3528ee0a3c9d0405317a8a24467134308f578c6c156609")
        || !code_matches(entry_address, 10,
            "47c80d7f0b7e2c4c53f91535f4ce6e090af48b681db25bfc9209676ce78bceed")
        || !code_matches(0x1fa06, 0x3a,
            "df9b8ad2079a424201b5270a8c3fee0387a4a9d3760f2c51106564c67bdc687f")
        || !code_matches(0x1fb00, 0x3a,
            "617e05e145ee984b224a3e2c58bcc7dfaa0cf66b91dfff98ecda2d7aca94a660")
        || !code_matches(0x1fb3a, 0x2e,
            "a06992ae43c41babc0fbfa06962be8506e75798e4b0e524bfa4c2ad967c65e7b")
        || !code_matches(0x1f9b8, 0x1a,
            "d15a4fe158160fd3aee3a439755fa108a403d09bd0d92c72ce0fea3505c1f3e5")
        || !code_matches(0x1f9d2, 0x1a,
            "f488d16abd19300d49deeb62d7d4227c0bdbb1b0d40b7bc305383414f1d195d3")) {
        return std::nullopt;
    }
    const auto stack_return = memory.read_linear_range(boundary.stack_pointer, 4);
    if (!stack_return || read_be32(*stack_return) != boundary.return_address)
        return std::nullopt;
    const auto saved_caller_word = memory.read_linear_range(boundary.stack_pointer + 4U, 2);
    const auto control_word = memory.read_linear_range(0x407e6, 2);
    if (!saved_caller_word || !control_word || *saved_caller_word != *control_word)
        return std::nullopt;

    const auto flag = memory.read_linear_range(0x1f98e, 1);
    const auto base = memory.read_linear_range(0x1f168, 4);
    if (!flag || !base || (*flag)[0] != 0) {
        return std::nullopt;
    }

    // $16 consumes bytes 00/0a and computes a text/data pointer from the
    // owned $1f168 base. The proven current dispatcher gate is the zero arm.
    const auto offset = std::uint32_t{0x0aU * 4U * 0x28U};
    const auto pointer = (read_be32(*base) & 0xffff0000U)
        | static_cast<std::uint16_t>((read_be32(*base) + offset) & 0xffffU);
    // $10 consumes 0f and points at row 15 of the eight-byte table; $11
    // consumes zero and points at its first row. The helpers only store these
    // addresses; they do not read the rows in this command sequence.
    const std::array<std::pair<std::uint32_t, std::uint32_t>, 3> writes{{
        {stack_data_address, pointer},
        {high_table_pointer_address, 0x1f8ecU + 0x0fU * 8U},
        {low_table_pointer_address, 0x1f8ecU},
    }};

    DeuterosAmigaTitleInlineStreamAdvance result{
        boundary.sequence, next_address, boundary.stack_pointer + 4U, next_address, {}};
    result.ordered_batches.reserve(writes.size());
    for (std::size_t index = 0; index < writes.size(); ++index) {
        result.ordered_batches.push_back({
            "deuteros-title-inline-stream-" + std::to_string(boundary.sequence)
                + "-" + std::to_string(index + 1U),
            true,
            {{1, linear(writes[index].first), MemoryTransferElementWidth::longword,
                NativeRuntimeByteOrder::big_endian, writes[index].second}},
        });
    }
    return result;
}

} // namespace eon
