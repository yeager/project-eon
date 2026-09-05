#pragma once

#include "data/amiga_adf.hpp"

#include <array>
#include <cstdint>
#include <span>
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

// The recovered bootstrap loader reaches both raw stages through A3.  This
// is a byte-exact description of the caller-side handoff only: the first
// stage is opaque and neither its return nor the terminal resident jump is
// executed by Project Eon.
struct MillenniumAmigaBootstrapOpaqueInvocationBoundary {
    std::uint32_t entry_address = 0;
    std::size_t raw_disk_offset = 0;
    std::size_t byte_count = 0;
    std::string sha256;
    std::uint32_t first_stage_invocation_address = 0;
    std::uint32_t first_stage_target = 0;
    std::uint32_t static_post_first_stage_address = 0;
    std::uint32_t resident_stage_jump_address = 0;
    std::uint32_t resident_stage_target = 0;
};

// Exact caller-connected entry of the corrected first trackdisk transfer.
// The initial BRA reaches a register-save/exception-vector setup and then an
// ILLEGAL instruction. The exception outcome is deliberately left external;
// no later bytes are called code until that boundary is recovered.
struct MillenniumAmigaFirstStageEntryBoundary {
    std::size_t raw_disk_offset = 0;
    std::size_t byte_count = 0;
    std::uint32_t destination = 0;
    std::string source_sha256;
    std::size_t entry_span_byte_count = 0;
    std::string entry_span_sha256;
    std::uint32_t branch_target = 0;
    std::uint32_t illegal_instruction_address = 0;
    std::uint32_t exception_vector_address = 0;
};

[[nodiscard]] MillenniumAmigaFirstStageEntryBoundary
parse_millennium_amiga_first_stage_entry_boundary(
    const AmigaAdf&, const MillenniumAmigaLoadPlan&);

// The bootstrap block starts at $70000, then contains a byte-copy relocator
// for its later loader continuation. The original DBRA count copies one byte
// beyond the preceding, independently recovered $400-byte boot I/O request.
// This record makes that mismatch explicit. It is a fail-closed preservation
// boundary, not permission to fabricate the missing byte, materialize the
// relocated loader, or call the later continuation at its apparent RAM PC.
struct MillenniumAmigaBootstrapRelocationBoundary {
    std::uint32_t entry_address = 0;
    std::uint32_t verified_loaded_start = 0;
    std::uint32_t verified_loaded_end_exclusive = 0;
    std::uint32_t copy_source_address = 0;
    std::uint32_t copy_destination_address = 0;
    std::uint32_t copy_byte_count = 0;
    std::uint32_t copy_source_end_inclusive = 0;
    std::uint32_t relocated_continuation_address = 0;
    std::uint32_t raw_continuation_source_address = 0;
    std::size_t raw_disk_offset = 0;
    std::uint32_t byte_count = 0;
    std::string sha256;
};

// Immutable source-only anchors inside the first range read by the bootstrap.
// The JSR (A3) representation has no recovered source-to-RAM transform, so
// this is deliberately a provenance record rather than decoded executable
// code, an AmigaOS API model, or an input-map implementation.
struct MillenniumAmigaFirstStageSourceAnchorBoundary {
    std::size_t raw_disk_offset = 0;
    std::size_t byte_count = 0;
    std::string sha256;
    std::array<std::uint32_t, 3> anchor_stage_offsets{};
    std::array<std::size_t, 2> window_stage_offsets{};
    std::array<std::size_t, 2> window_byte_counts{};
    std::array<std::string, 2> window_sha256{};
};

[[nodiscard]] MillenniumAmigaFirstStageSourceAnchorBoundary
parse_millennium_amiga_first_stage_source_anchor_boundary(
    const AmigaAdf&, const MillenniumAmigaLoadPlan&);

// Several supplied Amiga variants differ in their bootstrap and opaque
// first-stage raw representation but share this literal resident raw range. It is independently
// hash-validated so callers can preserve common evidence without treating a
// Defjam-specific load plan as proof for another release.
struct MillenniumAmigaSharedResidentLayout {
    std::uint32_t disk_offset = 0;
    std::uint32_t length = 0;
    std::uint32_t destination = 0;
    std::string raw_sha256;
};

