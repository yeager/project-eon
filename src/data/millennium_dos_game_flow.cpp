#include "data/millennium_dos_game_flow.hpp"

#include <algorithm>
#include <array>
#include <stdexcept>

namespace eon {
namespace {

bool has_bytes(std::span<const std::uint8_t> bytes, std::size_t offset,
               std::span<const std::uint8_t> expected) {
    return offset <= bytes.size() && expected.size() <= bytes.size() - offset
        && std::equal(expected.begin(), expected.end(), bytes.begin()
            + static_cast<std::ptrdiff_t>(offset));
}

} // namespace

MillenniumDosGameFlow parse_millennium_dos_game_flow(
    const std::span<const std::uint8_t> game_executable) {
    // 2200AD.EXE is a flat COM-style image loaded at 0x100.  Its startup
    // reaches this loop after initialization; it repeatedly calls 0x10f05,
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
    // loads AL=$02, enters $731a, then repeatedly calls $cafa while the carry
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
        .action_poll_address = 0x10f05,
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
            .repeated_call_address = 0xcafa,
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
