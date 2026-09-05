#include "data/millennium_dos_sound_driver.hpp"

#include "data/sha256.hpp"

#include <algorithm>
#include <array>
#include <stdexcept>

namespace eon {
namespace {

bool has_bytes(const std::span<const std::uint8_t> bytes, const std::size_t offset,
    const std::span<const std::uint8_t> expected) {
    return offset <= bytes.size() && expected.size() <= bytes.size() - offset
        && std::equal(expected.begin(), expected.end(),
            bytes.begin() + static_cast<std::ptrdiff_t>(offset));
}

} // namespace

MillenniumDosSoundSelectionEvidence parse_millennium_dos_sound_selection(
    const std::span<const std::uint8_t> mill_com) {
    constexpr auto launcher_sha256 =
        "4edc491db60d18ba74cda380c7ce99705b262801298829b63b09932f23f8667e";
    if (mill_com.size() != 1445 || to_hex(sha256(mill_com)) != launcher_sha256) {
        throw std::runtime_error("Unsupported Millennium DOS sound-selection launcher");
    }
    constexpr auto selector = std::to_array<std::uint8_t>({
        0x33, 0xc0, 0xbe, 0x80, 0x00, 0x0e, 0x1f, 0xac, 0x3c, 0x00, 0x74, 0x20,
        0xac, 0x3c, 0x0d, 0x74, 0x1b, 0xa2, 0x03, 0x04, 0x2c, 0x30, 0x72, 0xf4,
        0x3c, 0x03, 0x73, 0xf0, 0x32, 0xe4, 0x50, 0xba, 0x03, 0x04, 0x58, 0xc3,
        0xb4, 0x0f, 0xcd, 0x10, 0xb4, 0x00, 0xcd, 0x10, 0x0e, 0x1f, 0xba, 0x07,
        0x04, 0x89, 0xd2, 0xb4, 0x09, 0xcd, 0x21, 0x0e, 0x1f, 0xba, 0xe4, 0x04,
        0xb8, 0x0a, 0x0c, 0xcd, 0x21, 0xbe, 0xe4, 0x04, 0x8a, 0x44, 0x01, 0x22,
        0xc0, 0x74, 0xd9, 0x8a, 0x44, 0x02, 0x2c, 0x30, 0x72, 0xd2, 0x3c, 0x03,
        0x73, 0xce, 0x32, 0xe4, 0x50, 0xba, 0xa2, 0x04, 0x89, 0xd2, 0xb4, 0x09,
        0xcd, 0x21, 0x58, 0xc3,
    });
    constexpr auto filenames = std::to_array<std::uint8_t>({
        's','i','b','m','.','d','r','v',0, 's','a','d','l','.','d','r','v',0,
        's','r','o','l','.','d','r','v',0, 's','s','b','l','.','d','r','v',0,
        's','c','v','x','.','d','r','v',0, 's','t','d','y','.','d','r','v',0,
    });
    constexpr auto selection_table = std::to_array<std::uint8_t>({
        0x2a, 0x06, 0x33, 0x06, 0x3c, 0x06, 0x45, 0x06, 0x4e, 0x06, 0x57, 0x06,
        0x00, 0x00,
    });
    // File offsets are loaded COM addresses minus $0100.
    if (!has_bytes(mill_com, 0x411, selector) || !has_bytes(mill_com, 0x52a, filenames)
        || !has_bytes(mill_com, 0x56e, selection_table)) {
        throw std::runtime_error("Unsupported Millennium DOS sound-selection bytes");
    }
    return {
        .launcher_sha256 = launcher_sha256,
        .selector_entry_address = 0x511,
        .selector_byte_count = selector.size(),
        .selector_sha256 = "f9e63fc4c7c590fc57abef4a0154a2399f714951c787f98d2f7d64eee86a7434",
        .prompt_address = 0x407,
        .prompt_byte_count = 155,
        .prompt_sha256 = "d84297ee58abeaa4ca09d60a533fe0b05ea4b805af46629d32c031b11700cad0",
        .filename_table_address = 0x62a,
        .filename_table_byte_count = filenames.size(),
        .filename_table_sha256 = "a5a3260fdf7a7018df0f34b0e9ba6f74a03e157f6d97cfb8f2f70407d8791185",
        .selection_table_address = 0x66e,
        .selection_table_byte_count = selection_table.size(),
        .selection_table_sha256 = "c49071bf0db7a712437ca74d2e9effe9222665f2ab154db1f5d748f540e10ef8",
        .ibm_speaker_table_slot = 0,
        .sound_blaster_table_slot = 3,
        .covox_table_slot = 4,
        .ibm_speaker_filename = "sibm.drv",
        .sound_blaster_filename = "ssbl.drv",
        .covox_filename = "scvx.drv",
        .missing_srol_table_slot = 2,
        .missing_srol_filename = "srol.drv",
    };
}

std::string extract_millennium_dos_sound_selection_prompt(
    const std::span<const std::uint8_t> mill_com,
    const MillenniumDosSoundSelectionEvidence& evidence) {
    // Reparse, rather than trusting caller-provided offsets, before exposing
    // any bytes as original text.  The COM load address is $0100.
    const auto verified = parse_millennium_dos_sound_selection(mill_com);
    if (evidence.launcher_sha256 != verified.launcher_sha256
        || evidence.prompt_address != verified.prompt_address
        || evidence.prompt_byte_count != verified.prompt_byte_count
        || evidence.prompt_sha256 != verified.prompt_sha256) {
        throw std::runtime_error("Mismatched Millennium DOS sound-selection prompt evidence");
    }
    constexpr std::size_t com_load_address = 0x100;
    const auto offset = static_cast<std::size_t>(verified.prompt_address) - com_load_address;
    if (offset > mill_com.size() || verified.prompt_byte_count > mill_com.size() - offset) {
        throw std::runtime_error("Truncated Millennium DOS sound-selection prompt");
    }
    const auto prompt = mill_com.subspan(offset, verified.prompt_byte_count);
    if (prompt.empty() || prompt.back() != '$' || to_hex(sha256(prompt)) != verified.prompt_sha256) {
        throw std::runtime_error("Unsupported Millennium DOS sound-selection prompt bytes");
    }
    return {reinterpret_cast<const char*>(prompt.data()), prompt.size() - 1U};
}

MillenniumDosSoundDriverLeaf admit_millennium_dos_sound_driver_leaf(
    const std::span<const std::uint8_t> bytes) {
    const auto digest = to_hex(sha256(bytes));
    if (bytes.size() == 9194
        && digest == "be5a00e0b71d893a3aeaaa1127b1e5b870fe734dc876e636c6a933b6444f1b72") {
        return {MillenniumDosSoundDriverKind::sound_blaster, "ssbl.drv", digest, bytes.size()};
    }
    if (bytes.size() == 4053
        && digest == "99e110b91534206a6b83680a3e11cceadd0e5ddf863560aed53dcbd2c49df7c4") {
        return {MillenniumDosSoundDriverKind::covox_sound_master, "scvx.drv", digest, bytes.size()};
    }
    throw std::runtime_error("Unsupported Millennium DOS sound-driver leaf");
}

} // namespace eon
