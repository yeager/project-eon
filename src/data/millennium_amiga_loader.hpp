#pragma once

#include "data/amiga_adf.hpp"

#include <array>
#include <cstdint>
#include <string>

namespace eon {

// The supplied Millennium 2.2 Amiga images carry their game data in raw disk
// ranges.  This is a description of the requests made by the on-disk loader,
// not an extracted or installed representation of the game.
struct MillenniumAmigaLoadStage {
    std::uint32_t disk_offset = 0;
    std::uint32_t length = 0;
    std::uint32_t destination = 0;
    // SHA-256 of precisely the bytes requested from the supplied ADF. This is
    // evidence only; Project Eon never materializes this range as a file.
    std::string raw_sha256;
};

struct MillenniumAmigaLoadPlan {
    MillenniumAmigaLoadStage bootstrap_loader;
    MillenniumAmigaLoadStage first_stage;
    MillenniumAmigaLoadStage resident_stage;
    std::uint32_t resident_entry = 0;
    std::uint32_t loader_magic = 0;
};

// The resident range begins with a small 68000 entry gate.  It calls into the
// first loaded RAM stage and commits its returned word to a fixed location.
// This describes only the literal gate; the called stage is transformed by
// the preceding loader and is intentionally not guessed from raw disk bytes.
struct MillenniumAmigaResidentEntry {
    std::uint32_t entry_address = 0;
    std::uint32_t initializer_address = 0;
    std::uint32_t result_word_address = 0;
    std::uint16_t d3_nonzero_or_mask = 0;
};

// The next complete subroutine in the raw resident range is a literal
// sign/magnitude splitter.  It reads three words from A1+$36, writes their
// low fifteen bits and former high bits to fixed RAM locations, then calls a
// resident helper.  Names here describe data movement only, not gameplay
// meaning.  Project Eon does not execute this routine.
struct MillenniumAmigaResidentWordSplitter {
    std::uint32_t entry_address = 0;
    std::uint16_t source_a1_offset = 0;
    std::array<std::uint32_t, 3> magnitude_word_addresses{};
    std::array<std::uint32_t, 3> sign_byte_addresses{};
    std::uint32_t helper_address = 0;
    std::uint32_t signed_word_address = 0;
    std::uint32_t signed_sign_address = 0;
};

// Exact observable state immediately before the splitter's JSR $7ba12.  This
// is intentionally not presented as the subroutine's final result: the helper
// is not yet independently decoded, and may alter the fixed output locations.
struct MillenniumAmigaResidentWordSplitterPreHelperState {
    std::array<std::uint16_t, 3> magnitude_words{};
    std::array<std::uint8_t, 3> sign_bytes{};
};

// Chain-of-custody evidence for the location named by the splitter's JSR.
// The raw ADF is mapped directly at `resident_stage.destination` for the
// purpose of locating its source bytes, but those bytes have not established
// a complete executable helper boundary. This record intentionally exposes
// only the literal mapping and a small immutable fingerprint; it must not be
// used as an implementation of the helper.
struct MillenniumAmigaResidentHelperRawBoundary {
    std::uint32_t helper_address = 0;
    std::uint32_t raw_disk_offset = 0;
    std::array<std::uint8_t, 32> raw_prefix{};
    std::string raw_prefix_sha256;
};

// The staging callsites invoke a second, earlier resident target before the
// unrecovered helper.  Like the latter target, its bytes are only a linear
// raw-media correspondence; this record makes that distinction explicit.
struct MillenniumAmigaResidentSetupHelperRawBoundary {
    std::uint32_t helper_address = 0;
    std::uint32_t raw_disk_offset = 0;
    std::array<std::uint8_t, 32> raw_prefix{};
    std::string raw_prefix_sha256;
};

// Two further raw-resident routines stage the helper-visible fixed RAM fields
// before directly calling $7ba12.  Their source values remain runtime RAM and
// the helper itself has no validated executable representation, so this is
// preservation evidence only rather than an implementation of either call.
struct MillenniumAmigaResidentHelperStagingCallsite {
    std::uint32_t entry_address = 0;
    std::uint32_t source_address = 0;
    std::uint32_t magnitude_destination = 0;
    std::uint32_t sign_destination = 0;
    std::uint32_t setup_helper_address = 0;
    std::uint32_t clear_byte_address = 0;
    std::uint32_t helper_address = 0;
    // Static instruction address immediately after JSR $7ba12, plus the two
    // literal absolute operands beginning there. This is not a claim that the
    // unrecovered helper returns at runtime.
    std::uint32_t post_helper_return_address = 0;
    std::uint32_t post_helper_source_address = 0;
    std::uint32_t post_helper_magnitude_address = 0;
};

// The exact six source values transferred by a validated staging callsite
// immediately before its JSR $7b77e.  This is deliberately *before* that JSR:
// its target may alter RAM, so no state immediately before JSR $7ba12 can be
// claimed yet.  The transform is pure in-memory copying only.
struct MillenniumAmigaResidentHelperStagingPreSetupState {
    std::array<std::uint16_t, 3> magnitude_words{};
    std::array<std::uint8_t, 3> sign_bytes{};
};

// A longer static continuation anchor after the first staging caller's final
// JSR $7ba12. Its hash and two trailing JSR encodings are raw ADF evidence
// only; it must not be read as proof that $7ba12 returns or that either later
// call executes.
struct MillenniumAmigaResidentFirstPostHelperStaticChain {
    std::uint32_t staging_entry_address = 0;
    std::uint32_t static_start_address = 0;
    std::uint32_t raw_disk_offset = 0;
    std::uint32_t byte_count = 0;
    std::string sha256;
    std::uint32_t next_setup_call_address = 0;
    std::uint32_t next_setup_target = 0;
    std::uint32_t following_call_address = 0;
    std::uint32_t following_target = 0;
};

// Static continuation anchor after the second staging caller's JSR $7ba12.
// It is independent from the first caller: its own shorter original byte
// range reaches a different literal JSR. No helper-return or execution claim
// is represented by this evidence.
struct MillenniumAmigaResidentSecondPostHelperStaticChain {
    std::uint32_t staging_entry_address = 0;
    std::uint32_t static_start_address = 0;
    std::uint32_t raw_disk_offset = 0;
    std::uint32_t byte_count = 0;
    std::string sha256;
    std::uint32_t static_call_address = 0;
    std::uint32_t static_call_target = 0;
};

// Direct absolute, PC-relative BSR.W, and locally-resolved address-register
// reachability evidence for the two staging entries. A zero count proves only
// that this raw resident range contains none of those literal forms; indirect,
// transformed, or runtime paths remain explicitly unproven.
struct MillenniumAmigaResidentStagingDirectReachabilityBoundary {
    std::array<std::uint32_t, 2> staging_entry_addresses{};
    std::array<std::uint32_t, 2> absolute_jsr_counts{};
    std::array<std::uint32_t, 2> absolute_jmp_counts{};
    std::array<std::uint32_t, 2> pc_relative_bsr_word_counts{};
    std::array<std::uint32_t, 2> local_immediate_register_jsr_counts{};
    std::array<std::uint32_t, 2> local_immediate_register_jmp_counts{};
    std::uint32_t scanned_raw_disk_offset = 0;
    std::uint32_t scanned_byte_count = 0;
};

// The first independent raw-resident gate after the word splitter. Its callee
// remains a raw-media-only correspondence, so this records only the literal
// D3-controlled return/continuation split and an immutable target fingerprint.
struct MillenniumAmigaResidentPredicateGate {
    std::uint32_t entry_address = 0;
    std::uint32_t predicate_address = 0;
    std::uint32_t nonzero_return_address = 0;
    std::uint32_t zero_continue_address = 0;
    std::uint32_t predicate_raw_disk_offset = 0;
    std::array<std::uint8_t, 32> predicate_raw_prefix{};
    std::string predicate_raw_prefix_sha256;
};

// The zero-D3 fallthrough from the predicate gate has a complete local
// selector/branch sequence before it prepares one word argument and reaches
// its next unknown call. This is a static boundary only.
struct MillenniumAmigaResidentPredicateZeroPathBoundary {
    std::uint32_t entry_address = 0;
    std::uint16_t selector_a1_offset = 0;
    std::uint16_t selector_compare_value = 0;
    std::uint32_t selector_not_equal_branch_address = 0;
    std::uint32_t selector_not_equal_target = 0;
    std::uint16_t equal_path_argument_a1_offset = 0;
    std::uint32_t unknown_call_address = 0;
    std::uint32_t unknown_call_target = 0;
    std::uint32_t unknown_call_raw_disk_offset = 0;
    std::array<std::uint8_t, 32> unknown_call_raw_prefix{};
    std::string unknown_call_raw_prefix_sha256;
};

// The not-equal target has its own two-register argument setup before reaching
// the same unknown resident call. It records no post-call behavior.
struct MillenniumAmigaResidentPredicateNotEqualPathBoundary {
    std::uint32_t entry_address = 0;
    std::uint32_t pushed_first_register = 0;
    std::uint32_t pushed_second_register = 0;
    std::uint32_t unknown_call_address = 0;
    std::uint32_t unknown_call_target = 0;
};

// Independent raw-resident entry whose first two predicates are fully local.
// It stops before any later branch body or call is assigned meaning.
struct MillenniumAmigaResidentIndependentEntryGate {
    std::uint32_t entry_address = 0;
    std::uint32_t negative_d3_branch_address = 0;
    std::uint32_t negative_d3_target = 0;
    std::uint32_t flag_test_address = 0;
    std::uint32_t flag_address = 0;
    std::uint32_t flag_zero_branch_address = 0;
    std::uint32_t flag_zero_target = 0;
};

// Recovers the explicit raw-read requests from the first-stage 68000 loader.
// It validates the instruction sequence and every resulting disk range.  It
// intentionally does not decompress, write, or otherwise unpack game media.
[[nodiscard]] MillenniumAmigaLoadPlan parse_millennium_amiga_load_plan(
    const AmigaAdf& disk);

// Decode the exact first resident instructions after the verified raw-loader
// handoff. This is a read-only control-flow profile, not a decoder for the
// transformed first-stage RAM image.
[[nodiscard]] MillenniumAmigaResidentEntry parse_millennium_amiga_resident_entry(
    const AmigaAdf& disk, const MillenniumAmigaLoadPlan& plan);

// Validates the next, independent resident subroutine after the entry gate.
// It is a read-only byte profile of the supplied raw ADF; no transformed first
// stage, disk extraction, or 68000 execution is involved.
[[nodiscard]] MillenniumAmigaResidentWordSplitter parse_millennium_amiga_resident_word_splitter(
    const AmigaAdf& disk, const MillenniumAmigaLoadPlan& plan);

// Models only the three LSL/ROXL/LSR iterations proven by the validated raw
// routine. `source_words` are the big-endian words already resident at
// A1+$36..+$3b. It performs no disk I/O and cannot stand in for the later
// helper call or the routine's return value.
[[nodiscard]] MillenniumAmigaResidentWordSplitterPreHelperState
split_millennium_amiga_resident_words_pre_helper(
    const std::array<std::uint16_t, 3>& source_words);

// Locates and fingerprints the raw media bytes corresponding to the
// splitter's `$7ba12` target. It fails closed unless the source image has the
// verified shared prefix. It neither executes, transforms, nor assigns a
// routine boundary to those bytes.
[[nodiscard]] MillenniumAmigaResidentHelperRawBoundary
parse_millennium_amiga_resident_helper_raw_boundary(
    const AmigaAdf& disk, const MillenniumAmigaLoadPlan& plan,
    const MillenniumAmigaResidentWordSplitter& splitter);

// Locates and fingerprints the raw media bytes linearly corresponding to the
// `$7b77e` setup target. It never treats the bytes as executable code.
[[nodiscard]] MillenniumAmigaResidentSetupHelperRawBoundary
parse_millennium_amiga_resident_setup_helper_raw_boundary(
    const AmigaAdf& disk, const MillenniumAmigaLoadPlan& plan);

// Validates the two additional literal staging callsites for $7ba12. Each
// copies three words and three bytes from a runtime source into the same fixed
// fields used by the splitter, calls $7b77e, clears $7b14e, then calls the
// unimplemented helper. It also records the static bytes immediately after
// that final JSR without claiming a runtime return. It never reads source
// values or invokes either call.
[[nodiscard]] std::array<MillenniumAmigaResidentHelperStagingCallsite, 2>
parse_millennium_amiga_resident_helper_staging_callsites(
    const AmigaAdf& disk, const MillenniumAmigaLoadPlan& plan,
    const MillenniumAmigaResidentWordSplitter& splitter);

// Models the six literal `(A4)+` to `(A5)+` copies at either verified staging
// callsite. It cannot execute `$7b77e`, clear `$7b14e`, or call `$7ba12`.
[[nodiscard]] MillenniumAmigaResidentHelperStagingPreSetupState
stage_millennium_amiga_resident_helper_pre_setup(
    const std::array<std::uint16_t, 3>& source_words,
    const std::array<std::uint8_t, 3>& source_sign_bytes);

// Fingerprints the static 86-byte continuation after the first caller's JSR
// $7ba12 and validates two later literal JSR encodings in that range. It
// performs no helper-return, helper, or call execution.
[[nodiscard]] MillenniumAmigaResidentFirstPostHelperStaticChain
parse_millennium_amiga_resident_first_post_helper_static_chain(
    const AmigaAdf& disk, const MillenniumAmigaLoadPlan& plan,
    const MillenniumAmigaResidentHelperStagingCallsite& callsite);

// Fingerprints the second caller's independent 44-byte static continuation
// and its final literal JSR. It never executes or assumes a return from any
// helper/call involved.
[[nodiscard]] MillenniumAmigaResidentSecondPostHelperStaticChain
parse_millennium_amiga_resident_second_post_helper_static_chain(
    const AmigaAdf& disk, const MillenniumAmigaLoadPlan& plan,
    const MillenniumAmigaResidentHelperStagingCallsite& callsite);

// Scans the supplied resident raw range for exact absolute JSR/JMP,
// PC-relative BSR.W, and immediate-MOVEA followed by JSR/JMP (An) encodings
// to the two known staging entry addresses. It rejects a match but makes no
// claim about other reachability forms.
[[nodiscard]] MillenniumAmigaResidentStagingDirectReachabilityBoundary
parse_millennium_amiga_resident_staging_direct_reachability_boundary(
    const AmigaAdf& disk, const MillenniumAmigaLoadPlan& plan,
    const std::array<MillenniumAmigaResidentHelperStagingCallsite, 2>& callsites);

// Validates the literal call/test/return gate immediately after the splitter
// and fingerprints its in-range raw target. It never executes the target or
// its zero-D3 continuation body.
[[nodiscard]] MillenniumAmigaResidentPredicateGate
parse_millennium_amiga_resident_predicate_gate(
    const AmigaAdf& disk, const MillenniumAmigaLoadPlan& plan,
    const MillenniumAmigaResidentWordSplitter& splitter);

// Validates the zero-D3 local selector branch after the predicate gate and
// fingerprints its next unknown call target. It neither supplies A1 data nor
// enters the branch target or call.
[[nodiscard]] MillenniumAmigaResidentPredicateZeroPathBoundary
parse_millennium_amiga_resident_predicate_zero_path_boundary(
    const AmigaAdf& disk, const MillenniumAmigaLoadPlan& plan,
    const MillenniumAmigaResidentPredicateGate& gate);

// Validates the BNE target's two literal stack arguments through its next
// unknown call. It never models a call return or later loop instructions.
[[nodiscard]] MillenniumAmigaResidentPredicateNotEqualPathBoundary
parse_millennium_amiga_resident_predicate_not_equal_path_boundary(
    const AmigaAdf& disk, const MillenniumAmigaLoadPlan& plan,
    const MillenniumAmigaResidentPredicateZeroPathBoundary& zero_path);

// Validates the initial D3-negative and fixed-flag-zero branches at the
// independent resident entry $68508. It does not enter either target.
[[nodiscard]] MillenniumAmigaResidentIndependentEntryGate
parse_millennium_amiga_resident_independent_entry_gate(
    const AmigaAdf& disk, const MillenniumAmigaLoadPlan& plan);

} // namespace eon
