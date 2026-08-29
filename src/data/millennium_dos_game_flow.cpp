#include "data/millennium_dos_game_flow.hpp"

#include "data/sha256.hpp"

#include <algorithm>
#include <array>
#include <stdexcept>
#include <string_view>

namespace eon {
namespace {

bool has_bytes(std::span<const std::uint8_t> bytes, std::size_t offset,
               std::span<const std::uint8_t> expected) {
    return offset <= bytes.size() && expected.size() <= bytes.size() - offset
        && std::equal(expected.begin(), expected.end(), bytes.begin()
            + static_cast<std::ptrdiff_t>(offset));
}

// 8086 near CALL displacements are signed 16-bit values added to the next
// instruction pointer.  The resulting IP wraps at 64 KiB; do not promote it
// to a made-up flat address while documenting a COM image.
std::uint16_t near_call_target(const std::uint16_t next_ip,
    const std::uint8_t low, const std::uint8_t high) {
    const auto encoded = static_cast<std::uint16_t>(low)
        | (static_cast<std::uint16_t>(high) << 8U);
    const auto displacement = static_cast<std::int16_t>(encoded);
    return static_cast<std::uint16_t>(static_cast<std::int32_t>(next_ip) + displacement);
}

} // namespace

MillenniumDosGameFlow parse_millennium_dos_game_flow(
    const std::span<const std::uint8_t> game_executable) {
    // 2200AD.EXE is a flat COM-style image loaded at 0x100.  Its startup
    // reaches this loop after initialization; it repeatedly calls 0x0f05,
    // tests AL, treats 0x0b/0x0c separately, then converts 0x3b..0x44 to a
    // zero-based eight-byte dispatch-table index passed to 0x76f0.
    constexpr std::size_t load_bias = 0x100;
    constexpr std::size_t entry_offset = 0xd2b0 - load_bias;
    constexpr std::size_t startup_offset = 0xd2b4 - load_bias;
    constexpr std::size_t startup_first_call_offset = 0x0124 - load_bias;
    constexpr std::size_t startup_equal_path_offset = 0xd1a1 - load_bias;
    constexpr std::size_t startup_other_path_offset = 0xd1b5 - load_bias;
    constexpr std::size_t startup_equal_followup_offset = 0x044e - load_bias;
    constexpr std::size_t startup_other_followup_offset = 0x0466 - load_bias;
    constexpr std::size_t loop_offset = 0xd3d2 - load_bias;
    constexpr std::size_t action_poll_offset = 0x0f05 - load_bias;
    constexpr std::size_t f1_table_offset = 0x2fbf - load_bias;
    constexpr std::size_t f1_handler_offset = 0x6f9a - load_bias;
    constexpr std::size_t f1_setup_offset = 0x771d - load_bias;
    constexpr std::size_t f2_table_offset = 0x2fc7 - load_bias;
    constexpr std::size_t f2_handler_offset = 0x71ca - load_bias;
    constexpr std::size_t f2_setup_offset = 0x71de - load_bias;
    constexpr std::size_t f3_table_offset = 0x2fcf - load_bias;
    constexpr std::size_t f3_handler_offset = 0x6faa - load_bias;
    constexpr std::size_t f3_setup_offset = 0x6fc6 - load_bias;
    constexpr std::size_t f4_table_offset = 0x2fd7 - load_bias;
    constexpr std::size_t f4_handler_offset = 0x72f9 - load_bias;
    constexpr std::size_t f4_common_offset = 0xba5e - load_bias;
    constexpr std::size_t f4_guard_clear_context_offset = 0xa553 - load_bias;
    constexpr std::size_t f5_table_offset = 0x2fdf - load_bias;
    constexpr std::size_t f5_handler_offset = 0x7597 - load_bias;
    constexpr std::size_t f6_table_offset = 0x2fe7 - load_bias;
    constexpr std::size_t f6_handler_offset = 0x7415 - load_bias;
    constexpr std::size_t f6_restoration_offset = 0x7455 - load_bias;
    constexpr std::size_t f7_table_offset = 0x2fef - load_bias;
    constexpr std::size_t f7_handler_offset = 0x7521 - load_bias;
    constexpr std::size_t f8_table_offset = 0x2ff7 - load_bias;
    constexpr std::size_t f8_handler_offset = 0x7306 - load_bias;
    constexpr std::size_t f8_preflight_offset = 0x731a - load_bias;
    constexpr std::size_t f9_table_offset = 0x2fff - load_bias;
    constexpr std::size_t f9_handler_offset = 0x7339 - load_bias;
    constexpr std::size_t f10_table_offset = 0x3007 - load_bias;
    constexpr std::size_t f10_handler_offset = 0x7384 - load_bias;
    constexpr std::size_t record_pointer_offset = 0x27c4 - load_bias;
    constexpr std::size_t initial_record_offset = 0x12cc - load_bias;
    constexpr std::size_t initial_record_flag_offset = 0x12f0 - load_bias;
    constexpr std::array<std::uint8_t, 15> entry{
        0x0e, 0x1f, 0x0e, 0x07, 0x8c, 0xc8, 0x8e, 0xd0,
        0xb8, 0x00, 0xda, 0x89, 0xc4, 0xb8, 0x1f};
    // The post-entry block establishes SS=CS and SP=$da00, invokes the
    // original $0124 routine (the 16-bit IP wraps), then routes an AL==1 result to $d1a1 and all
    // other results to $d1b5.  A later DX test has a static nonzero edge to
    // $d44b.  This records raw reachability boundaries only: every call
    // between these instructions remains native and unexecuted.
    constexpr auto startup = std::to_array<std::uint8_t>({
        0x8c, 0xc8, 0x8e, 0xd0, 0xb8, 0x00, 0xda, 0x89, 0xc4,
        0xb8, 0x1f, 0x00, 0x0e, 0x07, 0xbb, 0x9e, 0xd1, 0xe8,
        0x5c, 0x2e, 0x2e, 0xa3, 0x28, 0xd1, 0x88, 0xe0, 0x2e, 0xa2,
        0x68, 0x43, 0xa2, 0x05, 0xda, 0x89, 0x26, 0x2c, 0xd1,
        0x3c, 0x01, 0x75, 0x05, 0xe8, 0xc1, 0xfe, 0xeb, 0x03,
        0xe8, 0xd0, 0xfe, 0x52, 0x0e, 0x1f, 0xe8, 0x0f, 0xff,
        0xa3, 0x28, 0xd1, 0x23, 0xd2, 0x74, 0x03, 0xe9, 0x56,
        0x01});
    // The wrapped first target saves the five original register values around
    // private INT $91 and ends in RET.  Its interrupt effect and whether it
    // returns at runtime remain deliberately unmodelled.
    constexpr auto startup_first_call = std::to_array<std::uint8_t>({
        0x1e, 0x56, 0x57, 0x55, 0x06, 0xcd, 0x91,
        0x07, 0x5d, 0x5f, 0x5e, 0x1f, 0xc3});
    // The two calls selected at $d2dd/$d2e2 use distinct small paths. Both
    // independently prepare the same $0124 private-interrupt wrapper, then
    // make a different immediate follow-up call if that wrapper returns.
    // Their native call effects and return behaviour are deliberately not
    // interpreted here.
    constexpr auto startup_equal_path = std::to_array<std::uint8_t>({
        0xb8, 0x04, 0x00, 0x0e, 0x07, 0xbb, 0x9f, 0xd1,
        0xe8, 0x78, 0x2f, 0xe8, 0x9f, 0x32, 0xb0, 0x01,
        0xa2, 0x05, 0xda, 0xc3});
    constexpr auto startup_other_path = std::to_array<std::uint8_t>({
        0xb8, 0x04, 0x00, 0x0e, 0x07, 0xbb, 0x9f, 0xd1,
        0xe8, 0x64, 0x2f, 0xe8, 0xa3, 0x32, 0xa0, 0x05,
        0xda, 0x3c, 0x02, 0x75, 0x06, 0xb8, 0x00, 0xb8,
        0xa3, 0x07, 0x01, 0xc3});
    // The equal path's follow-up writes a literal one then returns. The
    // other path reaches a BIOS interrupt after a fixed 16-byte in-image
    // table and local register setup. INT $10 is the first external boundary
    // in that path; neither its behavior nor the loop is executed here.
    constexpr auto startup_equal_followup = std::to_array<std::uint8_t>({
        0xb0, 0x01, 0x2e, 0x88, 0x06, 0x05, 0xda, 0xc3});
    constexpr auto startup_other_followup = std::to_array<std::uint8_t>({
        0x0e, 0x1f, 0xbe, 0x56, 0x04, 0xb9, 0x10, 0x00,
        0x32, 0xdb, 0xac, 0x8a, 0xf8, 0xb8, 0x00, 0x10,
        0xcd, 0x10, 0xfe, 0xc3, 0xe2, 0xf4, 0xc3});
    constexpr auto startup_other_followup_table = std::to_array<std::uint8_t>({
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
        0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f});
    constexpr auto loop = std::to_array<std::uint8_t>({
        0xe8, 0xe6, 0x3a, 0xe8, 0x29, 0xa2, 0xe8, 0xf0, 0xa7,
        0xe8, 0x27, 0x3b, 0x22, 0xc0, 0x74, 0xf0, 0x32, 0xe4,
        0x3c, 0x0b, 0x74,
        0x26, 0x8a, 0x0e, 0x3a, 0xda, 0x22, 0xc9, 0x75, 0xe2,
        0x3c, 0x0c, 0x75, 0x05, 0xe8, 0x79, 0x01, 0x33, 0xc0,
        0x2c, 0x3b, 0x3c, 0x0a, 0x73, 0xd3, 0xbe, 0xbf, 0x2f,
        0x32, 0xe4, 0xc0, 0xe0, 0x03, 0x01, 0xc6, 0xe8, 0xe4, 0xa2});
    constexpr auto action_poll = std::to_array<std::uint8_t>({
        0xb4, 0x06, 0xb2, 0xff, 0xcd, 0x21, 0xc3});
    // Table record 0 contains its non-semantic rectangle followed by the
    // handler entry. The F1 handler clears AX, calls the common display
    // selector at $d0c9, then calls the setup block below. That setup has an
    // unconditional pre-call prefix, but it is reached only if $d0c9 returns;
    // we therefore record it as code evidence rather than a host overlay.
    // It selects $12cc through the original word table at $27c4, retains the
    // word at $da20, and stores mode $07 / descriptor $300f before calling
    // $5b1f.
    constexpr auto f1_table = std::to_array<std::uint8_t>({
        0x00, 0x06, 0x09, 0x1b, 0x30, 0x00, 0x9a, 0x6f});
    constexpr auto f1_handler = std::to_array<std::uint8_t>({
        0x33, 0xc0, 0xe8, 0x2a, 0x61, 0xe8, 0x7b, 0x07,
        0xe8, 0x55, 0x9a, 0xd0, 0xeb, 0x72, 0xf9, 0xc3});
    constexpr auto f1_setup = std::to_array<std::uint8_t>({
        0xb8, 0xcc, 0x12, 0xc6, 0x06, 0x1f, 0xda, 0x00,
        0xa3, 0x20, 0xda, 0xb9, 0x0f, 0x30, 0xa0, 0x1f,
        0xda, 0x22, 0xc0, 0xb0, 0x07, 0x74, 0x05, 0xb0,
        0x05, 0xb9, 0x47, 0x30, 0xa2, 0xa8, 0x75, 0x89,
        0x0e, 0xa6, 0x75, 0xb8, 0x01, 0x00, 0x80, 0x3e,
        0xa8, 0x75, 0x07, 0x74, 0x03, 0xb8, 0x6d, 0x00,
        0xe8, 0xcf, 0xe3});
    constexpr auto record_pointer_table = std::to_array<std::uint8_t>({
        0xcc, 0x12, 0x84, 0x13, 0x44, 0x14, 0x04, 0x15});
    constexpr auto initial_record = std::to_array<std::uint8_t>({
        0x03, 0x00, 0x11, 0x00, 0x00, 0x00, 0x00, 0x00});
    constexpr auto initial_record_flag = std::to_array<std::uint8_t>({0x00});
    // Record one (raw F2 / $3c) enters $71ca. It reads a runtime byte at
    // $da26, waits in the original loop if it is below two, otherwise enters
    // $71de. That admitted path creates a $c0-stride list beginning at
    // $1384 and installs callback $7221. No host-side value is supplied for
    // the runtime byte.
    constexpr auto f2_table = std::to_array<std::uint8_t>({
        0x06, 0x0c, 0x09, 0x1b, 0x31, 0x01, 0xca, 0x71});
    constexpr auto f2_handler = std::to_array<std::uint8_t>({
        0xb0, 0x02, 0xa0, 0x26, 0xda, 0x3c, 0x02, 0x72, 0x03,
        0xe9, 0x08, 0x00, 0xe8, 0x21, 0x98, 0xd0, 0xeb, 0x72,
        0xf9, 0xc3});
    constexpr auto f2_setup = std::to_array<std::uint8_t>({
        0xb8, 0x21, 0x72, 0xa3, 0x98, 0x6f, 0xc6, 0x06, 0x98,
        0x6e, 0x01, 0xb8, 0x18, 0x00, 0xe8, 0x3d, 0xdb, 0xb8,
        0x19, 0x00, 0xe8, 0x41, 0xdb, 0xb8, 0x84, 0x13, 0x8a,
        0x0e, 0x26, 0xda, 0x32, 0xed, 0x49, 0x88, 0x0e, 0x95,
        0x6e, 0xbf, 0x99, 0x6e, 0x0e, 0x07, 0xab, 0x05, 0xc0,
        0x00, 0xe2, 0xfa, 0x0e, 0x1f, 0xc6, 0x06, 0x93, 0x6e,
        0xff, 0xe8, 0x9d, 0x00, 0xc6, 0x06, 0x1e, 0xda, 0x08,
        0xe8, 0x56, 0x99, 0xc3});
    constexpr auto f3_table = std::to_array<std::uint8_t>({
        0x0c, 0x12, 0x09, 0x1b, 0x32, 0x02, 0xaa, 0x6f});
    // F3 returns when $a19e is nonzero, waits at $09fa while $da27 is zero,
    // and only then reaches this setup. The two runtime values are not known.
    constexpr auto f3_handler = std::to_array<std::uint8_t>({
        0xa1, 0x9e, 0xa1, 0x23, 0xc0, 0x74, 0x01, 0xc3,
        0xb0, 0x02, 0xa1, 0x27, 0xda, 0x22, 0xc0, 0x74,
        0x03, 0xe9, 0x08, 0x00, 0xe8, 0x39, 0x9a, 0xd0,
        0xeb, 0x72, 0xf9, 0xc3});
    constexpr auto f3_setup = std::to_array<std::uint8_t>({
        0xb8, 0x2a, 0x71, 0xa3, 0x98, 0x6f, 0xc6, 0x06,
        0x98, 0x6e, 0x00, 0xb8, 0x16, 0x00, 0xe8, 0x55,
        0xdd, 0xb8, 0x17, 0x00, 0xe8, 0x59, 0xdd, 0xb8,
        0x00, 0x00, 0xba, 0x00, 0x00, 0x8b, 0x0e, 0x27,
        0xda, 0x88, 0x0e, 0x95, 0x6e, 0xbb, 0x99, 0x6e,
        0xc5, 0x36, 0x12, 0x01});
    // Record three (raw F4 / $3e) only admits when $a19e is zero.  It loads
    // AL=$02 and transfers to $ba5e. That routine calls $4d2c, writes $07 to
    // $da13, calls $9dd5, writes $09 to $da1e and clears $75a9. There is no
    // pre-call write: each literal write depends on the preceding native call
    // returning. The call effects and all three cells are native runtime facts,
    // not host state, so this is deliberately not an overlay effect.
    constexpr auto f4_table = std::to_array<std::uint8_t>({
        0x12, 0x18, 0x09, 0x1b, 0x33, 0x03, 0xf9, 0x72});
    constexpr auto f4_handler = std::to_array<std::uint8_t>({
        0xa1, 0x9e, 0xa1, 0x23, 0xc0, 0x74, 0x01, 0xc3,
        0xb0, 0x02, 0xe9, 0x58, 0x47});
    constexpr auto f4_common = std::to_array<std::uint8_t>({
        0xb8, 0x05, 0x00, 0xe8, 0xc8, 0x92, 0xc6, 0x06,
        0x13, 0xda, 0x07, 0xe8, 0x69, 0xe3, 0xc6, 0x06,
        0x1e, 0xda, 0x09, 0xc6, 0x06, 0xa9, 0x75, 0x00, 0xc3});
    constexpr auto f4_guard_clear = std::to_array<std::uint8_t>({
        0x8b, 0x0e, 0x9e, 0xa1, 0xc7, 0x06, 0x9e, 0xa1, 0x00, 0x00,
        0x32, 0xed});
    // Record four (raw F5 / $3f) enters $7597.  It has no store before the
    // first CALL.  That target immediately calls $52f9, so even the first
    // possible post-call state depends on native execution and return behavior.
    // The remaining F5 calls are ordinary 16-bit near targets: do not turn
    // their signed displacements into fictional addresses above 64 KiB.
    constexpr auto f5_table = std::to_array<std::uint8_t>({
        0x18, 0x1e, 0x09, 0x1b, 0x34, 0x04, 0x97, 0x75});
    constexpr auto f5_handler = std::to_array<std::uint8_t>({
        0xb0, 0x02, 0xe8, 0x8c, 0x48, 0xe8, 0xfe, 0x95,
        0xe8, 0x55, 0xd6, 0xe8, 0xd1, 0x95, 0xc3});
    constexpr auto f5_first_call = std::to_array<std::uint8_t>({
        0xe8, 0xce, 0x94});
    constexpr auto f5_second_call = std::to_array<std::uint8_t>({
        0x80, 0x3e, 0xf9, 0x07, 0x01, 0x75, 0x20});
    constexpr auto f5_third_call = std::to_array<std::uint8_t>({
        0x06, 0x57, 0x1e, 0x56, 0xe8, 0xd9, 0xbf});
    constexpr auto f5_fourth_call = std::to_array<std::uint8_t>({
        0x80, 0x3e, 0xf9, 0x07, 0x01, 0x75, 0x12});
    // Record five (raw F6 / $40) uses the same $a19e gate as F3/F4.  On its
    // admitted path the native image snapshots $75a8/$75ae/$75ac into its
    // own $7412/$740f/$7410 scratch cells, then installs literal temporary
    // values before calling the original $09fa polling routine.  The byte
    // following SHR BL,1 controls repetition of that poll; no host state is
    // supplied for it, and this parser deliberately does not execute it.
    constexpr auto f6_table = std::to_array<std::uint8_t>({
        0x1e, 0x24, 0x09, 0x1b, 0x35, 0x05, 0x15, 0x74});
    constexpr auto f6_handler = std::to_array<std::uint8_t>({
        0xa1, 0x9e, 0xa1, 0x23, 0xc0, 0x74, 0x01, 0xc3,
        0x33, 0xc0, 0xe8, 0xa7, 0x5c, 0xb8, 0x22, 0x00,
        0xe8, 0x04, 0xd9, 0xe8, 0x55, 0x55, 0xa0, 0xa8,
        0x75, 0xa2, 0x12, 0x74, 0xa0, 0xae, 0x75, 0xa2,
        0x0f, 0x74, 0xa1, 0xac, 0x75, 0xa3, 0x10, 0x74,
        0xc6, 0x06, 0xa8, 0x75, 0x0c, 0xc6, 0x06, 0xae,
        0x75, 0x00, 0xb8, 0x07, 0x32, 0xa3, 0xa6, 0x75,
        0xe8, 0xaa, 0x95, 0xd0, 0xeb, 0x72, 0xf9, 0xc3});
    // $7455 is a separate routine immediately following F6. It restores the
    // exact three F6 snapshots before its first CALL ($0b0c). The dispatch
    // path which reaches it is not yet proved, so this is cleanup evidence,
    // not an executed host effect.
    constexpr auto f6_restoration = std::to_array<std::uint8_t>({
        0xa0, 0x0f, 0x74, 0xa2, 0xae, 0x75,
        0xa1, 0x10, 0x74, 0xa3, 0xac, 0x75,
        0xa0, 0x12, 0x74, 0xa2, 0xa8, 0x75,
        0xe8, 0xa2, 0x96});
    // Record six (raw F7 / $41) uses the same native $a19e gate. On its
    // admitted path it reads words at $da17/$da18/$da27/$da26/$da35/$da37,
    // calls the observed helper sequence, and returns. The values and helper
    // effects are native runtime state, so this parser records no semantics.
    constexpr auto f7_table = std::to_array<std::uint8_t>({
        0x24, 0x2a, 0x09, 0x1b, 0x36, 0x06, 0x21, 0x75});
    constexpr auto f7_handler = std::to_array<std::uint8_t>({
        0xa1, 0x9e, 0xa1, 0x23, 0xc0, 0x74, 0x01, 0xc3,
        0xb0, 0x1d, 0xe8, 0xfe, 0xd7, 0xb8, 0x12, 0x06,
        0xe8, 0x08, 0x92, 0xb8, 0x2a, 0x01, 0xe8, 0x2c,
        0x91, 0xa0, 0x17, 0xda, 0x32, 0xe4, 0x05, 0xa2,
        0x01, 0x2e, 0x8b, 0x1e, 0xca, 0x05, 0xe8, 0x1c,
        0x91, 0xa1, 0x18, 0xda, 0xe8, 0x92, 0x91, 0x2e,
        0x89, 0x1e, 0xca, 0x05, 0xe8, 0x76, 0x90, 0xa1,
        0x27, 0xda, 0xe8, 0x7e, 0x91, 0xe8, 0x6d, 0x90,
        0xa0, 0x26, 0xda, 0x32, 0xe4, 0xe8, 0x73, 0x91,
        0xe8, 0x62, 0x90, 0xa1, 0x35, 0xda, 0xe8, 0x6a,
        0x91, 0xe8, 0x59, 0x90, 0xa1, 0x37, 0xda, 0x50,
        0x32, 0xe4, 0xe8, 0x5e, 0x91, 0xb0, 0x2e, 0xe8,
        0xfb, 0x91, 0x58, 0x88, 0xe0, 0x32, 0xe4, 0xe8,
        0x7f, 0x91, 0xb0, 0x25, 0xe8, 0xee, 0x91, 0xe8,
        0x0a, 0x96, 0xe8, 0x61, 0xd6, 0xc3});
    // Record seven (raw F8 / $42) enters $7306. It clears native byte $da30,
    // loads AL=$02, enters $731a, then repeatedly calls $09fa while the carry
    // result of SHR BL,1 remains set. $731a first reads $da39; when nonzero it
    // calls $7b47 and returns. Its zero path reads/decrements $da0a, applies
    // XLAT through BX=$db4b, then jumps to $7948. Neither runtime byte nor BL
    // is invented.
    constexpr auto f8_table = std::to_array<std::uint8_t>({
        0x2a, 0x30, 0x09, 0x1b, 0x37, 0x07, 0x06, 0x73});
    constexpr auto f8_handler = std::to_array<std::uint8_t>({
        0x0e, 0x1f, 0xc6, 0x06, 0x30, 0xda, 0x00, 0xb0,
        0x02, 0xe8, 0x08, 0x00, 0xe8, 0xe5, 0x96, 0xd0,
        0xeb, 0x72, 0xf9, 0xc3});
    constexpr auto f8_preflight = std::to_array<std::uint8_t>({
        0xa0, 0x39, 0xda, 0x22, 0xc0, 0x74, 0x04, 0xe8,
        0x23, 0x08, 0xc3, 0xa0, 0x0a, 0xda, 0x22, 0xc0,
        0x75, 0x01, 0xc3, 0xfe, 0xc8, 0xa2, 0x0a, 0xda,
        0xbb, 0x4b, 0xdb, 0xd7, 0xe9, 0x0f, 0x06});
    // Record eight (raw F9 / $43) admits only when $a19e is zero. It calls
    // $d0c9 with AX=0, clears $da30, sets its code-local $6e2f to one, clears
    // $dad7, and conditionally calls $7b47 if $da39 is nonzero. It then loops
    // through F8's $731a while $da06 is below nine. Its terminal call crosses
    // 64 KiB, so the trace stores it as a flat image address.
    constexpr auto f9_table = std::to_array<std::uint8_t>({
        0x30, 0x36, 0x09, 0x1b, 0x38, 0x08, 0x39, 0x73});
    constexpr auto f9_handler = std::to_array<std::uint8_t>({
        0xa1, 0x9e, 0xa1, 0x23, 0xc0, 0x74, 0x01, 0xc3,
        0x33, 0xc0, 0xe8, 0x83, 0x5d, 0xc6, 0x06, 0x30,
        0xda, 0x00, 0xb0, 0x02, 0x2e, 0xc6, 0x06, 0x2f,
        0x6e, 0x01, 0xc6, 0x06, 0xd7, 0xda, 0x00, 0xa0,
        0x39, 0xda, 0x22, 0xc0, 0x74, 0x03, 0xe8, 0xe5,
        0x07, 0xa0, 0x06, 0xda, 0x3c, 0x09, 0x72, 0x05,
        0xe8, 0xae, 0xff, 0xeb, 0xf4, 0x2e, 0xc6, 0x06,
        0x2f, 0x6e, 0x00, 0xa0, 0x09, 0xda, 0x22, 0xc0,
        0x75, 0x03, 0xe8, 0x1f, 0x07, 0xe8, 0xa3, 0xcd,
        0xe9, 0x48, 0x00});
    // Record nine (raw F10 / $44) enters $7384. It has the established
    // $a19e admission gate, clears $da30/$dad7, and sets code-local $6e2f to
    // one. If $da39 is nonzero it calls $7b47. It repeatedly calls F8's
    // $731a while $da06 is below two, resets $6e2f, conditionally calls
    // $7a9d from $da09, then reaches the observed call sequence. The final
    // wait/repetition depends on $da41 and BL/carry; no native state is
    // invented or mutated by this parser.
    constexpr auto f10_table = std::to_array<std::uint8_t>({
        0x36, 0x3c, 0x09, 0x1b, 0x39, 0x09, 0x84, 0x73});
    constexpr auto f10_handler = std::to_array<std::uint8_t>({
        0xa1, 0x9e, 0xa1, 0x23, 0xc0, 0x74, 0x01, 0xc3,
        0x33, 0xc0, 0xe8, 0x38, 0x5d, 0xc6, 0x06, 0x30,
        0xda, 0x00, 0xb0, 0x02, 0xc6, 0x06, 0xd7, 0xda,
        0x00, 0x2e, 0xc6, 0x06, 0x2f, 0x6e, 0x01, 0xa0,
        0x39, 0xda, 0x22, 0xc0, 0x74, 0x03, 0xe8, 0x9a,
        0x07, 0xa0, 0x06, 0xda, 0x3c, 0x02, 0x72, 0x05,
        0xe8, 0x63, 0xff, 0xeb, 0xf4, 0x2e, 0xc6, 0x06,
        0x2f, 0x6e, 0x00, 0xa0, 0x09, 0xda, 0x22, 0xc0,
        0x75, 0x03, 0xe8, 0xd4, 0x06, 0xe8, 0x74, 0xcd,
        0xe8, 0xfc, 0x07, 0xe8, 0xce, 0x2e, 0x2e, 0xa0,
        0x41, 0xda, 0x22, 0xc0, 0x75, 0x0d, 0xe8, 0x1d,
        0x96, 0x74, 0xed, 0xd0, 0xeb, 0x72, 0xe9, 0xe8,
        0x2b, 0xcd, 0xc3});
    if (!has_bytes(game_executable, entry_offset, entry)) {
        throw std::runtime_error("Unsupported Millennium DOS COM entry");
    }
    if (!has_bytes(game_executable, startup_offset, startup)) {
        throw std::runtime_error("Unsupported Millennium DOS startup profile");
    }
    if (!has_bytes(game_executable, startup_first_call_offset, startup_first_call)) {
        throw std::runtime_error("Unsupported Millennium DOS first startup-call boundary");
    }
    if (!has_bytes(game_executable, startup_equal_path_offset, startup_equal_path)
        || !has_bytes(game_executable, startup_other_path_offset, startup_other_path)
        || !has_bytes(game_executable, startup_equal_followup_offset, startup_equal_followup)
        || !has_bytes(game_executable, startup_other_followup_offset, startup_other_followup)
        || !has_bytes(game_executable, 0x0456 - load_bias, startup_other_followup_table)) {
        throw std::runtime_error("Unsupported Millennium DOS startup selector paths");
    }
    if (!has_bytes(game_executable, loop_offset, loop)
        || !has_bytes(game_executable, action_poll_offset, action_poll)
        || !has_bytes(game_executable, f1_table_offset, f1_table)
        || !has_bytes(game_executable, f1_handler_offset, f1_handler)
        || !has_bytes(game_executable, f1_setup_offset, f1_setup)
        || !has_bytes(game_executable, f2_table_offset, f2_table)
        || !has_bytes(game_executable, f2_handler_offset, f2_handler)
        || !has_bytes(game_executable, f2_setup_offset, f2_setup)
        || !has_bytes(game_executable, f3_table_offset, f3_table)
        || !has_bytes(game_executable, f3_handler_offset, f3_handler)
        || !has_bytes(game_executable, f3_setup_offset, f3_setup)
        || !has_bytes(game_executable, f4_table_offset, f4_table)
        || !has_bytes(game_executable, f4_handler_offset, f4_handler)
        || !has_bytes(game_executable, f4_common_offset, f4_common)
        || !has_bytes(game_executable, f4_guard_clear_context_offset, f4_guard_clear)
        || !has_bytes(game_executable, f5_table_offset, f5_table)
        || !has_bytes(game_executable, f5_handler_offset, f5_handler)
        || !has_bytes(game_executable, 0xbe28 - load_bias, f5_first_call)
        || !has_bytes(game_executable, 0x0b9d - load_bias, f5_second_call)
        || !has_bytes(game_executable, 0x4bf7 - load_bias, f5_third_call)
        || !has_bytes(game_executable, 0x0b76 - load_bias, f5_fourth_call)
        || !has_bytes(game_executable, f6_table_offset, f6_table)
        || !has_bytes(game_executable, f6_handler_offset, f6_handler)
        || !has_bytes(game_executable, f6_restoration_offset, f6_restoration)
        || !has_bytes(game_executable, f7_table_offset, f7_table)
        || !has_bytes(game_executable, f7_handler_offset, f7_handler)
        || !has_bytes(game_executable, f8_table_offset, f8_table)
        || !has_bytes(game_executable, f8_handler_offset, f8_handler)
        || !has_bytes(game_executable, f8_preflight_offset, f8_preflight)
        || !has_bytes(game_executable, f9_table_offset, f9_table)
        || !has_bytes(game_executable, f9_handler_offset, f9_handler)
        || !has_bytes(game_executable, f10_table_offset, f10_table)
        || !has_bytes(game_executable, f10_handler_offset, f10_handler)
        || !has_bytes(game_executable, record_pointer_offset, record_pointer_table)
        || !has_bytes(game_executable, initial_record_offset, initial_record)
        || !has_bytes(game_executable, initial_record_flag_offset, initial_record_flag)) {
        throw std::runtime_error("Unsupported Millennium DOS main-loop control flow");
    }
    return {
        .entry_address = 0xd2b0,
        .startup_address = 0xd2b4,
        .startup_stack_pointer = 0xda00,
        .startup_first_call_address = 0x0124,
        .startup_first_call_interrupt = 0x91,
        .startup_first_call_return_address = 0x0130,
        .startup_first_call_return_site = 0xd2c8,
        .startup_result_word_address = 0xd128,
        .startup_result_high_byte_first_address = 0x4368,
        .startup_result_high_byte_second_address = 0xda05,
        .startup_stack_snapshot_address = 0xd12c,
        .startup_mode_compare_address = 0xd2d9,
        .startup_mode_byte_address = 0xda05,
        .startup_mode_equal_value = 1,
        .startup_equal_call_address = 0xd1a1,
        .startup_other_call_address = 0xd1b5,
        .startup_equal_path_private_call_site = 0xd1a9,
        .startup_equal_path_next_call_address = 0x044e,
        .startup_equal_followup_write_address = 0xda05,
        .startup_equal_followup_write_value = 1,
        .startup_other_path_private_call_site = 0xd1bd,
        .startup_other_path_next_call_address = 0x0466,
        .startup_other_followup_table_address = 0x0456,
        .startup_other_followup_table_size = 16,
        .startup_other_followup_table_values = {
            0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
            0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f},
        .startup_other_followup_interrupt_site = 0x0476,
        .startup_other_followup_interrupt_number = 0x10,
        .startup_other_followup_video_function = 0x10,
        .startup_other_followup_video_subfunction = 0x00,
        .startup_nonzero_dx_branch_address = 0xd44b,
        .main_loop_address = 0xd3d2,
        .action_poll_address = 0x0f05,
        .special_action_0 = 0x0b,
        .special_action_1 = 0x0c,
        .function_key_first_action = 0x3b,
        .function_key_count = 10,
        .function_key_table_address = 0x2fbf,
        .function_key_table_stride = 8,
        .function_key_dispatch_address = 0x76f0,
        .first_function_key = {
            .handler_address = 0x6f9a,
            .display_selector_call_address = 0xd0c9,
            .setup_entry_address = 0x771d,
            .selector_address = 0xda1f,
            .selector_value = 0,
            .record_pointer_table_address = 0x27c4,
            .selected_record_address = 0x12cc,
            .selected_record_storage_address = 0xda20,
            .screen_descriptor_address = 0x300f,
            .screen_descriptor_mode = 7,
            .screen_selector_storage_address = 0x75a8,
            .screen_descriptor_storage_address = 0x75a6,
            .setup_first_call_address = 0x5b1f,
            .selected_record_byte_2 = 0x11,
            .selected_record_byte_36 = 0,
        },
        .second_function_key = {
            .handler_address = 0x71ca,
            .availability_address = 0xda26,
            .minimum_availability = 2,
            .wait_call_address = 0x9fa,
            .callback_slot_address = 0x6f98,
            .callback_address = 0x7221,
            .first_record_address = 0x1384,
            .record_stride = 0x00c0,
            .record_list_address = 0x6e99,
            .list_mode_address = 0x6e98,
            .list_mode_value = 1,
        },
        .third_function_key = {
            .handler_address = 0x6faa,
            .initialization_guard_address = 0xa19e,
            .availability_address = 0xda27,
            .wait_call_address = 0x09fa,
            .callback_slot_address = 0x6f98,
            .callback_address = 0x712a,
            .list_mode_address = 0x6e98,
            .list_mode_value = 0,
            .source_far_pointer_address = 0x0112,
            .list_address = 0x6e99,
        },
        .fourth_function_key = {
            .handler_address = 0x72f9,
            .initialization_guard_address = 0xa19e,
            .initialization_guard_clear_address = 0xa557,
            .transfer_al_value = 2,
            .common_routine_address = 0xba5e,
            .first_call_address = 0x4d2c,
            .first_write_instruction_address = 0xba64,
            .first_runtime_byte_address = 0xda13,
            .first_runtime_byte_value = 7,
            .second_call_address = 0x9dd5,
            .second_write_instruction_address = 0xba6c,
            .second_runtime_byte_address = 0xda1e,
            .second_runtime_byte_value = 9,
            .third_runtime_byte_address = 0x75a9,
            .third_runtime_byte_value = 0,
            .common_return_instruction_address = 0xba76,
        },
        .fifth_function_key = {
            .handler_address = 0x7597,
            .transfer_al_value = 2,
            .first_call_address = 0xbe28,
            .first_call_initial_nested_call_address = 0x52f9,
            .second_call_address = 0x0b9d,
            .second_call_mode_address = 0x07f9,
            .second_call_mode_value = 1,
            .third_call_address = 0x4bf7,
            .third_call_initial_nested_call_address = 0x0bd7,
            .fourth_call_address = 0x0b76,
        },
        .sixth_function_key = {
            .handler_address = 0x7415,
            .initialization_guard_address = 0xa19e,
            .display_selector_call_address = 0xd0c9,
            .command_value = 0x0022,
            .first_call_address = 0x4d2c,
            .second_call_address = 0xc980,
            .saved_first_byte_address = 0x7412,
            .first_byte_address = 0x75a8,
            .saved_second_byte_address = 0x740f,
            .second_byte_address = 0x75ae,
            .saved_word_address = 0x7410,
            .word_address = 0x75ac,
            .first_byte_value = 0x0c,
            .second_byte_value = 0x00,
            .callback_word_value = 0x3207,
            .callback_word_address = 0x75a6,
            .wait_call_address = 0x09fa,
            .restoration_handler_address = 0x7455,
            .restoration_first_source_address = 0x740f,
            .restoration_first_destination_address = 0x75ae,
            .restoration_word_source_address = 0x7410,
            .restoration_word_destination_address = 0x75ac,
            .restoration_second_source_address = 0x7412,
            .restoration_second_destination_address = 0x75a8,
            .restoration_first_call_address = 0x0b0c,
        },
        .seventh_function_key = {
            .handler_address = 0x7521,
            .initialization_guard_address = 0xa19e,
            .initial_al_value = 0x1d,
            .first_call_address = 0x4d2c,
            .first_command_value = 0x0612,
            .first_command_call_address = 0x073c,
            .second_command_value = 0x012a,
            .second_command_call_address = 0x0666,
            .first_runtime_word_address = 0xda17,
            .second_runtime_word_address = 0xda18,
            .third_runtime_word_address = 0xda27,
            .fourth_runtime_word_address = 0xda26,
            .fifth_runtime_word_address = 0xda35,
            .sixth_runtime_word_address = 0xda37,
            .helper_a_address = 0x06dc,
            .helper_b_address = 0x05ce,
            .helper_c_address = 0x077e,
            .literal_al_value = 0x0025,
            .terminal_call_address = 0x4bf7,
        },
        .eighth_function_key = {
            .handler_address = 0x7306,
            .reset_runtime_byte_address = 0xda30,
            .reset_runtime_byte_value = 0,
            .initial_al_value = 2,
            .local_preflight_address = 0x731a,
            .preflight_runtime_byte_address = 0xda39,
            .preflight_enabled_call_address = 0x7b47,
            .decrement_runtime_byte_address = 0xda0a,
            .depleted_jump_address = 0x7948,
            // CALL rel16 at $7312 is read from the accepted original handler
            // bytes. Its next IP is $7315, so the signed displacement resolves
            // modulo 64 KiB to $09fa rather than a fictional flat address.
            .repeated_call_address = near_call_target(0x7315, f8_handler[13], f8_handler[14]),
            .repeat_shift_register = 3,
        },
        .ninth_function_key = {
            .handler_address = 0x7339,
            .initialization_guard_address = 0xa19e,
            .display_selector_call_address = 0xd0c9,
            .first_reset_runtime_byte_address = 0xda30,
            .first_reset_runtime_byte_value = 0,
            .initial_al_value = 2,
            .local_mode_address = 0x6e2f,
            .local_mode_value = 1,
            .second_reset_runtime_byte_address = 0xdad7,
            .second_reset_runtime_byte_value = 0,
            .enabled_runtime_byte_address = 0xda39,
            .enabled_call_address = 0x7b47,
            .limit_runtime_byte_address = 0xda06,
            .limit_value = 9,
            .local_preflight_address = 0x731a,
            .terminal_call_address = 0x14124,
        },
        .tenth_function_key = {
            .handler_address = 0x7384,
            .initialization_guard_address = 0xa19e,
            .display_selector_call_address = 0xd0c9,
            .first_reset_runtime_byte_address = 0xda30,
            .first_reset_runtime_byte_value = 0,
            .initial_al_value = 2,
            .second_reset_runtime_byte_address = 0xdad7,
            .second_reset_runtime_byte_value = 0,
            .local_mode_address = 0x6e2f,
            .local_mode_value = 1,
            .enabled_runtime_byte_address = 0xda39,
            .enabled_call_address = 0x7b47,
            .limit_runtime_byte_address = 0xda06,
            .limit_value = 2,
            .local_preflight_address = 0x731a,
            .local_mode_reset_value = 0,
            .conditional_runtime_byte_address = 0xda09,
            .conditional_call_address = 0x7a9d,
            .first_terminal_call_address = 0x4140,
            .second_terminal_call_address = 0x7bcb,
            .third_terminal_call_address = 0xa2a0,
            .wait_runtime_byte_address = 0xda41,
            .wait_call_address = 0x09fa,
            .repeat_shift_register = 3,
            .final_call_address = 0x4111,
        },
    };
}

MillenniumDosStartupAllocationBoundary
parse_millennium_dos_startup_allocation_boundary(
    const std::span<const std::uint8_t> game_executable) {
    // All addresses below are COM load addresses. This continuation follows
    // one of the native selector calls at $d2dd/$d2e2 only if it returns.
    // Its first callee reaches INT $21 at $d201; retain the encoded request
    // but never invoke it or decide its result.
    constexpr std::size_t load_bias = 0x100;
    constexpr auto executable_sha256 =
        "427574e5f780b2a7b5c4207d167116dc44aea3fb67096fbf12a46c4f544a0a57";
    constexpr auto continuation = std::to_array<std::uint8_t>({
        0x52, 0x0e, 0x1f, 0xe8, 0x0f, 0xff, 0xa3, 0x28, 0xd1,
        0x23, 0xd2, 0x74, 0x03, 0xe9, 0x56, 0x01, 0xe8, 0x69, 0x3e});
    constexpr auto allocator_prefix = std::to_array<std::uint8_t>({
        0x0e, 0x07, 0xbb, 0x00, 0x10, 0xb4, 0x4a, 0xcd, 0x21});
    constexpr auto continuation_sha256 =
        "9623d493ddfa9339d3137799c133a99df86425db2fb0d674a81b2e09555692b6";
    constexpr auto allocator_prefix_sha256 =
        "11d1e2057faef11b2ebbcd56ea6e392435d75111519765894d3c839d6ba551c8";
    constexpr auto offset = [](const std::uint16_t address) {
        return static_cast<std::size_t>(address) - load_bias;
    };
    if (game_executable.size() != 54'391
        || to_hex(sha256(game_executable)) != executable_sha256
        || !has_bytes(game_executable, offset(0xd2e5), continuation)
        || !has_bytes(game_executable, offset(0xd1fa), allocator_prefix)
        || to_hex(sha256(game_executable.subspan(offset(0xd2e5), continuation.size())))
            != continuation_sha256
        || to_hex(sha256(game_executable.subspan(offset(0xd1fa), allocator_prefix.size())))
            != allocator_prefix_sha256) {
        throw std::runtime_error("Unsupported Millennium DOS startup allocation boundary");
    }
    return {std::string(executable_sha256), 0xd2e5, 0xd2e8, 0xd1fa, 0xd201,
        0x21, 0x4a, 0xd128, 0xd2ee, 0xd2f0, 0xd2f5, 0xd2f2, 0xd44b,
        0xd2f5, 0x1161, std::string(continuation_sha256),
        std::string(allocator_prefix_sha256)};
}

MillenniumDosStartupZeroPathBoundary
parse_millennium_dos_startup_zero_path_boundary(
    const std::span<const std::uint8_t> game_executable) {
    // The preceding allocation result is native state.  This parser exposes
    // only the raw DX-zero successor and stops at the first DOS boundary in
    // its second in-image callee; it does not select a mode or open a file.
    constexpr std::size_t load_bias = 0x100;
    constexpr auto executable_sha256 =
        "427574e5f780b2a7b5c4207d167116dc44aea3fb67096fbf12a46c4f544a0a57";
    constexpr auto zero_path = std::to_array<std::uint8_t>({
        0xe8, 0x69, 0x3e});
    constexpr auto selector = std::to_array<std::uint8_t>({
        0xba, 0x31, 0x11, 0xa0, 0x05, 0xda, 0x3c, 0x01, 0x74, 0x11,
        0xba, 0x3d, 0x11, 0x3c, 0x03, 0x74, 0x0a, 0xba, 0x55, 0x11,
        0x3c, 0x02, 0x74, 0x03, 0xba, 0x49, 0x11, 0xe8, 0xbb, 0xf3});
    constexpr auto security_loader_prefix = std::to_array<std::uint8_t>({
        0x1e, 0x06, 0x1e, 0x0e, 0x1f, 0xeb, 0x08, 0x1e, 0x06, 0x1e,
        0x0e, 0x1f, 0xba, 0x6a, 0x2f, 0xb0, 0x02, 0xb4, 0x3d, 0xcd,
        0x21, 0x89, 0x06, 0x00});
    constexpr auto selector_names = std::to_array<std::uint8_t>({
        0x56, 0x47, 0x41, 0x54, 0x58, 0x54, 0x2e, 0x42, 0x49, 0x4e, 0x00, 0x00,
        0x45, 0x47, 0x33, 0x54, 0x58, 0x54, 0x2e, 0x42, 0x49, 0x4e, 0x00, 0x00,
        0x45, 0x47, 0x36, 0x54, 0x58, 0x54, 0x2e, 0x42, 0x49, 0x4e, 0x00, 0x00,
        0x54, 0x44, 0x59, 0x54, 0x58, 0x54, 0x2e, 0x42, 0x49, 0x4e, 0x00, 0x00});
    constexpr auto security_name = std::to_array<std::uint8_t>({
        0x41, 0x3a, 0x5c, 0x32, 0x32, 0x30, 0x30, 0x41, 0x44, 0x5c, 0x53,
        0x45, 0x43, 0x55, 0x52, 0x49, 0x54, 0x59, 0x2e, 0x48, 0x49, 0x44, 0x00});
    constexpr auto zero_path_sha256 =
        "798bd5318e00348848f0ca4b876d687fec5c606abe88236ff4e922a77fe08b65";
    constexpr auto selector_sha256 =
        "fffa1b0e03e9abf90bfde3bfb86bf1125ae579ede767eea68223e098d641992f";
    constexpr auto security_loader_prefix_sha256 =
        "328e11edf0653b0e0f21db3b61cf9ff95795ec9431f07c0198a700358f75ed74";
    constexpr auto selector_names_sha256 =
        "153a0b62bdec1702cdd36ff6e7dc33ec4ed6673ad5d3f5f8bc07b748f7e06d76";
    constexpr auto security_name_sha256 =
        "1a95edb6109f3db1af0c0389f1aa5d597a184f26725e095f771b6622f654ec6a";
    constexpr auto offset = [](const std::uint16_t address) {
        return static_cast<std::size_t>(address) - load_bias;
    };
    if (game_executable.size() != 54'391
        || to_hex(sha256(game_executable)) != executable_sha256
        || !has_bytes(game_executable, offset(0xd2f5), zero_path)
        || !has_bytes(game_executable, offset(0x1161), selector)
        || !has_bytes(game_executable, offset(0x053a), security_loader_prefix)
        || !has_bytes(game_executable, offset(0x1131), selector_names)
        || !has_bytes(game_executable, offset(0x2f6a), security_name)
        || to_hex(sha256(game_executable.subspan(offset(0xd2f5), zero_path.size())))
            != zero_path_sha256
        || to_hex(sha256(game_executable.subspan(offset(0x1161), selector.size())))
            != selector_sha256
        || to_hex(sha256(game_executable.subspan(offset(0x053a), security_loader_prefix.size())))
            != security_loader_prefix_sha256
        || to_hex(sha256(game_executable.subspan(offset(0x1131), selector_names.size())))
            != selector_names_sha256
        || to_hex(sha256(game_executable.subspan(offset(0x2f6a), security_name.size())))
            != security_name_sha256) {
        throw std::runtime_error("Unsupported Millennium DOS startup zero-path boundary");
    }
    return {std::string(executable_sha256), 0xd2f5, 0x1161, 0xda05,
        {0x01, 0x03, 0x02}, {0x1131, 0x113d, 0x1155, 0x1149}, 0x117c, 0x053a,
        0x2f6a, 0x0550, 0x21, 0x3d, 0x02, std::string(zero_path_sha256),
        std::string(selector_sha256), std::string(security_loader_prefix_sha256),
        std::string(selector_names_sha256), std::string(security_name_sha256)};
}

MillenniumDosStartupNonzeroPathBoundary
parse_millennium_dos_startup_nonzero_path_boundary(
    const std::span<const std::uint8_t> game_executable) {
    // This is the static DX-nonzero edge from $d2f2.  The short jump skips
    // the adjacent zeroing instruction at $d419, so no AH value is inferred.
    // The first called in-image routine reaches an INT 33h boundary before
    // any return or mouse state can be established.
    constexpr std::size_t load_bias = 0x100;
    constexpr auto executable_sha256 =
        "427574e5f780b2a7b5c4207d167116dc44aea3fb67096fbf12a46c4f544a0a57";
    constexpr auto nonzero_entry = std::to_array<std::uint8_t>({
        0xb0, 0x08, 0xeb, 0xcc});
    constexpr auto continuation = std::to_array<std::uint8_t>({
        0x2e, 0xa2, 0xb2, 0x2f, 0x8b, 0x26, 0x2c, 0xd1, 0xe8, 0xbe, 0x35});
    constexpr auto local_callee_prefix = std::to_array<std::uint8_t>({
        0xb8, 0x00, 0x00, 0xcd, 0x33});
    constexpr auto nonzero_entry_sha256 =
        "92252049901ece1d56c7b17fdd7450ce8ade576650b4f7b032f61dd1e4e59522";
    constexpr auto continuation_sha256 =
        "7fb9d6276e557976c68a02e9900531347fd95ecbfbd6fc3fa60cd0c176ca5c5d";
    constexpr auto local_callee_prefix_sha256 =
        "d84b931c90a3b7e1baf2a0a6caf2c67fc5834ed6a160750ba6991b77fdb11909";
    constexpr auto offset = [](const std::uint16_t address) {
        return static_cast<std::size_t>(address) - load_bias;
    };
    if (game_executable.size() != 54'391
        || to_hex(sha256(game_executable)) != executable_sha256
        || !has_bytes(game_executable, offset(0xd44b), nonzero_entry)
        || !has_bytes(game_executable, offset(0xd41b), continuation)
        || !has_bytes(game_executable, offset(0x09e4), local_callee_prefix)
        || to_hex(sha256(game_executable.subspan(offset(0xd44b), nonzero_entry.size())))
            != nonzero_entry_sha256
        || to_hex(sha256(game_executable.subspan(offset(0xd41b), continuation.size())))
            != continuation_sha256
        || to_hex(sha256(game_executable.subspan(offset(0x09e4), local_callee_prefix.size())))
            != local_callee_prefix_sha256) {
        throw std::runtime_error("Unsupported Millennium DOS startup nonzero-path boundary");
    }
    return {std::string(executable_sha256), 0xd44b, 0x08, 0xd44d, 0xd41b,
        0x2fb2, 0xd12c, 0xd423, 0x09e4, 0x09e7, 0x33, 0x00,
        std::string(nonzero_entry_sha256), std::string(continuation_sha256),
        std::string(local_callee_prefix_sha256)};
}

MillenniumDosStartupZeroContinuationBoundary
parse_millennium_dos_startup_zero_continuation_boundary(
    const std::span<const std::uint8_t> game_executable) {
    // $d2f8 is the return site of the $d2f5 selector call. The selector's
    // own local callee reaches DOS first, so this is conditional static
    // provenance rather than an admitted execution path.
    constexpr std::size_t load_bias = 0x100;
    constexpr std::string_view executable_sha256 =
        "427574e5f780b2a7b5c4207d167116dc44aea3fb67096fbf12a46c4f544a0a57";
    constexpr std::uint16_t entry = 0xd2f8;
    constexpr auto continuation = std::to_array<std::uint8_t>({
        0xbe, 0x82, 0x00, 0x32, 0xe4, 0x2e, 0x8a, 0x04, 0x2c, 0x30,
        0xa2, 0x22, 0x01, 0xe8, 0x72, 0xfd, 0xbb, 0x00, 0xfa, 0xb4,
        0x48, 0xcd, 0x21,
    });
    constexpr std::string_view continuation_sha256 =
        "9c7b13c4e0b99e8529e78063b91ae92d967b9fc6de66ebeeaacec01563e4a9d9";
    const auto offset = [](const std::uint16_t address) {
        return static_cast<std::size_t>(address) - load_bias;
    };
    if (game_executable.size() != 54'391
        || to_hex(sha256(game_executable)) != executable_sha256
        || !has_bytes(game_executable, offset(entry), continuation)
        || near_call_target(0xd308, continuation[14], continuation[15]) != 0xd07a
        || to_hex(sha256(game_executable.subspan(offset(entry), continuation.size())))
            != continuation_sha256) {
        throw std::runtime_error("Unsupported Millennium DOS startup zero continuation");
    }
    return {std::string(executable_sha256), entry, continuation.size(),
        std::string(continuation_sha256), 0x0082, 0x30, 0x0122, 0xd305,
        0xd07a, 0xd30d, 0x21, 0x48, 0xfa00};
}

MillenniumDosStartupPostAllocationBoundary
parse_millennium_dos_startup_post_allocation_boundary(
    const std::span<const std::uint8_t> game_executable) {
    // $d30f follows INT $21/AH=$48 only if that native boundary returns.
    // Keep AX/BX uninterpreted: this profile establishes only that their
    // literal register operands occur before the next DOS boundary.
    constexpr std::size_t load_bias = 0x100;
    constexpr std::string_view executable_sha256 =
        "427574e5f780b2a7b5c4207d167116dc44aea3fb67096fbf12a46c4f544a0a57";
    constexpr std::uint16_t entry = 0xd30f;
    constexpr auto boundary = std::to_array<std::uint8_t>({
        0x2e, 0x89, 0x1e, 0x30, 0xd1, 0x8e, 0xc0, 0xb4, 0x49, 0xcd,
    });
    constexpr std::string_view boundary_sha256 =
        "f583faad7bddba301c431adb94fa9d53d5b197dcba2f447b0b654df6f1b452ce";
    const auto offset = [](const std::uint16_t address) {
        return static_cast<std::size_t>(address) - load_bias;
    };
    if (game_executable.size() != 54'391
        || to_hex(sha256(game_executable)) != executable_sha256
        || !has_bytes(game_executable, offset(entry), boundary)
        || to_hex(sha256(game_executable.subspan(offset(entry), boundary.size())))
            != boundary_sha256) {
        throw std::runtime_error("Unsupported Millennium DOS startup post-allocation boundary");
    }
    return {std::string(executable_sha256), entry, boundary.size(), 0xd30f,
        0xd130, 0xd314, 0xd318, 0x21, 0x49, std::string(boundary_sha256)};
}

MillenniumDosStartupPostReleaseContinuation
parse_millennium_dos_startup_post_release_continuation(
    const std::span<const std::uint8_t> game_executable) {
    // This is the caller's literal continuation at the return site following
    // INT $21/AH=$49. It remains conditional static provenance: $d318 may
    // not return, and each direct in-image call may likewise not return. Do
    // not infer a freed segment or usable far pointer from the DOS request.
    constexpr std::size_t load_bias = 0x100;
    constexpr std::string_view executable_sha256 =
        "427574e5f780b2a7b5c4207d167116dc44aea3fb67096fbf12a46c4f544a0a57";
    constexpr std::uint16_t entry = 0xd31a;
    constexpr auto continuation = std::to_array<std::uint8_t>({
        0x0e, 0x1f, 0x5a, 0xc5, 0x16, 0x42, 0x10, 0xb9, 0xff, 0xff,
        0x0e, 0x1f, 0xc5, 0x36, 0x42, 0x10, 0xb8, 0x00, 0x0a, 0x0e,
        0x1f, 0xe8, 0xc0, 0x98, 0xe8, 0xe5, 0x3c, 0xe8, 0x96, 0x3e,
    });
    constexpr std::string_view continuation_sha256 =
        "4d94bf904471cf96a03ce6dd111c0720f396e08ebf2f4603469377db0dc669ef";
    const auto offset = [](const std::uint16_t address) {
        return static_cast<std::size_t>(address) - load_bias;
    };
    if (game_executable.size() != 54'391
        || to_hex(sha256(game_executable)) != executable_sha256
        || !has_bytes(game_executable, offset(entry), continuation)
        || near_call_target(0xd332, continuation[22], continuation[23]) != 0x6bf2
        || near_call_target(0xd335, continuation[25], continuation[26]) != 0x101a
        || near_call_target(0xd338, continuation[28], continuation[29]) != 0x11ce
        || to_hex(sha256(game_executable.subspan(offset(entry), continuation.size())))
            != continuation_sha256) {
        throw std::runtime_error("Unsupported Millennium DOS startup post-release continuation");
    }
    return {std::string(executable_sha256), entry, continuation.size(), 0xd31c,
        0xd31d, 0x1042, 0xd326, 0xd32f, 0x6bf2, 0xd332, 0x101a,
        0xd335, 0x11ce, std::string(continuation_sha256)};
}

MillenniumDosStartupPostGxLoaderBoundary
parse_millennium_dos_startup_post_gx_loader_boundary(
    const std::span<const std::uint8_t> game_executable) {
    // This is not evidence that the GX loader returns.  It only preserves the
    // literal instruction sequence at that encoded return site, up to the
    // next private-runtime boundary.
    constexpr std::size_t load_bias = 0x100;
    constexpr std::string_view executable_sha256 =
        "427574e5f780b2a7b5c4207d167116dc44aea3fb67096fbf12a46c4f544a0a57";
    constexpr std::uint16_t entry = 0xd338;
    constexpr auto boundary = std::to_array<std::uint8_t>({
        0x0e, 0x07, 0xbb, 0xa0, 0xd1, 0xb8, 0x22, 0x00, 0xe8, 0xe1, 0x2d,
    });
    constexpr std::string_view boundary_sha256 =
        "64e7dddae2ca6942cddaa4c564d61203b26c469fc898bb923b2ba227d93876ab";
    const auto offset = [](const std::uint16_t address) {
        return static_cast<std::size_t>(address) - load_bias;
    };
    if (game_executable.size() != 54'391
        || to_hex(sha256(game_executable)) != executable_sha256
        || !has_bytes(game_executable, offset(entry), boundary)
        || near_call_target(0xd343, boundary[9], boundary[10]) != 0x0124
        || to_hex(sha256(game_executable.subspan(offset(entry), boundary.size())))
            != boundary_sha256) {
        throw std::runtime_error("Unsupported Millennium DOS startup post-GX-loader boundary");
    }
    return {std::string(executable_sha256), entry, boundary.size(), 0xd338, 0xd339,
        0xd1a0, 0x0022, 0xd340, 0x0124, 0x91, std::string(boundary_sha256)};
}

MillenniumDosPrivateInt91Wrapper parse_millennium_dos_private_int91_wrapper(
    const std::span<const std::uint8_t> game_executable) {
    // Preserve the routine and its direct caller as code provenance. The
    // private INT is an external boundary: neither this wrapper's preserved
    // stack instructions nor its RET establish any host-side ABI or result.
    constexpr std::size_t load_bias = 0x100;
    constexpr std::string_view executable_sha256 =
        "427574e5f780b2a7b5c4207d167116dc44aea3fb67096fbf12a46c4f544a0a57";
    constexpr std::uint16_t entry = 0x0124;
    constexpr auto wrapper = std::to_array<std::uint8_t>({
        0x1e, 0x56, 0x57, 0x55, 0x06, 0xcd, 0x91, 0x07, 0x5d, 0x5f,
        0x5e, 0x1f, 0xc3,
    });
    constexpr std::string_view wrapper_sha256 =
        "5d17daad68e9062dc6852ae76740db4afdcb81555ba9fb7d15d4e4aa8d088175";
    constexpr std::uint16_t caller_call = 0xd340;
    constexpr auto caller = std::to_array<std::uint8_t>({0xe8, 0xe1, 0x2d});
    const auto offset = [](const std::uint16_t address) {
        return static_cast<std::size_t>(address) - load_bias;
    };
    if (game_executable.size() != 54'391
        || to_hex(sha256(game_executable)) != executable_sha256
        || !has_bytes(game_executable, offset(entry), wrapper)
        || !has_bytes(game_executable, offset(caller_call), caller)
        || near_call_target(0xd343, caller[1], caller[2]) != entry
        || to_hex(sha256(game_executable.subspan(offset(entry), wrapper.size())))
            != wrapper_sha256) {
        throw std::runtime_error("Unsupported Millennium DOS private INT 91 wrapper");
    }
    return {std::string(executable_sha256), entry, wrapper.size(), caller_call, entry,
        0x0124, 0x0125, 0x0126, 0x0127, 0x0128, 0x0129, 0x91, 0x012b,
        0x012c, 0x012d, 0x012e, 0x012f, 0x0130, std::string(wrapper_sha256)};
}

MillenniumDosPostInt91CallerSelector parse_millennium_dos_post_int91_caller_selector(
    const std::span<const std::uint8_t> game_executable) {
    // The near CALL at $d340 returns here only if the private wrapper itself
    // returns. This literal prefix has three comparisons against a byte in
    // the original image, with a fourth fall-through pair, then reaches its
    // first local CALL. The data byte and every call remain native state/code
    // boundaries; preserve the encoded control flow without assigning it an
    // interrupt ABI or host-side effect.
    constexpr std::size_t load_bias = 0x100;
    constexpr std::string_view executable_sha256 =
        "427574e5f780b2a7b5c4207d167116dc44aea3fb67096fbf12a46c4f544a0a57";
    constexpr std::uint16_t entry = 0xd343;
    constexpr auto selector = std::to_array<std::uint8_t>({
        0xba, 0x28, 0x00, 0xb8, 0x0e, 0x00, 0x8a, 0x0e, 0x05, 0xda,
        0x80, 0xf9, 0x03, 0x74, 0x1c, 0xba, 0x50, 0x00, 0xb8, 0x12,
        0x00, 0x80, 0xf9, 0x04, 0x74, 0x11, 0xba, 0xa0, 0x00, 0xb8,
        0x14, 0x00, 0x80, 0xf9, 0x02, 0x74, 0x06, 0xba, 0x40, 0x01,
        0xb8, 0x0f, 0x00, 0x2e, 0x89, 0x16, 0x6e, 0x4b, 0xe8, 0xdc,
        0x98,
    });
    constexpr std::string_view selector_sha256 =
        "571626e83b0787401f89c8586c12dfb4d4221c44e0a9786727d2314b09327091";
    const auto offset = [](const std::uint16_t address) {
        return static_cast<std::size_t>(address) - load_bias;
    };
    if (game_executable.size() != 54'391
        || to_hex(sha256(game_executable)) != executable_sha256
        || !has_bytes(game_executable, offset(entry), selector)
        || near_call_target(0xd376, selector[49], selector[50]) != 0x6c52
        || to_hex(sha256(game_executable.subspan(offset(entry), selector.size())))
            != selector_sha256) {
        throw std::runtime_error("Unsupported Millennium DOS post-INT 91 caller selector");
    }
    return {std::string(executable_sha256), entry, selector.size(), 0xda05,
        0xd34d, 0x03, 0xd358, 0x04, 0xd363, 0x02, 0xd36e, 0x4b6e,
        0xd373, 0x6c52, std::string(selector_sha256)};
}

MillenniumDosPostOverlayAdapterContinuation
parse_millennium_dos_post_overlay_adapter_continuation(
    const std::span<const std::uint8_t> game_executable) {
    // The preceding adapter does a far transfer and may not return. This
    // strictly preserves the instructions located at its encoded caller
    // return site, without turning that continuation into host control flow.
    constexpr std::size_t load_bias = 0x100;
    constexpr std::string_view executable_sha256 =
        "427574e5f780b2a7b5c4207d167116dc44aea3fb67096fbf12a46c4f544a0a57";
    constexpr std::uint16_t entry = 0xd376;
    constexpr auto continuation = std::to_array<std::uint8_t>({
        0xe8, 0xd9, 0xfd, 0xe8, 0x8c, 0x7b, 0xe8, 0x92, 0x6d,
        0xe8, 0x2d, 0x6d, 0xe8, 0x2d, 0x6f, 0xe8, 0xf2, 0x3c,
        0x80, 0x3e, 0x05, 0xda, 0x01, 0x74, 0x05, 0xe8, 0x23,
        0xfe, 0xeb, 0x03, 0xe8, 0x0a, 0xfe, 0x0e, 0x1f, 0x0e,
        0x07, 0x0e, 0x07,
    });
    constexpr std::string_view continuation_sha256 =
        "1df4b30f14434eae3a44463402710bcd1b162200a923c0b9cc1f827faf3763ac";
    const auto offset = [](const std::uint16_t address) {
        return static_cast<std::size_t>(address) - load_bias;
    };
    if (game_executable.size() != 54'391
        || to_hex(sha256(game_executable)) != executable_sha256
        || !has_bytes(game_executable, offset(entry), continuation)
        || near_call_target(0xd379, continuation[1], continuation[2]) != 0xd152
        || near_call_target(0xd37c, continuation[4], continuation[5]) != 0x4f08
        || near_call_target(0xd37f, continuation[7], continuation[8]) != 0x4111
        || near_call_target(0xd382, continuation[10], continuation[11]) != 0x40af
        || near_call_target(0xd385, continuation[13], continuation[14]) != 0x42b2
        || near_call_target(0xd388, continuation[16], continuation[17]) != 0x107a
        || near_call_target(0xd392, continuation[26], continuation[27]) != 0xd1b5
        || near_call_target(0xd397, continuation[31], continuation[32]) != 0xd1a1
        || to_hex(sha256(game_executable.subspan(offset(entry), continuation.size())))
            != continuation_sha256) {
        throw std::runtime_error("Unsupported Millennium DOS post-overlay-adapter continuation");
    }
    return {std::string(executable_sha256), entry, continuation.size(),
        {0xd376, 0xd379, 0xd37c, 0xd37f, 0xd382, 0xd385},
        {0xd152, 0x4f08, 0x4111, 0x40af, 0x42b2, 0x107a},
        0xd388, 0xda05, 0x01, 0xd38d, 0xd394, 0xd38f, 0xd1b5,
        0xd392, 0xd397, 0xd394, 0xd1a1, 0xd397, 0xd398, 0xd39a,
        0xd39c, std::string(continuation_sha256)};
}

MillenniumDosPostOverlayAdapterLoop parse_millennium_dos_post_overlay_adapter_loop(
    const std::span<const std::uint8_t> game_executable) {
    // This begins at the byte directly after the independently bounded
    // segment setup. It remains conditional on the adapter and all earlier
    // calls returning. Preserve only its literal in-image instructions and
    // their direct near targets; the raw byte sequence and every runtime
    // interaction remain native boundaries.
    constexpr std::size_t load_bias = 0x100;
    constexpr std::string_view executable_sha256 =
        "427574e5f780b2a7b5c4207d167116dc44aea3fb67096fbf12a46c4f544a0a57";
    constexpr std::uint16_t entry = 0xd39d;
    constexpr auto loop = std::to_array<std::uint8_t>({
        0xe8, 0xca, 0x70, 0xb8, 0x00, 0x00, 0xe8, 0x79, 0x87,
        0xe8, 0xcf, 0x8d, 0x32, 0xc0, 0xe8, 0xee, 0xa5, 0xe8,
        0x48, 0x7f, 0xe8, 0xcb, 0xa7, 0xb8, 0x00, 0x00, 0xe8,
        0x2a, 0x36, 0x22, 0xc0, 0x75, 0x08, 0xa0, 0xf9, 0x07,
        0x34, 0x01, 0xa2, 0xf9, 0x07, 0xe8, 0xdb, 0x3d, 0xe8,
        0x40, 0x37, 0xe8, 0xd5, 0x3a, 0xe8, 0x89, 0x37, 0xe8,
        0xe6, 0x3a, 0xe8, 0x29, 0xa2, 0xe8, 0xf0, 0xa7, 0xe8,
        0x27, 0x3b, 0x22, 0xc0, 0x74, 0xf0,
    });
    constexpr std::string_view loop_sha256 =
        "1bbb4fcc18668021306de1e0014a9baab1f526af1514fa7ce9d1a61780972cf0";
    const auto offset = [](const std::uint16_t address) {
        return static_cast<std::size_t>(address) - load_bias;
    };
    if (game_executable.size() != 54'391
        || to_hex(sha256(game_executable)) != executable_sha256
        || !has_bytes(game_executable, offset(entry), loop)
        || near_call_target(0xd3a0, loop[1], loop[2]) != 0x446a
        || near_call_target(0xd3a6, loop[7], loop[8]) != 0x5b1f
        || near_call_target(0xd3a9, loop[10], loop[11]) != 0x6178
        || near_call_target(0xd3ae, loop[15], loop[16]) != 0x799c
        || near_call_target(0xd3b1, loop[18], loop[19]) != 0x52f9
        || near_call_target(0xd3b4, loop[21], loop[22]) != 0x7b7f
        || near_call_target(0xd3ba, loop[27], loop[28]) != 0x09e4
        || near_call_target(0xd3c9, loop[42], loop[43]) != 0x11a4
        || near_call_target(0xd3cc, loop[45], loop[46]) != 0x0b0c
        || near_call_target(0xd3cf, loop[48], loop[49]) != 0x0ea4
        || near_call_target(0xd3d2, loop[51], loop[52]) != 0x0b5b
        || near_call_target(0xd3d5, loop[54], loop[55]) != 0x0ebb
        || near_call_target(0xd3d8, loop[57], loop[58]) != 0x7601
        || near_call_target(0xd3db, loop[60], loop[61]) != 0x7bcb
        || near_call_target(0xd3de, loop[63], loop[64]) != 0x0f05
        || to_hex(sha256(game_executable.subspan(offset(entry), loop.size()))) != loop_sha256) {
        throw std::runtime_error("Unsupported Millennium DOS post-overlay-adapter loop");
    }
    return {std::string(executable_sha256), entry, loop.size(),
        {0xd39d, 0xd3a3, 0xd3a6, 0xd3ab, 0xd3ae, 0xd3b1, 0xd3b7,
         0xd3c6, 0xd3c9, 0xd3cc, 0xd3cf, 0xd3d2, 0xd3d5, 0xd3d8, 0xd3db},
        {0x446a, 0x5b1f, 0x6178, 0x799c, 0x52f9, 0x7b7f, 0x09e4,
         0x11a4, 0x0b0c, 0x0ea4, 0x0b5b, 0x0ebb, 0x7601, 0x7bcb, 0x0f05},
        0xd3ba, 0xd3bc, 0xd3c6, 0xd3be, 0x07f9, 0xd3c1, 0x01,
        0xd3c3, 0xd3de, 0xd3e0, 0xd3d2, 0xd3e2, std::string(loop_sha256)};
}

MillenniumDosPostOverlayDispatchPrefix parse_millennium_dos_post_overlay_dispatch_prefix(
    const std::span<const std::uint8_t> game_executable) {
    // This is the literal main-loop dispatcher reached only by the preceding
    // post-overlay span falling through. Keep every branch conditional and
    // every target native: the parser admits code provenance, not an input
    // event, a guard value, a selected table entry, or a call return.
    constexpr std::size_t load_bias = 0x100;
    constexpr std::string_view executable_sha256 =
        "427574e5f780b2a7b5c4207d167116dc44aea3fb67096fbf12a46c4f544a0a57";
    constexpr std::uint16_t entry = 0xd3e2;
    constexpr auto prefix = std::to_array<std::uint8_t>({
        0x32, 0xe4, 0x3c, 0x0b, 0x74, 0x26, 0x8a, 0x0e, 0x3a, 0xda,
        0x22, 0xc9, 0x75, 0xe2, 0x3c, 0x0c, 0x75, 0x05, 0xe8, 0x79,
        0x01, 0x33, 0xc0, 0x2c, 0x3b, 0x3c, 0x0a, 0x73, 0xd3, 0xbe,
        0xbf, 0x2f, 0x32, 0xe4, 0xc0, 0xe0, 0x03, 0x01, 0xc6, 0xe8,
        0xe4, 0xa2, 0xeb, 0xc4, 0xe8, 0x93, 0x3d, 0xeb, 0xbf,
    });
    constexpr std::string_view prefix_sha256 =
        "7abec93ec23f7ca3c4b400e16b9e746da7b0b9a1dd4bec88ba891ef04b322065";
    const auto offset = static_cast<std::size_t>(entry) - load_bias;
    if (game_executable.size() != 54'391
        || to_hex(sha256(game_executable)) != executable_sha256
        || !has_bytes(game_executable, offset, prefix)
        || near_call_target(0xd3f7, prefix[19], prefix[20]) != 0xd570
        || near_call_target(0xd40d, prefix[40], prefix[41]) != 0x76f1
        || near_call_target(0xd411, prefix[45], prefix[46]) != 0x11a4
        || to_hex(sha256(game_executable.subspan(offset, prefix.size()))) != prefix_sha256) {
        throw std::runtime_error("Unsupported Millennium DOS post-overlay dispatch prefix");
    }
    return {std::string(executable_sha256), entry, prefix.size(),
        0xd3e4, 0x0b, 0xd3e6, 0xd40e, 0xd3e8, 0xda3a, 0xd3ec,
        0xd3ee, 0xd3d2, 0xd3f0, 0x0c, 0xd3f2, 0xd3f8, 0xd3f4,
        0xd570, 0xd3f9, 0x3b, 0xd3fb, 0x0a, 0xd3fd, 0xd3d2,
        0xd3ff, 0x2fbf, 0xd40a, 0x76f1, 0xd40d, 0xd3d2, 0xd40e,
        0x11a4, 0xd411, 0xd3d2, std::string(prefix_sha256)};
}

MillenniumDosEighthFunctionKeyRepeatLoop
evaluate_millennium_dos_eighth_function_key_repeat_loop(
    const std::span<const std::uint8_t> game_executable,
    const std::span<const std::uint8_t> helper_return_bl_values) {
    // F8 reaches this only if its earlier $731a preflight returns. The exact
    // local tail is CALL $09fa; SHR BL,1; JC $7312; RET. $09fa itself remains
    // opaque, hence the caller-provided return bytes.
    constexpr std::size_t loop_offset = 0x7312 - 0x100;
    constexpr std::size_t loop_size = 8;
    constexpr std::string_view loop_sha256 =
        "2bf85a49d14034fb5562af6188745810721fd42e495877464d04f69783525a0a";
    if (loop_offset > game_executable.size() || loop_size > game_executable.size() - loop_offset) {
        throw std::runtime_error("Millennium DOS F8 repeat loop lies outside executable");
    }
    const auto loop = game_executable.subspan(loop_offset, loop_size);
    if (to_hex(sha256(loop)) != loop_sha256) {
        throw std::runtime_error("Unsupported Millennium DOS F8 repeat loop");
    }
    if (helper_return_bl_values.empty()) {
        throw std::runtime_error("Millennium DOS F8 repeat loop needs helper return");
    }
    MillenniumDosEighthFunctionKeyRepeatLoop result;
    result.call_address = 0x7312;
    result.helper_address = 0x09fa;
    result.shift_address = 0x7315;
    result.return_address = 0x7319;
    for (std::size_t index = 0; index < helper_return_bl_values.size(); ++index) {
        const auto returned_bl = helper_return_bl_values[index];
        const auto shifted_bl = static_cast<std::uint8_t>(returned_bl >> 1U);
        result.shifted_bl_values.push_back(shifted_bl);
        // JC observes the bit shifted out of BL. A clear carry reaches RET;
        // a set carry returns to the same original CALL instruction.
        if ((returned_bl & 1U) == 0) {
            if (index + 1 != helper_return_bl_values.size()) {
                throw std::runtime_error("Unexpected trailing Millennium DOS F8 helper return");
            }
            result.final_bl = shifted_bl;
            return result;
        }
    }
    throw std::runtime_error("Millennium DOS F8 repeat loop lacks terminating helper return");
}

MillenniumDosEighthFunctionKeyPreflight
evaluate_millennium_dos_eighth_function_key_preflight(
    const std::span<const std::uint8_t> game_executable,
    const std::uint8_t enabled_byte, const std::uint8_t counter_byte) {
    constexpr std::size_t preflight_offset = 0x731a - 0x100;
    constexpr std::size_t preflight_size = 31;
    constexpr std::string_view preflight_sha256 =
        "71c2c4189e66104aea08d4f7040e9d6bc873eb6717607eed30cf61ce27f5ac2e";
    if (preflight_offset > game_executable.size()
        || preflight_size > game_executable.size() - preflight_offset) {
        throw std::runtime_error("Millennium DOS F8 preflight lies outside executable");
    }
    const auto preflight = game_executable.subspan(preflight_offset, preflight_size);
    if (to_hex(sha256(preflight)) != preflight_sha256) {
        throw std::runtime_error("Unsupported Millennium DOS F8 preflight");
    }
    MillenniumDosEighthFunctionKeyPreflight result;
    result.entry_address = 0x731a;
    result.enabled_byte_address = 0xda39;
    result.enabled_byte_value = enabled_byte;
    result.helper_address = 0x7b47;
    result.counter_byte_address = 0xda0a;
    result.initial_counter_byte = counter_byte;
    result.translation_table_address = 0xdb4b;
    result.table_jump_address = 0x7948;
    if (enabled_byte != 0) {
        // $7b47 is a native helper. Its return effect is not assumed; the
        // following RET is recorded only as the local fall-through location.
        result.return_address = 0x7324;
        result.outcome = MillenniumDosEighthFunctionKeyPreflightOutcome::helper_boundary;
        return result;
    }
    if (counter_byte == 0) {
        result.return_address = 0x732c;
        result.outcome = MillenniumDosEighthFunctionKeyPreflightOutcome::returns;
        return result;
    }
    const auto decremented = static_cast<std::uint8_t>(counter_byte - 1U);
    result.decremented_counter_byte = decremented;
    result.translation_index = decremented;
    // XLAT reads CS:[$db4b + AL], but that native-memory table lies beyond
    // this COM image. Preserve its address/index and stop before fabricating
    // a byte or following the $7948 routine.
    result.outcome = MillenniumDosEighthFunctionKeyPreflightOutcome::table_jump_boundary;
    return result;
}

MillenniumDosEighthFunctionKeyTableJumpPrefix
evaluate_millennium_dos_eighth_function_key_table_jump_prefix(
    const std::span<const std::uint8_t> game_executable, const std::uint8_t translated_al) {
    constexpr std::size_t entry_offset = 0x7948 - 0x100;
    constexpr std::size_t entry_size = 32;
    constexpr std::string_view entry_sha256 =
        "c52d83152fef75a81d8956b76e7c6931ced4de6a579f4233faf8a28c3cdc72c9";
    constexpr std::size_t table_offset = 0x78f4 - 0x100;
    constexpr std::size_t table_entry_count = 10;
    constexpr std::string_view table_sha256 =
        "c42e986a183a46d7b4cdf7787766e5f81446b444180e0cf34d9fa5f4b8d50a0d";
    if (translated_al >= table_entry_count || entry_offset > game_executable.size()
        || entry_size > game_executable.size() - entry_offset || table_offset > game_executable.size()
        || table_entry_count * 2U > game_executable.size() - table_offset) {
        throw std::runtime_error("Millennium DOS F8 table-jump prefix lies outside executable");
    }
    const auto entry = game_executable.subspan(entry_offset, entry_size);
    const auto table = game_executable.subspan(table_offset, table_entry_count * 2U);
    if (to_hex(sha256(entry)) != entry_sha256 || to_hex(sha256(table)) != table_sha256) {
        throw std::runtime_error("Unsupported Millennium DOS F8 table-jump prefix");
    }
    const auto pointer_offset = static_cast<std::size_t>(translated_al) * 2U;
    const auto selected_pointer = static_cast<std::uint16_t>(
        static_cast<std::uint16_t>(table[pointer_offset])
        | static_cast<std::uint16_t>(static_cast<std::uint16_t>(table[pointer_offset + 1U]) << 8U));
    return {
        .entry_address = 0x7948,
        .translated_al = translated_al,
        .reset_runtime_byte_address = 0xda09,
        .reset_runtime_byte_value = 0,
        .selected_runtime_byte_address = 0xda06,
        .selected_runtime_byte_value = translated_al,
        .selector_table_address = 0x78f4,
        .selected_pointer = selected_pointer,
        .next_gate_runtime_byte_address = 0x6e2f,
        .nonzero_gate_address = 0x799a,
        .zero_gate_address = 0x7968,
    };
}

MillenniumDosEighthFunctionKeySelectedRecordGate
evaluate_millennium_dos_eighth_function_key_selected_record_gate(
    const std::span<const std::uint8_t> game_executable, const std::uint8_t translated_al,
    const std::uint8_t gate_runtime_byte) {
    constexpr std::size_t interpreter_offset = 0x7948 - 0x100;
    constexpr std::size_t interpreter_size = 84;
    constexpr std::string_view interpreter_sha256 =
        "99267e09fea1f7d3227b49b3c80a2eacf6673df542bb063da7c54ce87df8a666";
    constexpr std::size_t table_offset = 0x78f4 - 0x100;
    constexpr std::size_t table_entry_count = 10;
    constexpr std::string_view table_sha256 =
        "c42e986a183a46d7b4cdf7787766e5f81446b444180e0cf34d9fa5f4b8d50a0d";
    constexpr std::size_t record_bank_offset = 0x77f8 - 0x100;
    constexpr std::size_t record_bank_size = 152;
    constexpr std::string_view record_bank_sha256 =
        "53315644dbe9478d9e8b919d3958cf64cac95260fd3f89b600d92275f97e089c";
    if (translated_al >= table_entry_count || interpreter_offset > game_executable.size()
        || interpreter_size > game_executable.size() - interpreter_offset
        || table_offset > game_executable.size()
        || table_entry_count * 2U > game_executable.size() - table_offset
        || record_bank_offset > game_executable.size()
        || record_bank_size > game_executable.size() - record_bank_offset) {
        throw std::runtime_error("Millennium DOS F8 selected-record gate lies outside executable");
    }
    const auto interpreter = game_executable.subspan(interpreter_offset, interpreter_size);
    const auto table = game_executable.subspan(table_offset, table_entry_count * 2U);
    const auto record_bank = game_executable.subspan(record_bank_offset, record_bank_size);
    if (to_hex(sha256(interpreter)) != interpreter_sha256
        || to_hex(sha256(table)) != table_sha256
        || to_hex(sha256(record_bank)) != record_bank_sha256) {
        throw std::runtime_error("Unsupported Millennium DOS F8 selected-record gate");
    }
    const auto pointer_offset = static_cast<std::size_t>(translated_al) * 2U;
    const auto selected_pointer = static_cast<std::uint16_t>(
        static_cast<std::uint16_t>(table[pointer_offset])
        | static_cast<std::uint16_t>(static_cast<std::uint16_t>(table[pointer_offset + 1U]) << 8U));
    MillenniumDosEighthFunctionKeySelectedRecordGate result;
    result.entry_address = 0x7961;
    result.translated_al = translated_al;
    result.selector_table_address = 0x78f4;
    result.selected_pointer = selected_pointer;
    result.gate_runtime_byte_address = 0x6e2f;
    result.gate_runtime_byte_value = gate_runtime_byte;
    result.zero_gate_address = 0x7968;
    result.nonzero_gate_address = 0x799a;
    result.return_address = 0x799b;
    if (gate_runtime_byte != 0) {
        // JNE $799a; POP DS; RET.  The selected pointer has already been
        // loaded from the original table, but no byte at that pointer is read.
        return result;
    }
    constexpr std::size_t record_prefix_size = 5;
    if (selected_pointer < 0x77f8 || selected_pointer > 0x788b) {
        throw std::runtime_error("Millennium DOS F8 selected record outside verified bank");
    }
    const auto selected_offset = static_cast<std::size_t>(selected_pointer - 0x100);
    if (selected_offset > game_executable.size()
        || record_prefix_size > game_executable.size() - selected_offset) {
        throw std::runtime_error("Millennium DOS F8 selected record truncated");
    }
    const auto record = game_executable.subspan(selected_offset, record_prefix_size);
    // $7968 loads byte zero and the following word as local register facts.
    // $7974 then loads byte three as the loop count.  All hash-accepted
    // selected records have a nonzero count, so $797f reaches $7924 after
    // loading the first list byte.  Stop at that native ABI boundary.
    if (record[3] == 0) {
        throw std::runtime_error("Unsupported Millennium DOS F8 empty selected record");
    }
    result.record_byte_0 = record[0];
    result.record_word_1 = static_cast<std::uint16_t>(
        static_cast<std::uint16_t>(record[1])
        | static_cast<std::uint16_t>(static_cast<std::uint16_t>(record[2]) << 8U));
    result.record_byte_3 = record[3];
    result.record_byte_4 = record[4];
    result.first_helper_call_address = 0x797f;
    result.first_helper_address = 0x7924;
    result.outcome = MillenniumDosEighthFunctionKeySelectedRecordOutcome::first_helper_boundary;
    return result;
}

MillenniumDosFirstSpecialActionPrefix evaluate_millennium_dos_first_special_action_prefix(
    const std::span<const std::uint8_t> game_executable,
    const std::uint8_t observed_runtime_byte) {
    constexpr std::size_t load_bias = 0x100;
    constexpr std::size_t dispatch_offset = 0xd3e2 - load_bias;
    constexpr std::size_t handler_offset = 0x11a4 - load_bias;
    constexpr auto dispatch_bytes = std::to_array<std::uint8_t>({
        0x32, 0xe4, 0x3c, 0x0b, 0x74, 0x26, 0x8a, 0x0e, 0x3a, 0xda,
        0x22, 0xc9, 0x75, 0xe2, 0x3c, 0x0c, 0x75, 0x05, 0xe8, 0x79,
        0x01, 0x33, 0xc0, 0x2c, 0x3b, 0x3c, 0x0a, 0x73, 0xd3, 0xbe,
        0xbf, 0x2f, 0x32, 0xe4, 0xc0, 0xe0, 0x03, 0x01, 0xc6, 0xe8,
        0xe4, 0xa2, 0xeb,
    });
    constexpr auto handler_bytes = std::to_array<std::uint8_t>({
        0x8a, 0x0e, 0xf9, 0x07, 0x22, 0xc9, 0xb8, 0x8f, 0x01, 0x74,
        0x01, 0x48, 0x80, 0xf1, 0x01, 0x88, 0x0e, 0xf9, 0x07, 0xe8,
        0xac, 0xf4,
    });
    constexpr std::string_view dispatch_hash =
        "1e4e43aad1a2507aa7f85189022063db0f0cb481d267ef79789a447c3e184d62";
    constexpr std::string_view handler_hash =
        "2cd76e49776b940065ecb01418394984a9e03a6d6a6fc161c218f450faac1ed5";
    if (!has_bytes(game_executable, dispatch_offset, dispatch_bytes)
        || !has_bytes(game_executable, handler_offset, handler_bytes)) {
        throw std::runtime_error("Unsupported Millennium DOS first special-action prefix");
    }
    const auto dispatch = game_executable.subspan(dispatch_offset, dispatch_bytes.size());
    const auto handler = game_executable.subspan(handler_offset, handler_bytes.size());
    if (to_hex(sha256(dispatch)) != dispatch_hash || to_hex(sha256(handler)) != handler_hash) {
        throw std::runtime_error("Unsupported Millennium DOS first special-action hashes");
    }
    return {
        .action = 0x0b,
        .dispatch_branch_address = 0xd3e6,
        .dispatch_call_address = 0xd40e,
        .handler_address = 0x11a4,
        .runtime_byte_address = 0x07f9,
        .observed_runtime_byte = observed_runtime_byte,
        .toggled_runtime_byte = static_cast<std::uint8_t>(observed_runtime_byte ^ 1U),
        .selected_ax_value = static_cast<std::uint16_t>(
            observed_runtime_byte == 0 ? 0x018f : 0x018e),
        .helper_call_address = 0x11b7,
        .helper_address = near_call_target(0x11ba, handler[20], handler[21]),
    };
}

MillenniumDosSharedHelperPrefix evaluate_millennium_dos_shared_helper_prefix(
    const std::span<const std::uint8_t> game_executable, const std::uint16_t caller_ax) {
    constexpr std::size_t load_bias = 0x100;
    constexpr std::size_t offset = 0x0666 - load_bias;
    constexpr auto expected = std::to_array<std::uint8_t>({
        0x1e, 0x56, 0x50, 0x2e, 0x8e, 0x1e, 0x16, 0x01, 0x2e, 0xc6,
        0x06, 0xc8, 0x05, 0x00, 0xd1, 0xe0, 0x8b, 0xf0, 0xad, 0x8b,
        0xf0, 0xe8, 0x79, 0xff, 0x58, 0x5e, 0x1f, 0xc3,
    });
    constexpr std::string_view executable_hash =
        "427574e5f780b2a7b5c4207d167116dc44aea3fb67096fbf12a46c4f544a0a57";
    constexpr std::string_view prefix_hash =
        "8dc7586f3809a14f3ed6acd601cd42486841adb9d9cb09d3e9b1ed727329e485";
    if (to_hex(sha256(game_executable)) != executable_hash
        || !has_bytes(game_executable, offset, expected)) {
        throw std::runtime_error("Unsupported Millennium DOS shared helper executable");
    }
    const auto bytes = game_executable.subspan(offset, expected.size());
    if (to_hex(sha256(bytes)) != prefix_hash) {
        throw std::runtime_error("Unsupported Millennium DOS shared helper prefix");
    }
    return {0x0666, caller_ax, 0x0116, 0x05c8, 0,
        static_cast<std::uint16_t>(caller_ax << 1U), 0x0678, 0x067b,
        near_call_target(0x067e, bytes[22], bytes[23]), std::string(prefix_hash)};
}

MillenniumDosSecondSpecialActionPrefix evaluate_millennium_dos_second_special_action_prefix(
    const std::span<const std::uint8_t> game_executable,
    const std::uint8_t observed_runtime_byte) {
    constexpr std::size_t load_bias = 0x100;
    constexpr std::size_t dispatch_offset = 0xd3e8 - load_bias;
    constexpr std::size_t handler_offset = 0xd570 - load_bias;
    constexpr auto dispatch_bytes = std::to_array<std::uint8_t>({
        0x8a, 0x0e, 0x3a, 0xda, 0x22, 0xc9, 0x75, 0xe2, 0x3c, 0x0c,
        0x75, 0x05, 0xe8, 0x79, 0x01,
    });
    constexpr auto handler_bytes = std::to_array<std::uint8_t>({
        0xb8, 0x0d, 0x00, 0xe8, 0xdc, 0x96, 0xc3,
    });
    constexpr std::string_view dispatch_hash =
        "e59faad9b95521837b340ff56ef032cb140327bfabb0b39be32d01bb9c05bda3";
    constexpr std::string_view handler_hash =
        "f266d52e554a2e85147994b34eb69e7678cd9339fda1b99206c18fc05361232b";
    if (!has_bytes(game_executable, dispatch_offset, dispatch_bytes)
        || !has_bytes(game_executable, handler_offset, handler_bytes)) {
        throw std::runtime_error("Unsupported Millennium DOS second special-action prefix");
    }
    const auto dispatch = game_executable.subspan(dispatch_offset, dispatch_bytes.size());
    const auto handler = game_executable.subspan(handler_offset, handler_bytes.size());
    if (to_hex(sha256(dispatch)) != dispatch_hash || to_hex(sha256(handler)) != handler_hash) {
        throw std::runtime_error("Unsupported Millennium DOS second special-action hashes");
    }
    MillenniumDosSecondSpecialActionPrefix result;
    result.action = 0x0c;
    result.runtime_byte_address = 0xda3a;
    result.observed_runtime_byte = observed_runtime_byte;
    result.blocked_loop_address = 0xd3d2;
    result.handler_address = 0xd570;
    result.selected_ax_value = 0x000d;
    result.helper_call_address = 0xd573;
    result.helper_address = near_call_target(0xd576, handler[4], handler[5]);
    if (observed_runtime_byte == 0) {
        result.outcome = MillenniumDosSecondSpecialActionOutcome::helper_boundary;
    }
    return result;
}

MillenniumDosGxOverlayLoadEvidence parse_millennium_dos_gx_overlay_load_evidence(
    const std::span<const std::uint8_t> game_executable,
    const std::span<const std::uint8_t> gx_overlay_executable) {
    constexpr auto game_sha256 =
        "427574e5f780b2a7b5c4207d167116dc44aea3fb67096fbf12a46c4f544a0a57";
    constexpr auto overlay_sha256 =
        "093f8416de6d23837d2faf82360ef79777c2c2bf146619aafad87626c61ab6fb";
    constexpr std::size_t load_bias = 0x100;
    constexpr std::uint16_t name_address = 0x11c2;
    constexpr auto name = std::to_array<std::uint8_t>({
        0x32, 0x32, 0x30, 0x30, 0x47, 0x58, 0x2e, 0x45, 0x58, 0x45, 0x00, 0x00});
    constexpr std::uint16_t loader_address = 0x11ce;
    constexpr auto loader = std::to_array<std::uint8_t>({
        0xba, 0xc2, 0x11, 0xe8, 0x66, 0xf3, 0x73, 0x03, 0xe9, 0xce, 0xf3,
        0xb9, 0xff, 0xff, 0x33, 0xd2, 0x2e, 0xa1, 0x18, 0x01, 0x8e, 0xd8,
        0xe8, 0x8d, 0xf3, 0x73, 0x03, 0xe9, 0xbb, 0xf3, 0xe8, 0xa7, 0xf3,
        0x0e, 0x1f, 0x73, 0x03, 0xe9, 0xb1, 0xf3, 0xc3});
    constexpr auto loader_sha256 =
        "a8972b74ad9d1dfabe508c42b7fcda0fb45e0d449613449ab8a2763ca8ecff45";
    constexpr std::uint16_t caller_address = 0xd335;
    constexpr std::array<std::uint8_t, 3> caller{{0xe8, 0x96, 0x3e}};
    const auto offset = [](const std::uint16_t address) {
        return static_cast<std::size_t>(address) - load_bias;
    };
    if (to_hex(sha256(game_executable)) != game_sha256
        || to_hex(sha256(gx_overlay_executable)) != overlay_sha256
        || !has_bytes(game_executable, offset(name_address), name)
        || !has_bytes(game_executable, offset(loader_address), loader)
        || !has_bytes(game_executable, offset(caller_address), caller)
        || to_hex(sha256(game_executable.subspan(offset(loader_address), loader.size())))
            != loader_sha256) {
        throw std::runtime_error("Unexpected Millennium DOS GX overlay load evidence");
    }
    return {game_sha256, overlay_sha256, name_address, loader_address, 0x0118,
        0x11d1, near_call_target(0x11d4, loader[4], loader[5]),
        0x11e4, near_call_target(0x11e7, loader[23], loader[24]),
        0x11ec, near_call_target(0x11ef, loader[31], loader[32]), 0x11f6,
        caller_address, near_call_target(0xd338, caller[1], caller[2]), loader_sha256};
}

MillenniumDosStaticDataLoadEvidence parse_millennium_dos_static_data_load_evidence(
    const std::span<const std::uint8_t> game_executable) {
    constexpr std::size_t load_bias = 0x100;
    constexpr auto game_sha256 = "427574e5f780b2a7b5c4207d167116dc44aea3fb67096fbf12a46c4f544a0a57";
    constexpr std::uint16_t name_address = 0x100d;
    constexpr auto name = std::to_array<std::uint8_t>({0x32,0x32,0x30,0x30,0x41,0x44,0x34,0x2e,0x42,0x49,0x4e,0x00});
    constexpr std::uint16_t loader_address = 0x101a;
    constexpr auto loader = std::to_array<std::uint8_t>({
        0xba,0x0d,0x10,0xe8,0x1a,0xf5,0x73,0x03,0xe9,0x82,0xf5,0xb9,0xff,0xff,0x8e,0x1e,
        0x16,0x01,0x33,0xd2,0xe8,0x43,0xf5,0x73,0x03,0xe9,0x71,0xf5,0xe8,0x5d,0xf5,0x0e,
        0x1f,0x73,0x03,0xe9,0x67,0xf5,0xc3});
    constexpr std::uint16_t caller_address = 0xd332;
    constexpr auto caller = std::to_array<std::uint8_t>({0xe8,0xe5,0x3c});
    constexpr auto caller_sha256 = "f8b1e2bed8701d623133bbe3d5d24e133a2e78ee068a7f335fb43289bffaf286";
    constexpr auto loader_sha256 = "d81719b0293c15ad5edbc5c816feb0c44e78abdde749473e5b5795848e4c86cb";
    constexpr auto name_sha256 = "91032791cbe9e4cfaa88d2f3d9d4882e58dd66ccfbc8a0c457af21dfcefd63ae";
    const auto offset = [](const std::uint16_t address) { return static_cast<std::size_t>(address) - load_bias; };
    if (to_hex(sha256(game_executable)) != game_sha256
        || !has_bytes(game_executable, offset(name_address), name)
        || !has_bytes(game_executable, offset(loader_address), loader)
        || !has_bytes(game_executable, offset(caller_address), caller)
        || to_hex(sha256(game_executable.subspan(offset(name_address), name.size()))) != name_sha256
        || to_hex(sha256(game_executable.subspan(offset(loader_address), loader.size()))) != loader_sha256
        || to_hex(sha256(game_executable.subspan(offset(caller_address), caller.size()))) != caller_sha256) {
        throw std::runtime_error("Unexpected Millennium DOS static-data load evidence");
    }
    return {name_address, loader_address, caller_address, near_call_target(0xd335, caller[1], caller[2]),
        0x101d, 0x102e, 0x1036, 0x1040, caller_sha256, loader_sha256, name_sha256};
}

MillenniumDosGxOverlayAdapterEvidence parse_millennium_dos_gx_overlay_adapter_evidence(
    const std::span<const std::uint8_t> game_executable,
    const MillenniumDosGxOverlayLoadEvidence& loader) {
    constexpr std::size_t load_bias = 0x100;
    constexpr std::uint16_t entry = 0x6c52;
    constexpr auto expected = std::to_array<std::uint8_t>({
        0x1e, 0x56, 0x06, 0x57, 0x51, 0x8c, 0xc9, 0x51, 0xb9, 0x69, 0x6c,
        0x51, 0x2e, 0x8b, 0x0e, 0x18, 0x01, 0x51, 0xb9, 0x00, 0x00, 0x51,
        0xcb, 0x50, 0x0e, 0x1f, 0x58, 0x59, 0x5f, 0x07, 0x5e, 0x1f, 0xc3});
    constexpr auto expected_sha256 =
        "b34e5abf8ecd790fce3e7a032d7a7fcacc073d03909e98fd33f9503113e3ad87";
    const auto offset = static_cast<std::size_t>(entry) - load_bias;
    if (loader.loader_segment_cell_address != 0x0118 || loader.caller_target != loader.loader_entry_address
        || !has_bytes(game_executable, offset, expected)
        || to_hex(sha256(game_executable.subspan(offset, expected.size()))) != expected_sha256) {
        throw std::runtime_error("Unexpected Millennium DOS GX overlay adapter evidence");
    }
    return {entry, 0x0118, 0x0000, 0x6c68, 0x6c69, 0x6c72, expected_sha256};
}

MillenniumDosGxOverlayDispatcherEvidence parse_millennium_dos_gx_overlay_dispatcher_evidence(
    const std::span<const std::uint8_t> gx_overlay_executable,
    const MillenniumDosGxOverlayAdapterEvidence& adapter) {
    constexpr auto overlay_sha256 =
        "093f8416de6d23837d2faf82360ef79777c2c2bf146619aafad87626c61ab6fb";
    constexpr std::array<std::uint8_t, 20> dispatch{{
        0x32, 0xe4, 0x0e, 0x07, 0x0e, 0x1f, 0xbe, 0x15, 0x00, 0xd1,
        0xe0, 0x01, 0xc6, 0xad, 0xbe, 0x14, 0x00, 0x56, 0x50, 0xc3}};
    constexpr std::array<std::uint8_t, 42> table{{
        0x0c,0x08,0x26,0x25,0x57,0x25,0x8a,0x25,0x73,0x25,0x13,0x2e,
        0xa1,0x25,0x14,0x25,0x8f,0x25,0x19,0x26,0x35,0x26,0x60,0x26,
        0x8b,0x26,0xd0,0x08,0x90,0x00,0x9f,0x00,0x98,0x25,0x3f,0x31,
        0x97,0x00,0x72,0x31,0xa7,0x00}};
    constexpr auto dispatch_sha256 =
        "f4d657fcbdda23d7f0fdf2bbf48405d0a04e8b8149df064607f49042525fbd55";
    constexpr auto table_sha256 =
        "4d04568e05378787921012654fe9c157419ce7c07f9943b51135258f32a06df3";
    if (adapter.overlay_entry_offset != 0 || adapter.far_transfer_address != 0x6c68
        || to_hex(sha256(gx_overlay_executable)) != overlay_sha256
        || !has_bytes(gx_overlay_executable, 0, dispatch)
        || gx_overlay_executable.size() <= 0x14 || gx_overlay_executable[0x14] != 0xcb
        || !has_bytes(gx_overlay_executable, 0x15, table)
        || to_hex(sha256(gx_overlay_executable.subspan(0, dispatch.size()))) != dispatch_sha256
        || to_hex(sha256(gx_overlay_executable.subspan(0x15, table.size()))) != table_sha256) {
        throw std::runtime_error("Unexpected Millennium DOS GX overlay dispatcher evidence");
    }
    return {0, 0x14, 0x15,
        {0x080c,0x2526,0x2557,0x258a,0x2573,0x2e13,0x25a1,0x2514,0x258f,0x2619,
         0x2635,0x2660,0x268b,0x08d0,0x0090,0x009f,0x2598,0x313f,0x0097,0x3172,0x00a7},
        dispatch_sha256, table_sha256};
}

MillenniumDosGxOverlaySelectorEvidence parse_millennium_dos_gx_overlay_selector_evidence(
    const std::span<const std::uint8_t> game_executable,
    const std::span<const std::uint8_t> gx_overlay_executable,
    const MillenniumDosGxOverlayAdapterEvidence& adapter,
    const MillenniumDosGxOverlayDispatcherEvidence& dispatcher) {
    constexpr std::size_t load_bias = 0x100;
    constexpr std::uint16_t caller_address = 0xd343;
    constexpr auto caller = std::to_array<std::uint8_t>({
        0xba,0x28,0x00,0xb8,0x0e,0x00,0x8a,0x0e,0x05,0xda,0x80,0xf9,0x03,0x74,0x1c,
        0xba,0x50,0x00,0xb8,0x12,0x00,0x80,0xf9,0x04,0x74,0x11,0xba,0xa0,0x00,0xb8,
        0x14,0x00,0x80,0xf9,0x02,0x74,0x06,0xba,0x40,0x01,0xb8,0x0f,0x00,0x2e,0x89,
        0x16,0x6e,0x4b,0xe8,0xdc,0x98});
    constexpr auto caller_sha256 =
        "571626e83b0787401f89c8586c12dfb4d4221c44e0a9786727d2314b09327091";
    constexpr std::size_t overlay_offset = 0x90;
    constexpr std::size_t overlay_length = 94;
    constexpr auto overlay_sha256 =
        "8d412472415d513482b5c70198bb1aa04fa0d25798dd5f4b40b262151c489736";
    if (adapter.entry_address != 0x6c52 || dispatcher.observed_selector_targets[0x0e] != 0x0090
        || dispatcher.observed_selector_targets[0x0f] != 0x009f
        || dispatcher.observed_selector_targets[0x12] != 0x0097
        || dispatcher.observed_selector_targets[0x14] != 0x00a7
        || !has_bytes(game_executable, static_cast<std::size_t>(caller_address) - load_bias, caller)
        || to_hex(sha256(game_executable.subspan(static_cast<std::size_t>(caller_address) - load_bias,
            caller.size()))) != caller_sha256
        || overlay_offset > gx_overlay_executable.size()
        || overlay_length > gx_overlay_executable.size() - overlay_offset
        || to_hex(sha256(gx_overlay_executable.subspan(overlay_offset, overlay_length))) != overlay_sha256) {
        throw std::runtime_error("Unexpected Millennium DOS GX overlay selector evidence");
    }
    return {caller_address, 0xda05, {0x03,0x04,0x02}, {0x0028,0x0050,0x00a0,0x0140},
        {0x000e,0x0012,0x0014,0x000f}, {0x0070,0x0080,0x0088,0x0078},
        0x4b6e, 0xd373, 0x6c52, caller_sha256, overlay_sha256};
}

MillenniumDosGxOverlayStartupRecordEvidence
parse_millennium_dos_gx_overlay_startup_record_evidence(
    const std::span<const std::uint8_t> gx_overlay_executable,
    const MillenniumDosGxOverlaySelectorEvidence& selector) {
    constexpr std::string_view overlay_sha256 =
        "093f8416de6d23837d2faf82360ef79777c2c2bf146619aafad87626c61ab6fb";
    constexpr std::size_t first_entry_offset = 0x90;
    constexpr auto entry_span = std::to_array<std::uint8_t>({
        0xbe,0x70,0x00,0x33,0xc0,0xeb,0x1d,0xbe,0x80,0x00,0xb8,0x02,0x00,0xeb,0x15,0xbe,
        0x78,0x00,0xb8,0x01,0x00,0xeb,0x0d,0xb8,0x00,0xb8,0x2e,0xa3,0x5a,0x00,0xbe,0x88,
        0x00,0xb8,0x03,0x00,0xbf,0x65,0x00,0xa5,0xa5,0xa5,0xa5,0xa2,0x6d,0x00,0xb8,0xf4,
        0x00,0xb9,0xf0,0x00,0xba,0xf2,0x00,0x05,0x08,0x00,0x25,0xff,0x03,0x83,0xc1,0x02,
        0x81,0xe1,0xff,0x03,0x83,0xc2,0x04,0x81,0xe2,0xff,0x03,0x89,0x06,0xf4,0x00,0x89,
        0x0e,0xf0,0x00,0x89,0x16,0xf2,0x00,0xb8,0xea,0x47,0xa3,0x5c,0x00,0xc3,
    });
    constexpr auto records = std::to_array<std::uint8_t>({
        0xc7,0x0d,0x24,0x00,0xa0,0x05,0xa2,0x05,
        0x1f,0x37,0x20,0x01,0xa0,0x05,0x10,0x2d,
        0x8f,0x1b,0x48,0x00,0xa0,0x05,0x44,0x0b,
        0x8f,0x1b,0x90,0x00,0xa0,0x05,0xa8,0x05,
    });
    constexpr std::string_view entry_span_sha256 =
        "8d412472415d513482b5c70198bb1aa04fa0d25798dd5f4b40b262151c489736";
    constexpr std::string_view record_bank_sha256 =
        "1b92e08f514f6b6dee4683550e2d9363d39e6ed0375ac9c9e2b652754326965f";
    constexpr std::array<std::uint16_t, 4> entry_offsets{{0x90, 0x97, 0x9f, 0xa7}};
    // This order follows the executable paths, not sorted source addresses.
    constexpr std::array<std::uint16_t, 4> source_offsets{{0x70, 0x80, 0x78, 0x88}};
    // 2200AD's mode-value order is 3/4/2/default, whereas the native GX
    // targets execute in 0x90/0x97/0x9f/0xa7 order.
    constexpr std::array<std::uint16_t, 4> selector_source_offsets{{0x70, 0x80, 0x88, 0x78}};
    if (to_hex(sha256(gx_overlay_executable)) != overlay_sha256
        || selector.overlay_prefix_sha256 != "8d412472415d513482b5c70198bb1aa04fa0d25798dd5f4b40b262151c489736"
        || selector.overlay_targets != std::array<std::uint16_t, 4>{{0x000e,0x0012,0x0014,0x000f}}
        || selector.overlay_record_offsets != selector_source_offsets
        || !has_bytes(gx_overlay_executable, first_entry_offset, entry_span)
        || !has_bytes(gx_overlay_executable, 0x70, records)
        || to_hex(sha256(gx_overlay_executable.subspan(first_entry_offset, entry_span.size())))
            != entry_span_sha256
        || to_hex(sha256(gx_overlay_executable.subspan(0x70, records.size())))
            != record_bank_sha256) {
        throw std::runtime_error("Unexpected Millennium DOS GX overlay startup record evidence");
    }
    MillenniumDosGxOverlayStartupRecordEvidence result;
    result.overlay_sha256 = std::string(overlay_sha256);
    result.first_entry_offset = first_entry_offset;
    result.entry_span_byte_count = entry_span.size();
    result.entry_offsets = entry_offsets;
    result.source_record_offsets = source_offsets;
    for (std::size_t index = 0; index < source_offsets.size(); ++index) {
        const auto source = static_cast<std::size_t>(source_offsets[index]);
        std::copy_n(gx_overlay_executable.begin() + static_cast<std::ptrdiff_t>(source), 8,
            result.source_records[index].begin());
    }
    result.shared_copy_entry_offset = 0xb2;
    result.copy_destination_offset = 0x65;
    result.copy_word_count = 4;
    result.copied_last_byte_storage_offset = 0x6d;
    result.state_word_storage_offsets = {0xf4, 0xf0, 0xf2};
    result.terminal_word_storage_offset = 0x5c;
    result.terminal_word_value = 0x47ea;
    result.entry_span_sha256 = std::string(entry_span_sha256);
    result.record_bank_sha256 = std::string(record_bank_sha256);
    return result;
}

MillenniumDosGxOverlayDispatch13Evidence
parse_millennium_dos_gx_overlay_dispatch13_evidence(
    const std::span<const std::uint8_t> gx_overlay_executable,
    const MillenniumDosGxOverlayDispatcherEvidence& dispatcher) {
    // Table slot 13 is a direct in-overlay target. The span ends precisely
    // at its encoded short back edge, rather than pretending that any call
    // result, branch condition, or target routine is understood.
    constexpr std::string_view overlay_sha256 =
        "093f8416de6d23837d2faf82360ef79777c2c2bf146619aafad87626c61ab6fb";
    constexpr std::size_t entry = 0x08d0;
    constexpr auto span = std::to_array<std::uint8_t>({
        0x06,0x57,0x1e,0x56,0xe8,0x2b,0xff,0xe8,0x79,0xfb,0xb9,0x00,0x00,0xba,0x00,0x00,
        0xb8,0x00,0x00,0xc7,0x06,0xf0,0x00,0x00,0x00,0xa3,0xf2,0x00,0xc7,0x06,0xf4,0x00,
        0x00,0x00,0xb8,0x00,0x00,0x8b,0x16,0xf2,0x00,0x8b,0x0e,0xf0,0x00,0xe8,0x54,0x1b,
        0x3c,0x20,0x74,0x07,0xa1,0xf4,0x00,0xa8,0x01,0x74,0x05,0x5e,0x1f,0x5f,0x07,0xc3,
        0xa1,0xf4,0x00,0x8b,0x0e,0xf0,0x00,0x8b,0x16,0xf2,0x00,0x05,0x08,0x00,0x25,0xff,
        0x03,0x83,0xc1,0x02,0x81,0xe1,0xff,0x03,0x83,0xc2,0x04,0x81,0xe2,0xff,0x03,0xa3,
        0xf4,0x00,0x89,0x0e,0xf0,0x00,0x89,0x16,0xf2,0x00,0xe8,0x16,0xfb,0xe8,0x58,0x00,
        0x0e,0x1f,0x8b,0x1e,0xce,0x08,0xd1,0xe3,0x81,0xc3,0x78,0x41,0x8b,0x07,0x8b,0xf0,
        0xe8,0xaa,0xfd,0xe8,0x41,0xfe,0x0e,0x07,0x2e,0xa1,0xf4,0x00,0xbf,0xea,0x47,0xe8,
        0xa4,0xfb,0xeb,0x99,
    });
    constexpr std::string_view span_sha256 =
        "afd0e53d6588f8576da75c48155d63b8f1b2380f02c9d2adfc65a27e78e25ee0";
    if (to_hex(sha256(gx_overlay_executable)) != overlay_sha256
        || dispatcher.observed_selector_targets[13] != entry
        || !has_bytes(gx_overlay_executable, entry, span)
        || to_hex(sha256(gx_overlay_executable.subspan(entry, span.size())) ) != span_sha256) {
        throw std::runtime_error("Unexpected Millennium DOS GX dispatcher slot 13 evidence");
    }
    return {std::string(overlay_sha256), static_cast<std::uint16_t>(entry), span.size(),
        {0x08d4,0x08d7,0x08fd,0x093d,0x0940,0x094f,0x0952},
        {0x0802,0x0453,0x2454,0x0454,0x099b,0x06fc,0x0796},
        {0x00f0,0x00f2,0x00f4}, 0x0900, 0x20, 0x0902, 0x090b,
        0x0907, 0x0909, 0x0910, 0x091b, 0x03ff, 0x0002, 0x0962, 0x08fc,
        std::string(span_sha256)};
}

MillenniumDosEnglishGameStartupCallees
parse_millennium_dos_english_game_startup_callees(
    const std::span<const std::uint8_t> game_executable) {
    // These are the two direct targets selected by the native AL comparison
    // at $d2d9.  The result remains native: this function just preserves the
    // immediate code/data operands that follow either encoded target.
    constexpr std::size_t load_bias = 0x100;
    constexpr std::uint16_t equal_entry = 0xd1a1;
    constexpr std::uint16_t other_entry = 0xd1b5;
    constexpr auto equal = std::to_array<std::uint8_t>({
        0xb8, 0x04, 0x00, 0x0e, 0x07, 0xbb, 0x9f, 0xd1, 0xe8, 0x78,
        0x2f, 0xe8, 0x9f, 0x32, 0xb0, 0x01, 0xa2, 0x05, 0xda, 0xc3,
    });
    constexpr auto other = std::to_array<std::uint8_t>({
        0xb8, 0x04, 0x00, 0x0e, 0x07, 0xbb, 0x9f, 0xd1, 0xe8, 0x64,
        0x2f, 0xe8, 0xa3, 0x32, 0xa0, 0x05, 0xda, 0x3c, 0x02, 0x75,
        0x06, 0xb8, 0x00, 0xb8, 0xa3, 0x07, 0x01, 0xc3,
    });
    constexpr std::string_view executable_sha256 =
        "427574e5f780b2a7b5c4207d167116dc44aea3fb67096fbf12a46c4f544a0a57";
    constexpr std::string_view equal_sha256 =
        "6f59df77c567324b41dd6159a6fbac7d8970626fc40e8b908f9f58746a993a3e";
    constexpr std::string_view other_sha256 =
        "2f61098eb45bb48ea7a38ab2fcc2e065ae0d0b2ad08ea9973e3fe464943fba9b";
    const auto offset = [](const std::uint16_t address) {
        return static_cast<std::size_t>(address) - load_bias;
    };
    if (game_executable.size() != 54'391
        || to_hex(sha256(game_executable)) != executable_sha256
        || !has_bytes(game_executable, offset(equal_entry), equal)
        || !has_bytes(game_executable, offset(other_entry), other)
        || near_call_target(0xd1ac, equal[9], equal[10]) != 0x0124
        || near_call_target(0xd1af, equal[12], equal[13]) != 0x044e
        || near_call_target(0xd1c0, other[9], other[10]) != 0x0124
        || near_call_target(0xd1c3, other[12], other[13]) != 0x0466
        || to_hex(sha256(game_executable.subspan(offset(equal_entry), equal.size()))) != equal_sha256
        || to_hex(sha256(game_executable.subspan(offset(other_entry), other.size()))) != other_sha256) {
        throw std::runtime_error("Unexpected Millennium English DOS startup callees");
    }
    return {std::string(executable_sha256), equal_entry, equal.size(), std::string(equal_sha256),
        0x0004, 0xd19f, 0xd1a9, 0x0124, 0xd1ac, 0x044e, 0x01, 0xda05, 0xd1b4,
        other_entry, other.size(), std::string(other_sha256), 0x0004, 0xd19f,
        0xd1bd, 0x0124, 0xd1c0, 0x0466, 0xda05, 0x02, 0x0107, 0xd1d0};
}

MillenniumDosEnglishGameStartupFollowups
parse_millennium_dos_english_game_startup_followups(
    const std::span<const std::uint8_t> game_executable,
    const MillenniumDosEnglishGameStartupCallees& callees) {
    constexpr std::size_t load_bias = 0x100;
    constexpr std::uint16_t equal_entry = 0x044e;
    constexpr std::uint16_t palette_entry = 0x0466;
    constexpr auto equal = std::to_array<std::uint8_t>({0xb0, 0x01, 0x2e, 0x88, 0x06, 0x05, 0xda, 0xc3});
    constexpr auto palette = std::to_array<std::uint8_t>({
        0x0e, 0x1f, 0xbe, 0x56, 0x04, 0xb9, 0x10, 0x00, 0x32, 0xdb,
        0xac, 0x8a, 0xf8, 0xb8, 0x00, 0x10, 0xcd, 0x10, 0xfe, 0xc3,
        0xe2, 0xf4, 0xc3,
    });
    constexpr std::array<std::uint8_t, 16> palette_table{
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
        0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f,
    };
    constexpr std::string_view executable_sha256 =
        "427574e5f780b2a7b5c4207d167116dc44aea3fb67096fbf12a46c4f544a0a57";
    constexpr std::string_view equal_sha256 =
        "38889279a8b89e0e600bb25298015ccd8aadc09ea3858a1790097b3f7ff4ea8f";
    constexpr std::string_view palette_sha256 =
        "b17db26fa4fa8b7307fb767ff98351bd6dcca202829dd2d9348ff4991942d779";
    constexpr std::string_view palette_table_sha256 =
        "ce46bce999708ea5109a857b0b6ecc02ece34eaf431cd148ef1aa1c0e80aed0a";
    const auto offset = [](const std::uint16_t address) {
        return static_cast<std::size_t>(address) - load_bias;
    };
    if (to_hex(sha256(game_executable)) != executable_sha256
        || callees.executable_sha256 != executable_sha256
        || callees.equal_followup_target_address != equal_entry
        || callees.other_followup_target_address != palette_entry
        || !has_bytes(game_executable, offset(equal_entry), equal)
        || !has_bytes(game_executable, offset(0x0456), palette_table)
        || !has_bytes(game_executable, offset(palette_entry), palette)
        || to_hex(sha256(game_executable.subspan(offset(equal_entry), equal.size()))) != equal_sha256
        || to_hex(sha256(game_executable.subspan(offset(0x0456), palette_table.size()))) != palette_table_sha256
        || to_hex(sha256(game_executable.subspan(offset(palette_entry), palette.size()))) != palette_sha256) {
        throw std::runtime_error("Unexpected Millennium English DOS startup follow-ups");
    }
    return {std::string(executable_sha256), equal_entry, equal.size(), std::string(equal_sha256),
        0x01, 0xda05, 0x0455, palette_entry, palette.size(), std::string(palette_sha256),
        0x0456, palette_table, std::string(palette_table_sha256), 16, 0x10, 0x1000, 0x047c};
}

MillenniumDosSpanishIbmHandoffEvidence parse_millennium_dos_spanish_ibm_handoff_evidence(
    const std::span<const std::uint8_t> ibm_executable,
    const std::span<const std::uint8_t> titles_executable,
    const std::span<const std::uint8_t> game_executable) {
    constexpr auto ibm_sha256 =
        "84b7d158c770117aeaa07cb5ea2e7ed4a6bcc288d6b352d82569ff4d97b2fda9";
    constexpr auto titles_sha256 =
        "02082c35e18cee330f7d1b88098f502e68011f7e47a3a649961f6f03d1d14fe7";
    constexpr auto game_sha256 =
        "9f7d6f28f71eb7f2f6bb48cb3977efbf45049fc74083f8cbc865ec25396330c6";
    constexpr std::size_t load_bias = 0x100;
    constexpr std::uint16_t caller_address = 0x023d;
    constexpr auto caller = std::to_array<std::uint8_t>({
        0xba,0x1d,0x07,0xe8,0xf6,0x00,0x22,0xc0,0x75,0x19,0x0e,0x1f,
        0xba,0x28,0x07,0xe8,0xea,0x00,0x22,0xc0,0x75,0x0d});
    constexpr auto caller_sha256 =
        "6e1cf860908aa88e9427efac371439744c1a10f5bb5fcc7d9588a7f18085cbb7";
    constexpr std::uint16_t names_address = 0x071d;
    constexpr auto names = std::to_array<std::uint8_t>({
        0x54,0x49,0x54,0x4c,0x45,0x53,0x2e,0x45,0x58,0x45,0x00,
        0x32,0x32,0x30,0x30,0x61,0x64,0x2e,0x65,0x78,0x65,0x00});
    constexpr std::uint16_t callee_address = 0x0339;
    constexpr auto callee = std::to_array<std::uint8_t>({
        0x8c,0xc8,0x89,0x06,0x0c,0x07,0x89,0x06,0x10,0x07,0x89,0x06,
        0x14,0x07,0x8e,0xc0,0xbb,0x08,0x07,0x89,0x26,0x84,0x06,0xb8,
        0x00,0x4b,0xcd,0x21,0x8c,0xc9,0x8e,0xd1,0x2e,0x8b,0x26,0x84,
        0x06,0x8e,0xd9,0x8e,0xc1,0x72,0x05,0xb4,0x4d,0xcd,0x21,0xc3});
    constexpr auto callee_sha256 =
        "c2f5b915a0fbbc7a25d8a3f4c0e5fcc97eb197d44048eaff53e2046eb6e7c32c";
    const auto offset = [](const std::uint16_t address) {
        return static_cast<std::size_t>(address) - load_bias;
    };
    if (to_hex(sha256(ibm_executable)) != ibm_sha256
        || to_hex(sha256(titles_executable)) != titles_sha256
        || to_hex(sha256(game_executable)) != game_sha256
        || !has_bytes(ibm_executable, offset(caller_address), caller)
        || !has_bytes(ibm_executable, offset(names_address), names)
        || !has_bytes(ibm_executable, offset(callee_address), callee)
        || to_hex(sha256(ibm_executable.subspan(offset(caller_address), caller.size()))) != caller_sha256
        || to_hex(sha256(ibm_executable.subspan(offset(callee_address), callee.size()))) != callee_sha256) {
        throw std::runtime_error("Unexpected Millennium Spanish IBM.COM handoff evidence");
    }
    // The static callee uses DS:DX inherited from the caller for the filename,
    // and establishes ES:BX = CS:0708 before INT 21h AX=4b00.  Carry after
    // that interrupt is not a host-side result: its taken branch skips the
    // local child-status query and RET, to the next byte after this span.
    return {ibm_sha256, titles_sha256, game_sha256, caller_address, 0x071d, 0x0728,
        0x0240, 0x024c, callee_address, 0x0245, 0x0251, 0x0368,
        0x0708, 0x4b00, 0x21, 0x0362, 0x0369, 0x4d, 0x21,
        caller_sha256, callee_sha256};
}

MillenniumDosSpanishGameStartupEvidence
parse_millennium_dos_spanish_game_startup_evidence(const std::span<const std::uint8_t> game_executable) {
    // This COM image's entry preserves DS/ES, then its near jump lands at
    // $d2cd. The startup establishes SS=CS/SP=$da00, prepares AX=$001f and
    // ES:BX=CS:$d1bb, and calls the private wrapper at the wrapped IP $0124.
    // Its returned AL is compared with $01, selecting one of two local calls.
    // No private interrupt or return value is executed or supplied here.
    constexpr std::string_view executable_sha256 =
        "9f7d6f28f71eb7f2f6bb48cb3977efbf45049fc74083f8cbc865ec25396330c6";
    constexpr std::size_t load_bias = 0x100;
    constexpr std::uint16_t startup_entry_address = 0xd2cd;
    constexpr std::size_t startup_offset = startup_entry_address - load_bias;
    constexpr auto entry = std::to_array<std::uint8_t>({0x0e, 0x1f, 0x0e, 0x07, 0xe9, 0xc6, 0xd1});
    constexpr auto startup = std::to_array<std::uint8_t>({
        0x0e, 0x1f, 0x0e, 0x07, 0x8c, 0xc8, 0x8e, 0xd0, 0xb8, 0x00,
        0xda, 0x89, 0xc4, 0xb8, 0x1f, 0x00, 0x0e, 0x07, 0xbb, 0xbb,
        0xd1, 0xe8, 0x3f, 0x2e, 0x2e, 0xa3, 0x4a, 0xd1, 0x88, 0xe0,
        0x2e, 0xa2, 0x68, 0x43, 0xa2, 0x05, 0xda, 0x89, 0x26, 0x4e,
        0xd1, 0x3c, 0x01, 0x75, 0x05, 0xe8, 0xc1, 0xfe, 0xeb, 0x03,
        0xe8, 0xd0, 0xfe, 0x52, 0x0e, 0x1f, 0xe8, 0x0f, 0xff, 0xa3,
        0x4a, 0xd1, 0x23, 0xd2, 0x74, 0x03, 0xe9, 0xd1, 0x01, 0x2e,
    });
    constexpr std::string_view startup_sha256 =
        "acbfcacc4cfac948944e42181f2fe0dfec11b9ab2c9b79b8aff79d958c5469c6";
    if (game_executable.size() != 54'566 || to_hex(sha256(game_executable)) != executable_sha256
        || !has_bytes(game_executable, 0, entry)
        || !has_bytes(game_executable, startup_offset, startup)
        || near_call_target(0x0107, entry[5], entry[6]) != startup_entry_address
        || near_call_target(0xd2e5, startup[22], startup[23]) != 0x0124
        || near_call_target(0xd2fd, startup[46], startup[47]) != 0xd1be
        || near_call_target(0xd302, startup[51], startup[52]) != 0xd1d2
        || to_hex(sha256(game_executable.subspan(startup_offset, startup.size()))) != startup_sha256) {
        throw std::runtime_error("Unexpected Millennium Spanish DOS game startup evidence");
    }
    return {std::string(executable_sha256), startup_entry_address, startup_entry_address,
        startup.size(), std::string(startup_sha256), 0xda00, 0x001f, 0xd1bb,
        0xd2e2, 0x0124, 0xd14a, 0xd2f6, 0x01, 0xd2fa, 0xd1be, 0xd2ff, 0xd1d2};
}

MillenniumDosSpanishGameStartupCallees
parse_millennium_dos_spanish_game_startup_callees(
    const std::span<const std::uint8_t> game_executable,
    const MillenniumDosSpanishGameStartupEvidence& startup) {
    // Both branch targets set AX=$0004 and ES=CS before retaining their
    // distinct private-wrapper and local-follow-up calls. The equal route
    // writes literal AL=$01 only after both calls return. The other route
    // reads $da05 and conditionally stores $b800 at $0107. Those return and
    // predicate conditions remain native boundaries, not host behavior.
    constexpr std::size_t load_bias = 0x100;
    constexpr std::uint16_t equal_entry = 0xd1be;
    constexpr std::uint16_t other_entry = 0xd1d2;
    constexpr auto equal = std::to_array<std::uint8_t>({
        0xb8, 0x04, 0x00, 0x0e, 0x07, 0xbb, 0xbc, 0xd1, 0xe8, 0x5b,
        0x2f, 0xe8, 0x82, 0x32, 0xb0, 0x01, 0xa2, 0x05, 0xda, 0xc3,
    });
    constexpr auto other = std::to_array<std::uint8_t>({
        0xb8, 0x04, 0x00, 0x0e, 0x07, 0xbb, 0xbc, 0xd1, 0xe8, 0x47,
        0x2f, 0xe8, 0x86, 0x32, 0xa0, 0x05, 0xda, 0x3c, 0x02, 0x75,
        0x06, 0xb8, 0x00, 0xb8, 0xa3, 0x07, 0x01, 0xc3,
    });
    constexpr std::string_view equal_sha256 =
        "fdfc8f02550ee226dea27b1ac0204d1ead083c9d5585e18103bfe67435f0a5bb";
    constexpr std::string_view other_sha256 =
        "6b8180c8f3b01e1f8810b2132756486dc761aee980949643129eeb53f6e86472";
    const auto offset = [](const std::uint16_t address) {
        return static_cast<std::size_t>(address) - load_bias;
    };
    if (startup.executable_sha256 != "9f7d6f28f71eb7f2f6bb48cb3977efbf45049fc74083f8cbc865ec25396330c6"
        || startup.equal_call_target_address != equal_entry
        || startup.other_call_target_address != other_entry
        || !has_bytes(game_executable, offset(equal_entry), equal)
        || !has_bytes(game_executable, offset(other_entry), other)
        || near_call_target(0xd1c9, equal[9], equal[10]) != 0x0124
        || near_call_target(0xd1cc, equal[12], equal[13]) != 0x044e
        || near_call_target(0xd1dd, other[9], other[10]) != 0x0124
        || near_call_target(0xd1e0, other[12], other[13]) != 0x0466
        || to_hex(sha256(game_executable.subspan(offset(equal_entry), equal.size()))) != equal_sha256
        || to_hex(sha256(game_executable.subspan(offset(other_entry), other.size()))) != other_sha256) {
        throw std::runtime_error("Unexpected Millennium Spanish DOS startup callees");
    }
    return {equal_entry, equal.size(), std::string(equal_sha256), 0x0004, 0xd1bc,
        0xd1c6, 0x0124, 0xd1c9, 0x044e, 0x01, 0xda05, 0xd1d1,
        other_entry, other.size(), std::string(other_sha256), 0x0004, 0xd1bc,
        0xd1da, 0x0124, 0xd1dd, 0x0466, 0xda05, 0x02, 0x0107, 0xd1ed};
}

MillenniumDosSpanishGameStartupFollowups
parse_millennium_dos_spanish_game_startup_followups(
    const std::span<const std::uint8_t> game_executable,
    const MillenniumDosSpanishGameStartupCallees& callees) {
    // $044e is local and ends in RET. $0466 initializes CX=$10, reads its
    // in-image table, requests BIOS AX=$1000, increments BL, and has a local
    // LOOP back edge. The BIOS call's register effects remain external, so
    // CX=$10 is an initial value rather than a claimed runtime iteration
    // count. The loop is evidence, not a host palette operation.
    constexpr std::size_t load_bias = 0x100;
    constexpr std::uint16_t equal_entry = 0x044e;
    constexpr std::uint16_t palette_entry = 0x0466;
    constexpr auto equal = std::to_array<std::uint8_t>({0xb0, 0x01, 0x2e, 0x88, 0x06, 0x05, 0xda, 0xc3});
    constexpr auto palette = std::to_array<std::uint8_t>({
        0x0e, 0x1f, 0xbe, 0x56, 0x04, 0xb9, 0x10, 0x00, 0x32, 0xdb,
        0xac, 0x8a, 0xf8, 0xb8, 0x00, 0x10, 0xcd, 0x10, 0xfe, 0xc3,
        0xe2, 0xf4, 0xc3,
    });
    constexpr std::array<std::uint8_t, 16> palette_table{
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
        0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f,
    };
    constexpr std::string_view equal_sha256 =
        "38889279a8b89e0e600bb25298015ccd8aadc09ea3858a1790097b3f7ff4ea8f";
    constexpr std::string_view palette_sha256 =
        "b17db26fa4fa8b7307fb767ff98351bd6dcca202829dd2d9348ff4991942d779";
    constexpr std::string_view palette_table_sha256 =
        "ce46bce999708ea5109a857b0b6ecc02ece34eaf431cd148ef1aa1c0e80aed0a";
    const auto offset = [](const std::uint16_t address) {
        return static_cast<std::size_t>(address) - load_bias;
    };
    constexpr std::string_view executable_sha256 =
        "9f7d6f28f71eb7f2f6bb48cb3977efbf45049fc74083f8cbc865ec25396330c6";
    if (to_hex(sha256(game_executable)) != executable_sha256
        || callees.equal_followup_target_address != equal_entry
        || callees.other_followup_target_address != palette_entry
        || !has_bytes(game_executable, offset(equal_entry), equal)
        || !has_bytes(game_executable, offset(0x0456), palette_table)
        || !has_bytes(game_executable, offset(palette_entry), palette)
        || to_hex(sha256(game_executable.subspan(offset(equal_entry), equal.size()))) != equal_sha256
        || to_hex(sha256(game_executable.subspan(offset(0x0456), palette_table.size()))) != palette_table_sha256
        || to_hex(sha256(game_executable.subspan(offset(palette_entry), palette.size()))) != palette_sha256) {
        throw std::runtime_error("Unexpected Millennium Spanish DOS startup follow-ups");
    }
    return {equal_entry, equal.size(), std::string(equal_sha256), 0x01, 0xda05, 0x0455,
        palette_entry, palette.size(), std::string(palette_sha256), 0x0456, palette_table,
        std::string(palette_table_sha256), 16, 0x10, 0x1000, 0x047c};
}

std::array<MillenniumDosEgaPaletteRegisterWrite, 16>
millennium_dos_startup_ega_palette_register_writes(const MillenniumDosGameFlow& flow) {
    constexpr std::uint8_t bios_video_interrupt = 0x10;
    constexpr std::uint8_t set_palette_register = 0x10;
    constexpr std::uint8_t single_palette_register = 0x00;
    if (flow.startup_other_followup_table_size != flow.startup_other_followup_table_values.size()
        || flow.startup_other_followup_interrupt_number != bios_video_interrupt
        || flow.startup_other_followup_video_function != set_palette_register
        || flow.startup_other_followup_video_subfunction != single_palette_register) {
        throw std::runtime_error("Unsupported Millennium DOS startup video profile");
    }
    std::array<MillenniumDosEgaPaletteRegisterWrite, 16> result{};
    for (std::size_t index = 0; index < result.size(); ++index) {
        result[index] = {
            .register_index = static_cast<std::uint8_t>(index),
            .color_value = flow.startup_other_followup_table_values[index],
        };
    }
    return result;
}

} // namespace eon