// The resident range begins with a small 68000 entry gate.  It calls into the
// first loaded RAM stage and commits its returned word to a fixed location.
// This describes only the literal gate; the mapping from the preceding raw
// stage invocation is unknown and intentionally not guessed from disk bytes.
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
struct MillenniumAmigaResidentNegativeD3Continuation {
    std::uint32_t entry_address = 0;
    std::uint32_t external_jump_address = 0;
    std::uint32_t external_jump_target = 0;
    std::uint32_t return_address = 0;
};

// The two branch-only tail instructions of the negative-D3 continuation.
// This records their encoded immediate words and terminal RTS, not their
// runtime predicates or register effects.
struct MillenniumAmigaResidentNegativeD3Terminal {
    std::uint32_t entry_address = 0;
    std::uint16_t first_add_immediate = 0;
    std::uint32_t second_add_address = 0;
    std::uint16_t second_add_immediate = 0;
    std::uint32_t return_address = 0;
};

// A complete local sequence immediately after the negative-D3 terminal.
// The first BPL target is intentionally left as a raw stopping boundary.
struct MillenniumAmigaResidentPostNegativeD3Terminal {
    std::uint32_t entry_address = 0;
    std::array<std::uint32_t, 2> absolute_byte_store_addresses{};
    std::uint32_t copied_d1_address = 0;
    std::uint32_t copied_d2_address = 0;
    std::uint32_t d0_test_address = 0;
    std::uint32_t nonzero_branch_address = 0;
    std::uint32_t nonzero_branch_target = 0;
    std::uint32_t zero_return_address = 0;
    std::uint32_t nonnegative_branch_address = 0;
    std::uint32_t nonnegative_branch_target = 0;
    std::uint32_t negative_return_address = 0;
    std::string raw_sha256;
};

// The complete, call-free prefix at $685fe has no hidden memory reads: it
// clears/stores the low byte of D0, transfers the low words of D1 and D2, and
// takes one of two RTS instructions or arrives at the next separately checked
// boundary.  These types make that bounded operation available without
// pretending that an original caller, stack, or following routine is known.
struct MillenniumAmigaResidentPostNegativeD3TerminalInput {
    std::uint32_t d0 = 0;
    std::uint32_t d1 = 0;
    std::uint32_t d2 = 0;
};

enum class MillenniumAmigaResidentPostNegativeD3TerminalStop {
    zero_return,
    negative_return,
    nonnegative_continuation_boundary,
};

struct MillenniumAmigaResidentPostNegativeD3TerminalExecution {
    std::uint32_t d0 = 0;
    std::uint32_t d1 = 0;
    std::uint32_t d2 = 0;
    std::array<std::uint8_t, 2> absolute_byte_writes{};
    MillenniumAmigaResidentPostNegativeD3TerminalStop stop =
        MillenniumAmigaResidentPostNegativeD3TerminalStop::zero_return;
    std::uint32_t next_address = 0;
};

// The BPL target from the immediately preceding local terminal is itself a
// complete static selector prefix.  It has two unresolved local branches and
// a terminal external JMP, so this deliberately records encodings and raw
// provenance only; it never selects a route, restores registers, or enters
// the jump target.
struct MillenniumAmigaResidentPostNegativeD3ContinuationBoundary {
    std::uint32_t entry_address = 0;
    std::array<std::uint16_t, 3> add_immediates{};
    std::uint16_t range_base_immediate = 0;
    std::uint32_t compare_branch_address = 0;
    std::uint32_t compare_branch_target = 0;
    std::uint32_t low_range_branch_address = 0;
    std::uint32_t low_range_branch_target = 0;
    std::uint32_t negative_range_branch_address = 0;
    std::uint32_t negative_range_branch_target = 0;
    std::uint32_t terminal_jump_address = 0;
    std::uint32_t terminal_jump_target = 0;
    std::size_t raw_disk_offset = 0;
    std::uint32_t byte_count = 0;
    std::string raw_sha256;
};

