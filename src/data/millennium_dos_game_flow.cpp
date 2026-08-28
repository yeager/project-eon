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
    return {ibm_sha256, titles_sha256, game_sha256, caller_address, 0x071d, 0x0728,
        0x0240, 0x024c, callee_address, 0x0245, 0x0251, 0x0368,
        caller_sha256, callee_sha256};
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