// $6861a is reached only after the preceding bounded terminal takes BPL. It
// has no calls and balances its MOVEM save before the terminal external JMP.
// The model exposes only its literal word arithmetic and where it leaves the
// verified local bytes; it neither invents a stack nor follows an external
// target or either branch beyond this span.
struct MillenniumAmigaResidentPostNegativeD3ContinuationInput {
    std::uint32_t d0 = 0;
    std::uint32_t d1 = 0;
    std::uint32_t d2 = 0;
    std::uint32_t d3 = 0;
    std::uint32_t d6 = 0;
    std::uint32_t d7 = 0;
    std::uint32_t a5 = 0;
};

enum class MillenniumAmigaResidentPostNegativeD3ContinuationStop {
    low_range_branch_boundary,
    negative_range_branch_boundary,
    external_jump_boundary,
};

struct MillenniumAmigaResidentPostNegativeD3ContinuationExecution {
    // Values immediately before a local branch leaves the span or before the
    // balanced restore. `restored_registers` is D0,D1,D2,D3,A5 on the sole
    // external-jump route; no host stack is represented.
    std::uint32_t d0 = 0;
    std::uint32_t d1 = 0;
    std::uint32_t d2 = 0;
    std::uint32_t d3 = 0;
    std::uint32_t d6 = 0;
    std::uint32_t d7 = 0;
    std::uint32_t a5 = 0;
    std::array<std::uint32_t, 5> restored_registers{};
    MillenniumAmigaResidentPostNegativeD3ContinuationStop stop =
        MillenniumAmigaResidentPostNegativeD3ContinuationStop::external_jump_boundary;
    std::uint32_t next_address = 0;
};

struct MillenniumAmigaResidentIndependentZeroTargetBoundary {
    std::uint32_t entry_address = 0;
    std::uint16_t compare_immediate = 0;
    std::uint32_t conditional_branch_address = 0;
    std::uint32_t conditional_branch_target = 0;
};

struct MillenniumAmigaResidentIndependentCompareTargetBoundary {
    std::uint32_t entry_address = 0;
    std::uint32_t conditional_branch_address = 0;
    std::uint32_t conditional_branch_target = 0;
};
struct MillenniumAmigaResidentIndependentBranchTargetBoundary {
    std::uint32_t entry_address = 0;
    std::uint32_t conditional_branch_address = 0;
    std::uint32_t conditional_branch_target = 0;
};
struct MillenniumAmigaResidentIndependentBranchPreparationBoundary {
    std::uint32_t entry_address = 0;
    std::uint32_t unknown_call_address = 0;
    std::uint32_t unknown_call_target = 0;
};

// The caller-side bytes immediately after the independent preparation's
// unknown JSR are a complete call-free raw tail. It is deliberately static
// evidence only: the JSR may not return and the tail reads live absolute
// cells, so this does not model registers, memory, or either return path.
struct MillenniumAmigaResidentIndependentPostCallTailBoundary {
    std::uint32_t entry_address = 0;
    std::size_t raw_disk_offset = 0;
    std::size_t byte_count = 0;
    std::string sha256;
    std::array<std::uint32_t, 6> absolute_byte_addresses{};
    std::uint32_t external_jump_address = 0;
    std::uint32_t external_jump_target = 0;
    std::uint32_t negative_path_address = 0;
    std::uint32_t negative_path_return_address = 0;
    std::uint32_t nonnegative_return_address = 0;
};
struct MillenniumAmigaResidentSeparateEntryGate {
    std::uint32_t entry_address = 0;
    std::uint32_t branch_address = 0;
    std::uint32_t branch_target = 0;
};
struct MillenniumAmigaResidentSeparateBranchBoundary { std::uint32_t entry_address = 0; std::uint32_t long_branch_address = 0; std::uint32_t long_branch_target = 0; std::uint32_t unknown_call_address = 0; std::uint32_t unknown_call_target = 0; };
struct MillenniumAmigaResidentSeparatePostCallBoundary {
    std::uint32_t entry_address = 0;
    std::size_t raw_disk_offset = 0;
    std::string sha256;
    std::uint16_t d0_immediate = 0;
    std::uint32_t a5_source_address = 0;
    std::uint32_t stored_d0_address = 0;
    std::uint32_t following_call_address = 0;
    std::uint32_t following_call_target = 0;
    std::size_t following_target_raw_disk_offset = 0;
    std::string following_target_prefix_sha256;
};

// If the preceding unknown JSR and the post-call JSR both return, the next
// six absolute JSR instructions are fixed raw bytes. Their targets are kept
// as independent ADF correspondences only; neither target execution nor any
// call/return behavior is modeled.
struct MillenniumAmigaResidentSeparatePostCallTailBoundary {
    std::uint32_t entry_address = 0;
    std::size_t raw_disk_offset = 0;
    std::size_t byte_count = 0;
    std::string sha256;
    std::array<std::uint32_t, 6> call_addresses{};
    std::array<std::uint32_t, 6> call_targets{};
    std::array<std::size_t, 6> target_raw_disk_offsets{};
    std::array<std::string, 6> target_prefix_sha256{};
};

// The first local control-flow prefix after the static six-call tail. Its
// branch target and target prefix remain raw evidence: the preceding calls
// may not return and no live RAM value or callee behavior is inferred.
struct MillenniumAmigaResidentSeparatePostCallTailBranchBoundary {
    std::uint32_t entry_address = 0;
    std::size_t raw_disk_offset = 0;
    std::string sha256;
    std::uint32_t compared_byte_address = 0;
    std::uint8_t compare_immediate = 0;
    std::uint32_t conditional_branch_address = 0;
    std::uint32_t conditional_branch_target = 0;
    std::size_t target_raw_disk_offset = 0;
    std::string target_prefix_sha256;
};

// Recovers the explicit raw-read requests from the first-stage 68000 loader.
// It validates the instruction sequence and every resulting disk range.  It
// intentionally does not decompress, write, or otherwise unpack game media.
[[nodiscard]] MillenniumAmigaLoadPlan parse_millennium_amiga_load_plan(
    const AmigaAdf& disk);

// Hash-locks the complete caller-side raw-loader continuation containing the
// indirect first-stage JSR and terminal resident JMP. It proves neither that
// the first stage returns nor that either opaque target is executable as a
// direct linear ADF representation.
[[nodiscard]] MillenniumAmigaBootstrapOpaqueInvocationBoundary
parse_millennium_amiga_bootstrap_opaque_invocation_boundary(
    const AmigaAdf& disk, const MillenniumAmigaLoadPlan& plan);

// Validates the exact relocator before the raw-loader continuation. It proves
// the static source/destination correspondence and fails closed because the
// final DBRA iteration needs $70400 while the boot I/O request has only
// established [$70000, $70400). No relocated bytes are returned.
[[nodiscard]] MillenniumAmigaBootstrapRelocationBoundary
parse_millennium_amiga_bootstrap_relocation_boundary(
    const AmigaAdf& disk, const MillenniumAmigaLoadPlan& plan);

// Validates the common raw resident interval directly from an ADF image byte
// span. This deliberately accepts a shorter supplied image when the complete
// resident range itself is present, since it performs no geometry, boot, or
// loader inference.
[[nodiscard]] MillenniumAmigaSharedResidentLayout
parse_millennium_amiga_shared_resident_layout(std::span<const std::uint8_t> image);

// Decode the exact first resident instructions after the verified raw-loader
// handoff. This is a read-only control-flow profile, not a decoder for the
// opaque first-stage RAM representation.
[[nodiscard]] MillenniumAmigaResidentEntry parse_millennium_amiga_resident_entry(
    const AmigaAdf& disk, const MillenniumAmigaLoadPlan& plan);

// Validates the next, independent resident subroutine after the entry gate.
// It is a read-only byte profile of the supplied raw ADF; no transformed first
// stage mapping, disk extraction, or 68000 execution is involved.
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
[[nodiscard]] MillenniumAmigaResidentNegativeD3Continuation
parse_millennium_amiga_resident_negative_d3_continuation(
    const AmigaAdf& disk, const MillenniumAmigaLoadPlan& plan,
    const MillenniumAmigaResidentIndependentEntryGate& gate);
// Validates the local terminal tail reachable only through unresolved
// negative-D3 continuation predicates. It does not select or execute a path.
[[nodiscard]] MillenniumAmigaResidentNegativeD3Terminal
parse_millennium_amiga_resident_negative_d3_terminal(
    const AmigaAdf& disk, const MillenniumAmigaLoadPlan& plan,
    const MillenniumAmigaResidentNegativeD3Continuation& continuation);
// Validates the complete local sequence after the negative-D3 terminal. It
// records encodings and two local returns only; no register, cell, or branch
// result is interpreted, and the $6861a continuation stays a hard boundary.
[[nodiscard]] MillenniumAmigaResidentPostNegativeD3Terminal
parse_millennium_amiga_resident_post_negative_d3_terminal(
    const AmigaAdf& disk, const MillenniumAmigaLoadPlan& plan,
    const MillenniumAmigaResidentNegativeD3Terminal& terminal);

[[nodiscard]] MillenniumAmigaResidentPostNegativeD3TerminalExecution
execute_millennium_amiga_resident_post_negative_d3_terminal_prefix(
    const MillenniumAmigaResidentPostNegativeD3Terminal& terminal,
    MillenniumAmigaResidentPostNegativeD3TerminalInput input);

[[nodiscard]] MillenniumAmigaResidentPostNegativeD3ContinuationBoundary
parse_millennium_amiga_resident_post_negative_d3_continuation_boundary(
    const AmigaAdf& disk, const MillenniumAmigaLoadPlan& plan,
    const MillenniumAmigaResidentPostNegativeD3Terminal& terminal);

[[nodiscard]] MillenniumAmigaResidentPostNegativeD3ContinuationExecution
execute_millennium_amiga_resident_post_negative_d3_continuation_prefix(
    const MillenniumAmigaResidentPostNegativeD3ContinuationBoundary& boundary,
    MillenniumAmigaResidentPostNegativeD3ContinuationInput input);

// Validates only the first compare/conditional-branch pair at the independent
// entry's fixed-flag-zero target. It does not interpret the comparison.
[[nodiscard]] MillenniumAmigaResidentIndependentZeroTargetBoundary
parse_millennium_amiga_resident_independent_zero_target_boundary(
    const AmigaAdf& disk, const MillenniumAmigaLoadPlan& plan,
    const MillenniumAmigaResidentIndependentEntryGate& gate);

[[nodiscard]] MillenniumAmigaResidentIndependentCompareTargetBoundary
parse_millennium_amiga_resident_independent_compare_target_boundary(
    const AmigaAdf& disk, const MillenniumAmigaLoadPlan& plan,
    const MillenniumAmigaResidentIndependentZeroTargetBoundary& boundary);
[[nodiscard]] MillenniumAmigaResidentIndependentBranchTargetBoundary
parse_millennium_amiga_resident_independent_branch_target_boundary(
    const AmigaAdf& disk, const MillenniumAmigaLoadPlan& plan,
    const MillenniumAmigaResidentIndependentCompareTargetBoundary& boundary);
[[nodiscard]] MillenniumAmigaResidentIndependentBranchPreparationBoundary
parse_millennium_amiga_resident_independent_branch_preparation_boundary(
    const AmigaAdf& disk, const MillenniumAmigaLoadPlan& plan,
    const MillenniumAmigaResidentIndependentBranchTargetBoundary& boundary);
[[nodiscard]] MillenniumAmigaResidentIndependentPostCallTailBoundary
parse_millennium_amiga_resident_independent_post_call_tail_boundary(
    const AmigaAdf& disk, const MillenniumAmigaLoadPlan& plan,
    const MillenniumAmigaResidentIndependentBranchPreparationBoundary& boundary);
[[nodiscard]] MillenniumAmigaResidentSeparateEntryGate
parse_millennium_amiga_resident_separate_entry_gate(
    const AmigaAdf& disk, const MillenniumAmigaLoadPlan& plan);
[[nodiscard]] MillenniumAmigaResidentSeparateBranchBoundary parse_millennium_amiga_resident_separate_branch_boundary(const AmigaAdf&, const MillenniumAmigaLoadPlan&, const MillenniumAmigaResidentSeparateEntryGate&);
[[nodiscard]] MillenniumAmigaResidentSeparatePostCallBoundary parse_millennium_amiga_resident_separate_post_call_boundary(const AmigaAdf&, const MillenniumAmigaLoadPlan&, const MillenniumAmigaResidentSeparateBranchBoundary&);
[[nodiscard]] MillenniumAmigaResidentSeparatePostCallTailBoundary
parse_millennium_amiga_resident_separate_post_call_tail_boundary(
    const AmigaAdf&, const MillenniumAmigaLoadPlan&,
    const MillenniumAmigaResidentSeparatePostCallBoundary&);
[[nodiscard]] MillenniumAmigaResidentSeparatePostCallTailBranchBoundary
parse_millennium_amiga_resident_separate_post_call_tail_branch_boundary(
    const AmigaAdf&, const MillenniumAmigaLoadPlan&,
    const MillenniumAmigaResidentSeparatePostCallTailBoundary&);

// The BNE.W embedded in the prior target reaches this separate, fully local
// pair of D3/D2 comparison branches. Only the literal encodings and their
// resolved local targets are preserved: register values, flags, and purpose
// remain runtime-dependent and unclassified.
struct MillenniumAmigaResidentSeparateComparisonBoundary {
    std::uint32_t entry_address = 0;
    std::size_t raw_disk_offset = 0;
    std::string sha256;
    std::uint32_t preceding_branch_address = 0;
    std::uint32_t preceding_branch_target = 0;
    std::array<std::uint32_t, 4> conditional_branch_addresses{};
    std::array<std::uint32_t, 4> conditional_branch_targets{};
    std::size_t continuation_raw_disk_offset = 0;
    std::string continuation_prefix_sha256;
};

[[nodiscard]] MillenniumAmigaResidentSeparateComparisonBoundary
parse_millennium_amiga_resident_separate_comparison_boundary(
    const AmigaAdf&, const MillenniumAmigaLoadPlan&,
    const MillenniumAmigaResidentSeparatePostCallTailBranchBoundary&);

struct MillenniumAmigaResidentSeparateByteGateBoundary {
    std::uint32_t entry_address = 0;
    std::size_t raw_disk_offset = 0;
    std::string sha256;
    std::uint32_t compared_byte_address = 0;
    std::uint32_t conditional_branch_address = 0;
    std::uint32_t conditional_branch_target = 0;
    std::size_t target_raw_disk_offset = 0;
    std::string target_prefix_sha256;
    std::size_t fallthrough_raw_disk_offset = 0;
    std::string fallthrough_prefix_sha256;
};

[[nodiscard]] MillenniumAmigaResidentSeparateByteGateBoundary
parse_millennium_amiga_resident_separate_byte_gate_boundary(
    const AmigaAdf&, const MillenniumAmigaLoadPlan&,
    const MillenniumAmigaResidentSeparateComparisonBoundary&);

// The taken BEQ.W target is a complete local static prefix.  Its two BCC.W
// encodings and the straight-line instruction converge at the same next raw
// address; runtime flags, cells, and path meaning deliberately remain outside
// this parser.
struct MillenniumAmigaResidentSeparateByteGateTargetBoundary {
    std::uint32_t entry_address = 0;
    std::size_t raw_disk_offset = 0;
    std::string sha256;
    std::array<std::uint32_t, 2> conditional_branch_addresses{};
    std::array<std::uint32_t, 2> conditional_branch_targets{};
    std::uint32_t convergence_address = 0;
    std::size_t convergence_raw_disk_offset = 0;
    std::string convergence_prefix_sha256;
};

[[nodiscard]] MillenniumAmigaResidentSeparateByteGateTargetBoundary
parse_millennium_amiga_resident_separate_byte_gate_target_boundary(
    const AmigaAdf&, const MillenniumAmigaLoadPlan&,
    const MillenniumAmigaResidentSeparateByteGateBoundary&);

struct MillenniumAmigaResidentSeparateByteGateConvergenceBoundary {
    std::uint32_t entry_address = 0;
    std::size_t raw_disk_offset = 0;
    std::string sha256;
    std::uint32_t conditional_branch_address = 0;
    std::uint32_t conditional_branch_target = 0;
    std::size_t target_raw_disk_offset = 0;
    std::string target_prefix_sha256;
    std::size_t fallthrough_raw_disk_offset = 0;
    std::string fallthrough_prefix_sha256;
};

[[nodiscard]] MillenniumAmigaResidentSeparateByteGateConvergenceBoundary
parse_millennium_amiga_resident_separate_byte_gate_convergence_boundary(
    const AmigaAdf&, const MillenniumAmigaLoadPlan&,
    const MillenniumAmigaResidentSeparateByteGateTargetBoundary&);

struct MillenniumAmigaResidentSeparateByteGateTakenBranchBoundary {
    std::uint32_t entry_address = 0;
    std::size_t raw_disk_offset = 0;
    std::string sha256;
    std::array<std::uint32_t, 2> conditional_branch_addresses{};
    std::array<std::uint32_t, 2> conditional_branch_targets{};
    std::uint32_t convergence_address = 0;
    std::uint32_t external_call_address = 0;
    std::uint32_t external_call_target = 0;
    std::size_t external_prefix_raw_disk_offset = 0;
    std::string external_prefix_sha256;
};

[[nodiscard]] MillenniumAmigaResidentSeparateByteGateTakenBranchBoundary
parse_millennium_amiga_resident_separate_byte_gate_taken_branch_boundary(
    const AmigaAdf&, const MillenniumAmigaLoadPlan&,
    const MillenniumAmigaResidentSeparateByteGateConvergenceBoundary&);

struct MillenniumAmigaResidentSeparateByteGateFallthroughBoundary {
    std::uint32_t entry_address = 0;
    std::size_t raw_disk_offset = 0;
    std::string sha256;
    std::uint32_t conditional_branch_address = 0;
    std::uint32_t conditional_branch_target = 0;
    std::uint32_t other_path_entry_address = 0;
    std::string other_path_sha256;
    std::uint32_t other_path_branch_address = 0;
    std::uint32_t other_path_branch_target = 0;
    std::uint32_t convergence_address = 0;
};

[[nodiscard]] MillenniumAmigaResidentSeparateByteGateFallthroughBoundary
parse_millennium_amiga_resident_separate_byte_gate_fallthrough_boundary(
    const AmigaAdf&, const MillenniumAmigaLoadPlan&,
    const MillenniumAmigaResidentSeparateByteGateConvergenceBoundary&);

// This is the fixed raw continuation only if the unknown JSR at $68f48
// returns. It records subsequent call/jump operands and raw target prefixes;
// it does not establish that any call returns or that an absolute cell exists.
struct MillenniumAmigaResidentSeparatePostExternalCallBoundary {
    std::uint32_t entry_address = 0;
    std::size_t raw_disk_offset = 0;
    std::size_t byte_count = 0;
    std::string sha256;
    std::array<std::uint32_t, 3> call_addresses{};
    std::array<std::uint32_t, 3> call_targets{};
    std::array<std::size_t, 3> call_target_raw_disk_offsets{};
    std::array<std::string, 3> call_target_prefix_sha256{};
    std::array<std::uint32_t, 2> address_literals{};
    std::uint32_t terminal_jump_address = 0;
    std::uint32_t terminal_jump_target = 0;
    std::size_t terminal_jump_target_raw_disk_offset = 0;
    std::string terminal_jump_target_prefix_sha256;
};

[[nodiscard]] MillenniumAmigaResidentSeparatePostExternalCallBoundary
parse_millennium_amiga_resident_separate_post_external_call_boundary(
    const AmigaAdf&, const MillenniumAmigaLoadPlan&,
    const MillenniumAmigaResidentSeparateByteGateTakenBranchBoundary&);

// The terminal JMP in the conditional post-external-call byte sequence names
// this raw resident source window.  The loader's transform is not recovered,
// so these bytes are provenance only and must not be decoded as executable
// target instructions.
struct MillenniumAmigaResidentSeparateTerminalJumpRawTargetBoundary {
    std::uint32_t jump_address = 0;
    std::uint32_t target_address = 0;
    std::size_t raw_disk_offset = 0;
    std::size_t byte_count = 0;
    std::string sha256;
};

[[nodiscard]] MillenniumAmigaResidentSeparateTerminalJumpRawTargetBoundary
parse_millennium_amiga_resident_separate_terminal_jump_raw_target_boundary(
    const AmigaAdf&, const MillenniumAmigaLoadPlan&,
    const MillenniumAmigaResidentSeparatePostExternalCallBoundary&);

} // namespace eon
