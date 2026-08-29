#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "data/fat12.hpp"

namespace eon {

// A longword in the combined TEXT+DATA image which GEMDOS asks the loader to
// adjust by its actual load base. The parser retains the value present on the
// original disk; it deliberately does not choose or apply a load address.
struct AtariStRelocation {
    std::uint32_t offset = 0;
    std::uint32_t original_value = 0;
};

// Layout recovered from an Atari ST GEMDOS executable (the 0x601a PRG
// header).  Values are retained as source-media facts; no load address or
// relocation result is invented by the parser.
struct AtariStPrg {
    std::uint32_t text_bytes = 0;
    std::uint32_t data_bytes = 0;
    std::uint32_t bss_bytes = 0;
    std::uint32_t symbol_bytes = 0;
    std::uint32_t flags = 0;
    std::uint16_t absolute_flag = 0;
    std::size_t relocation_count = 0;
    std::uint32_t first_relocation_offset = 0;
    std::uint32_t last_relocation_offset = 0;
    std::vector<AtariStRelocation> relocations;
};

// The first executable control transfer in the verified Equinox Millennium
// PRG. GEMDOS has already performed its ordinary relocation before execution;
// this retains original offsets and never selects a host load address.
struct MillenniumAtariBootstrap {
    std::uint32_t entry_offset = 0;
    std::uint32_t branch_target_offset = 0;
    std::uint32_t stage_source_offset = 0;
    std::uint32_t stage_last_longword_offset = 0;
    std::uint32_t stage_destination_offset = 0;
    std::uint32_t stage_bytes = 0;
};

// The first instruction sequence in the small bootstrap block after it has
// been copied into BSS. These are original absolute machine addresses, not
// host addresses selected by Project Eon.
struct MillenniumAtariBssEntry {
    std::uint32_t entry_offset = 0;
    std::uint32_t copy_source_address = 0;
    std::uint32_t copy_destination_address = 0;
    std::uint16_t initial_d0 = 0;
    std::uint32_t copied_words = 0;
    std::uint32_t jump_address = 0;
};

// Provenance of the exact source range consumed by Millennium's second BSS
// stub.  Its first bytes are the original DATA tail copied by the bootstrap;
// its remaining bytes are the GEMDOS-established zero portion of BSS.  The
// byte vector is an in-memory reconstruction of that loader state only: it is
// neither an unpacked game file nor a writable media image.
struct MillenniumAtariBssSource {
    std::uint32_t load_base = 0;
    std::uint32_t bss_start_address = 0;
    std::uint32_t source_address = 0;
    std::uint32_t source_data_offset = 0;
    std::uint32_t original_data_bytes = 0;
    std::uint32_t bss_zero_bytes = 0;
    std::vector<std::uint8_t> bytes;
};

// The exact 514-byte transfer made by Millennium's next BSS-resident stub.
// It is loader state assembled solely in memory; its leading bytes retain PRG
// DATA provenance and its tail is the GEMDOS-zeroed BSS interval.
struct MillenniumAtariMaterializedTarget {
    std::uint32_t source_address = 0;
    std::uint32_t target_address = 0;
    std::uint16_t first_opcode = 0;
    std::uint16_t first_immediate_word = 0;
    std::uint32_t first_immediate_longword = 0;
    std::vector<std::uint8_t> bytes;
};

// Result of executing the two wholly local 68000 copy loops at the start of
// the verified Equinox loader. This is intentionally not a general 68000 or
// GEMDOS emulator: it performs only fixed, byte-validated transfers and stops
// at the address of the first TRAP #1 instruction.
struct MillenniumAtariBootstrapExecution {
    std::uint32_t initial_pc_offset = 0;
    std::uint32_t branch_pc_offset = 0;
    std::uint32_t first_copy_longwords = 0;
    std::uint32_t bss_entry_address = 0;
    std::uint32_t second_copy_words = 0;
    std::uint32_t target_address = 0;
    std::uint32_t stop_before_trap_address = 0;
    // The three fixed instructions at the materialized target create the
    // Fopen argument frame before the first GEMDOS boundary.  This is a
    // relative, byte-addressed record only: no initial A7 value, host stack,
    // trap invocation, or D0 result is selected by Project Eon.
    std::uint32_t target_prefix_bytes_executed = 0;
    std::int32_t relative_stack_pointer_delta = 0;
    std::vector<std::uint8_t> fopen_frame_bytes;
    std::vector<std::uint8_t> copied_stage_bytes;
    MillenniumAtariMaterializedTarget target;
};

// Executes exactly the two original copy loops whose inputs are fully local:
// the PRG DATA interval into loader BSS, then the proved DATA/BSS source to
// $77000. The established zeroed BSS tail is represented in memory only.
// Execution stops before the first GEMDOS instruction; no service frame or
// return value is created.
[[nodiscard]] MillenniumAtariBootstrapExecution
execute_millennium_atari_bootstrap_prefix(std::span<const std::uint8_t> bytes,
    const AtariStPrg& prg, const MillenniumAtariBootstrap& bootstrap,
    const MillenniumAtariBssEntry& entry);

// The first GEMDOS boundary reached by the verified BSS materialization.
// These are literal stack arguments and control-flow facts from the original
// bytes.  `fopen_result_negative_branch_target_offset` is deliberately an
// offset, not a host behaviour: GEMDOS owns the return value in D0 and Project
// Eon neither invokes the trap nor decides whether the retry loop terminates.
struct MillenniumAtariTrapEntry {
    std::uint32_t target_address = 0;
    std::uint32_t fopen_filename_address = 0;
    std::string fopen_filename;
    std::uint16_t fopen_access_mode = 0;
    std::uint16_t fopen_function = 0;
    std::uint32_t fopen_trap_offset = 0;
    std::uint16_t following_fclose_function = 0;
    std::uint32_t following_fclose_selector_offset = 0;
    std::uint32_t fopen_result_test_offset = 0;
    std::uint32_t fopen_result_negative_branch_offset = 0;
    std::uint32_t fopen_result_negative_branch_target_offset = 0;
};

// The short original-code result gate immediately after Fopen's TRAP #1.
// This is a symbolic local execution record: D0 remains an opaque GEMDOS
// output, so it deliberately exposes both encoded successors rather than
// selecting either an error retry or the Fread fall-through. The saved handle
// word is therefore recorded as a D0 dependency, never as invented stack
// bytes or a host file handle.
struct MillenniumAtariFopenResultGateExecution {
    std::uint32_t target_address = 0;
    std::size_t entry_offset = 0;
    std::size_t byte_count = 0;
    std::string sha256;
    std::uint16_t opaque_handle_push_opcode = 0;
    std::uint16_t fclose_selector_push_opcode = 0;
    std::uint16_t fclose_selector = 0;
    std::uint16_t result_test_opcode = 0;
    std::uint16_t negative_branch_opcode = 0;
    std::int8_t negative_branch_displacement = 0;
    std::size_t negative_successor_offset = 0;
    std::size_t nonnegative_successor_offset = 0;
    std::int32_t relative_stack_pointer_delta = 0;
};

// Symbolically executes only the post-TRAP local instructions that preserve
// an unknown original D0 and form the branch. It requires no GEMDOS result,
// never calls the trap, and returns both possible original-code successors.
[[nodiscard]] MillenniumAtariFopenResultGateExecution
execute_millennium_atari_fopen_result_gate(const MillenniumAtariMaterializedTarget& target,
    const MillenniumAtariTrapEntry& trap);

// The static fall-through after the Fopen negative-result loop prepares one
// further documented GEMDOS interface. This is a byte-level call boundary;
// neither Fopen's D0 result nor the following Fread service is invoked.
struct MillenniumAtariFopenFallthrough {
    std::uint32_t target_address = 0;
    std::size_t entry_offset = 0;
    std::size_t byte_count = 0;
    std::string sha256;
    std::uint32_t fread_buffer_address = 0;
    std::uint32_t fread_byte_count = 0;
    std::uint16_t handle_push_opcode = 0;
    std::uint16_t fread_function = 0;
    std::size_t fread_trap_offset = 0;
    std::uint16_t stack_cleanup_opcode = 0;
    std::uint32_t stack_cleanup_bytes = 0;
};

[[nodiscard]] MillenniumAtariFopenFallthrough parse_millennium_atari_fopen_fallthrough(
    const MillenniumAtariMaterializedTarget& target, const MillenniumAtariTrapEntry& trap);

// Symbolically executes the wholly local Fread argument writes reached by
// Fopen's nonnegative successor.  The handle word is deliberately opaque D0
// output from the prior GEMDOS call; it is represented as a reserved slot,
// not fabricated stack data.  Execution stops immediately before Fread's
// original TRAP #1.
struct MillenniumAtariFreadFramePrefixExecution {
    std::uint32_t target_address = 0;
    std::size_t entry_offset = 0;
    std::size_t byte_count = 0;
    std::string sha256;
    std::uint32_t buffer_address = 0;
    std::uint32_t byte_count_argument = 0;
    std::uint16_t function = 0;
    std::size_t opaque_handle_frame_offset = 0;
    std::size_t opaque_handle_frame_bytes = 0;
    std::int32_t relative_stack_pointer_delta = 0;
    std::size_t stop_before_trap_offset = 0;
};

[[nodiscard]] MillenniumAtariFreadFramePrefixExecution
execute_millennium_atari_fread_frame_prefix(const MillenniumAtariMaterializedTarget& target,
    const MillenniumAtariFopenFallthrough& fallthrough);

// The original continuation immediately after the statically prepared Fread
// boundary. It begins with the still-uninvoked GEMDOS trap and ends at an
// absolute JSR into the Fread destination. This proves a loader-to-buffer
// control transfer encoding only: it does not claim that either GEMDOS call
// returns, that the destination was filled, or that the JSR is reachable.
struct MillenniumAtariFreadConfigTransferBoundary {
    std::uint32_t target_address = 0;
    std::size_t entry_offset = 0;
    std::size_t byte_count = 0;
    std::string sha256;
    std::uint16_t trap_opcode = 0;
    std::uint16_t stack_cleanup_opcode = 0;
    std::uint32_t stack_cleanup_bytes = 0;
    std::uint16_t config_jsr_opcode = 0;
    std::uint32_t config_buffer_address = 0;
};

struct MillenniumAtariConfigEntry;

[[nodiscard]] MillenniumAtariFreadConfigTransferBoundary
parse_millennium_atari_fread_config_transfer_boundary(
    const MillenniumAtariMaterializedTarget& target,
    const MillenniumAtariFopenFallthrough& fallthrough);

// The recovered Fread boundary names $2a500 as its destination, while the
// supplied MILL22A.inf has an absolute leading JMP.  This parser preserves
// the resulting address relationship without treating the file bytes as a
// successfully materialized native buffer.  In particular, it makes the
// 34-byte disagreement between that literal destination mapping and the
// independently verified candidate entry explicit rather than silently
// choosing a host-side load address.
struct MillenniumAtariFreadConfigLoadAddressBoundary {
    std::uint32_t fread_destination_address = 0;
    std::uint16_t payload_initial_jump_opcode = 0;
    std::uint32_t payload_initial_jump_target_address = 0;
    std::uint32_t payload_initial_jump_target_file_offset_from_destination = 0;
    std::string payload_initial_jump_sha256;
    std::uint32_t independent_entry_load_base = 0;
    std::uint32_t independent_entry_file_offset = 0;
    std::int32_t independent_entry_offset_delta = 0;
};

[[nodiscard]] MillenniumAtariFreadConfigLoadAddressBoundary
parse_millennium_atari_fread_config_load_address_boundary(
    const MillenniumAtariFreadConfigTransferBoundary& transfer,
    std::span<const std::uint8_t> payload,
    const MillenniumAtariConfigEntry& independent_entry);

// Conditional original-code evidence at the destination named by the Fread
// boundary.  If native Fread returned with file byte zero at that literal
// address, the file's leading JMP lands at this 34-byte prefix, whose two
// local branches converge at one JSR and then fall through to the separately
// bounded candidate entry.  This does not make Fread return, run the code, or
// assign platform meaning to its SR/register/memory instructions.
struct MillenniumAtariFreadMappedConfigPrelude {
    std::uint32_t fread_destination_address = 0;
    std::uint32_t mapped_entry_address = 0;
    std::uint32_t mapped_entry_file_offset = 0;
    std::uint32_t continuation_address = 0;
    std::size_t byte_count = 0;
    std::string sha256;
    std::uint16_t initial_opcode = 0;
    std::uint16_t conditional_branch_opcode = 0;
    std::uint32_t conditional_branch_target_address = 0;
    std::uint32_t converged_jsr_address = 0;
    std::uint16_t converged_jsr_opcode = 0;
    std::uint32_t converged_jsr_target_address = 0;
};

[[nodiscard]] MillenniumAtariFreadMappedConfigPrelude
parse_millennium_atari_fread_mapped_config_prelude(
    const MillenniumAtariFreadConfigTransferBoundary& transfer,
    std::span<const std::uint8_t> payload,
    const MillenniumAtariConfigEntry& independent_entry);

// Evidence for the exact configuration filename requested by the recovered
// Fopen boundary.  The profile retains only filesystem facts, a whole-payload
// hash and the literal leading instruction word(s); it never projects the
// supplied payload into a host-side model or rewrites it as an inferred
// default.
struct MillenniumAtariConfigEvidence {
    std::string requested_filename;
    std::size_t root_entry_count = 0;
    bool present = false;
    std::uint16_t first_cluster = 0;
    std::uint32_t size = 0;
    std::string sha256;
    std::uint16_t first_word = 0;
    std::uint32_t first_longword_operand = 0;
};

// Complete read-only FAT12 inventory of the verified Equinox one-disk image.
// Each digest is calculated from the original cluster chain. An entry's
// presence does not establish that the game opens, decodes, presents, or
// mutates that file.
struct MillenniumAtariRootFileEvidence {
    std::string name;
    std::uint16_t first_cluster = 0;
    std::uint32_t size = 0;
    std::string sha256;
};

struct MillenniumAtariRootInventory {
    std::vector<MillenniumAtariRootFileEvidence> files;
};

// A hash-identified filename literal in the supplied Equinox auxiliary
// payload. This is provenance only: it does not prove that a native routine
// opens the named file, selects a record, or decodes its contents.
struct MillenniumAtariAuxiliaryResourceNameEvidence {
    std::string container_filename;
    std::uint16_t first_cluster = 0;
    std::uint32_t size = 0;
    std::string sha256;
    std::uint32_t literal_file_offset = 0;
    std::string literal_filename;
    std::uint32_t preceding_return_file_offset = 0;
};

// An independently address-derived candidate control block in the genuine
// Equinox MILL22A.inf payload. The file is not a host configuration format:
// its leading JMP and all reported addresses are original 68000 facts. The
// Fread destination relationship remains an explicit unresolved boundary, so
// this parser never claims the block is dynamically reached, calls traps,
// follows JSRs, or projects mutable words into replacement game state.
struct MillenniumAtariConfigEntry {
    std::uint32_t proven_load_base = 0;
    std::uint32_t entry_address = 0;
    std::uint32_t entry_file_offset = 0;
    std::uint16_t initial_trap_selector = 0;
    std::uint32_t initial_trap_longword_argument = 0;
    std::uint16_t second_trap_selector = 0;
    std::uint32_t second_trap_longword_argument = 0;
    std::vector<std::uint32_t> jsr_targets;
    std::uint32_t final_pea_address = 0;
    std::uint16_t final_trap_selector = 0;
    std::uint32_t return_offset = 0;
};

// The second literal TRAP #14 argument in the verified entry block. Its
// bytes are two NUL-terminated filenames; the service and their consumer are
// intentionally not inferred.
struct MillenniumAtariConfigTrapArgumentStrings {
    std::uint32_t proven_load_base = 0;
    std::uint32_t argument_address = 0;
    std::uint32_t file_offset = 0;
    std::array<std::string, 2> strings;
    std::string sha256;
};

// The first direct JSR destination in the verified MILL22A.inf entry block.
// This is deliberately a control/data boundary rather than an emulated call:
// it retains the original target's exact leading machine words and the
// directly-decodable MOVEM/RTS suffix, but makes no claim about the preceding
// bit-operation's register state or the routine's wider calling convention.
struct MillenniumAtariConfigFirstJsr {
    std::uint32_t proven_load_base = 0;
    std::uint32_t target_address = 0;
    std::uint32_t target_file_offset = 0;
    std::uint16_t leading_opcode = 0;
    std::uint16_t movem_opcode = 0;
    std::uint16_t movem_register_mask = 0;
    std::uint16_t return_opcode = 0;
};

// The second direct JSR destination in the same verified entry block. This
// preserves its immediate-bit opcode, conditional branch and the two literal
// successor call destinations. It intentionally does not attach an effect to
// the intermediate hardware-facing instruction bytes or pick a branch from
// an invented D0 value.
struct MillenniumAtariConfigSecondJsr {
    std::uint32_t proven_load_base = 0;
    std::uint32_t target_address = 0;
    std::uint32_t target_file_offset = 0;
    std::uint16_t initial_opcode = 0;
    std::uint16_t immediate_bit_number = 0;
    std::uint16_t conditional_branch_opcode = 0;
    std::uint32_t conditional_branch_target_address = 0;
    std::uint32_t join_jsr_address = 0;
    std::uint32_t join_jsr_target = 0;
    std::uint32_t following_jsr_target = 0;
};

// The shared JSR target reached from the second config routine. The fields
// retain only the exact original words and absolute slots directly encoded in
// its small body. The Line-A word is intentionally not interpreted: doing so
// would require platform state and an OS/firmware implementation.
struct MillenniumAtariConfigJoinJsr {
    std::uint32_t proven_load_base = 0;
    std::uint32_t target_address = 0;
    std::uint32_t target_file_offset = 0;
    std::uint16_t initial_opcode = 0;
    std::uint16_t d0_word_store_opcode = 0;
    std::uint32_t d0_word_store_address = 0;
    std::uint16_t line_a_opcode = 0;
    std::uint32_t first_longword_store_address = 0;
    std::uint32_t second_longword_store_address = 0;
    std::uint16_t return_opcode = 0;
};

// The repeated $2aa0c JSR target is a strict original forwarding stub. Its
// destination body exposes a trap selector and stack cleanup as byte facts;
// neither is treated as a request to emulate XBIOS or determine its result.
struct MillenniumAtariConfigForwardedJsr {
    std::uint32_t proven_load_base = 0;
    std::uint32_t entry_address = 0;
    std::uint32_t entry_file_offset = 0;
    std::uint16_t jump_opcode = 0;
    std::uint32_t forwarded_address = 0;
    std::uint32_t forwarded_file_offset = 0;
    std::uint16_t initial_opcode = 0;
    std::uint16_t trap_selector = 0;
    std::uint16_t trap_opcode = 0;
    std::uint16_t stack_cleanup_opcode = 0;
    std::uint16_t return_opcode = 0;
};

// The direct $2b2be config target begins with a local D0 gate whose branch
// target is still in original file bytes. Project Eon records the exact
// machine words and branch shape, not the resulting D0-dependent behaviour.
struct MillenniumAtariConfigThirdJsr {
    std::uint32_t proven_load_base = 0;
    std::uint32_t target_address = 0;
    std::uint32_t target_file_offset = 0;
    std::uint16_t initial_opcode = 0;
    std::uint16_t gate_opcode = 0;
    std::uint16_t gate_immediate = 0;
    std::uint16_t branch_opcode = 0;
    std::uint16_t branch_displacement = 0;
    std::uint32_t branch_target_address = 0;
    std::uint16_t branch_target_opcode = 0;
    std::uint16_t branch_target_immediate = 0;
    std::uint16_t branch_target_branch_opcode = 0;
};

// The entire local $2b2be routine is hash-locked as static code evidence.
// Its branches and copies depend on caller-owned registers/pointers, so this
// does not model an execution result, input, graphics, or native-service ABI.
struct MillenniumAtariConfigThirdRoutine {
    std::uint32_t target_address = 0;
    std::uint32_t target_file_offset = 0;
    std::uint32_t terminal_return_address = 0;
    std::size_t byte_count = 0;
    std::string sha256;
};

// The $2b448 direct target has a complete, local register/address setup
// prefix before its first loop body. These are literal 68000 dataflow facts;
// Project Eon does not infer what the pointers or counters represent.
struct MillenniumAtariConfigFourthJsr {
    std::uint32_t proven_load_base = 0;
    std::uint32_t target_address = 0;
    std::uint32_t target_file_offset = 0;
    std::uint16_t d7_setup_opcode = 0;
    std::uint16_t d7_initial_value = 0;
    std::uint32_t a5_initial_address = 0;
    std::uint32_t a4_initial_address = 0;
    std::uint16_t d6_initial_value = 0;
    std::uint16_t d5_initial_value = 0;
    std::uint16_t d4_initial_value = 0;
};

// The contiguous local predecessor immediately before the direct $2b448
// target. It is a hash-addressed byte/control-flow anchor only: no entry
// path to its first byte is established, and neither DBF loop is executed or
// assigned a data effect.
struct MillenniumAtariConfigFourthPrelude {
    std::uint32_t prelude_address = 0;
    std::uint32_t prelude_file_offset = 0;
    std::uint32_t byte_count = 0;
    std::string sha256;
    std::uint16_t d0_setup_opcode = 0;
    std::uint32_t d0_initial_value = 0;
    std::uint16_t d1_setup_opcode = 0;
    std::uint16_t d1_initial_value = 0;
    std::uint16_t first_dbf_opcode = 0;
    std::int16_t first_dbf_displacement = 0;
    std::uint32_t first_dbf_target_address = 0;
    std::uint16_t a3_push_opcode = 0;
    std::uint32_t a5_initial_address = 0;
    std::uint16_t second_d0_setup_opcode = 0;
    std::uint16_t second_d0_initial_value = 0;
    std::uint16_t second_dbf_opcode = 0;
    std::int16_t second_dbf_displacement = 0;
    std::uint32_t second_dbf_target_address = 0;
    std::uint32_t continuation_address = 0;
};

// The first local loop immediately after $2b448's proven setup. This is a
// byte-precise DBF backedge, not a claim about the loop's data or effects.
struct MillenniumAtariConfigFourthLoop {
    std::uint32_t target_address = 0;
    std::uint32_t body_address = 0;
    std::uint32_t body_file_offset = 0;
    std::uint32_t body_bytes = 0;
    std::uint16_t backedge_opcode = 0;
    std::int16_t backedge_displacement = 0;
    std::uint32_t backedge_target_address = 0;
    std::uint16_t setup_d5_value = 0;
};

// The exact six-byte post-inner-loop path. It advances A5 and uses the
// original outer DBF backedge; this records control/dataflow only.
struct MillenniumAtariConfigFourthPostLoop {
    std::uint32_t post_loop_address = 0;
    std::uint32_t post_loop_file_offset = 0;
    std::uint16_t a5_advance_opcode = 0;
    std::uint16_t outer_backedge_opcode = 0;
    std::int16_t outer_backedge_displacement = 0;
    std::uint32_t outer_backedge_target_address = 0;
    std::uint16_t target_setup_opcode = 0;
    std::uint16_t target_setup_immediate = 0;
};

// The exact outer-loop target prefix reached by the post-inner-loop DBF. It
// establishes the two immediate register words immediately before the already
// verified inner loop; no loop execution is implied.
struct MillenniumAtariConfigFourthOuterSetup {
    std::uint32_t setup_address = 0;
    std::uint32_t setup_file_offset = 0;
    std::uint16_t d5_setup_opcode = 0;
    std::uint16_t d5_initial_value = 0;
    std::uint16_t d4_setup_opcode = 0;
    std::uint16_t d4_initial_value = 0;
    std::uint32_t continuation_address = 0;
};

// The original fall-through reached only when the outer DBF does not take its
// backedge. This reports the literal stack arguments at its first TRAP #14
// boundary and never invokes that trap.
struct MillenniumAtariConfigFourthPostOuterBoundary {
    std::uint32_t boundary_address = 0;
    std::uint32_t boundary_file_offset = 0;
    std::uint16_t longword_push_opcode = 0;
    std::uint32_t longword_argument = 0;
    std::uint16_t selector_push_opcode = 0;
    std::uint16_t trap_selector = 0;
    std::uint16_t trap_opcode = 0;
};

// The original bytes immediately following that first post-outer-loop trap.
// Their control effects still depend on the native TRAP #14 return, so this is
// intentionally a hash-addressed preservation anchor rather than an execution
// model or a claim that the subsequent path is dynamically reachable.
struct MillenniumAtariConfigFourthPostOuterTail {
    std::uint32_t tail_address = 0;
    std::uint32_t tail_file_offset = 0;
    std::uint32_t tail_bytes = 0;
    std::string sha256;
    std::uint16_t initial_stack_cleanup_opcode = 0;
    std::uint16_t d0_load_opcode = 0;
    std::uint32_t d0_initial_value = 0;
    std::uint16_t d0_decrement_opcode = 0;
    std::uint16_t d0_nonzero_branch_opcode = 0;
    std::int8_t d0_nonzero_branch_displacement = 0;
    std::uint32_t d0_nonzero_branch_target_address = 0;
    std::uint16_t d7_backedge_opcode = 0;
    std::int16_t d7_backedge_displacement = 0;
    std::uint32_t d7_backedge_target_address = 0;
    std::uint16_t selector_push_opcode = 0;
    std::uint16_t selector = 0;
    std::uint16_t trap_opcode = 0;
    std::uint16_t final_stack_cleanup_opcode = 0;
    std::uint16_t return_opcode = 0;
};

// The literal setup prefix at the D7 DBF backedge target. This ties the
// native-dependent tail to the independently validated loop entry without
// treating either loop as executable.
struct MillenniumAtariConfigFourthPostOuterRecurrence {
    std::uint32_t prefix_address = 0;
    std::uint32_t prefix_file_offset = 0;
    std::uint32_t prefix_bytes = 0;
    std::string sha256;
    std::uint32_t continuation_address = 0;
};

// A byte-level inventory of absolute-JSR encodings in the original config
// payload. These remain deliberately non-reachability facts: only the six
// entry-block callsites have a separately proven execution context.
struct MillenniumAtariConfigAbsoluteJsrInventory {
    std::vector<std::pair<std::uint32_t, std::uint32_t>> encodings;
};

// A hash-identified local span at one of the non-entry-block absolute-JSR
// targets. The inventory proves only that the encoding occurs in the original
// payload; it does not establish reachability or a calling convention. The
// body is retained whole because its register and memory effects depend on
// state outside the static bytes.
struct MillenniumAtariConfigResidualJsrBody {
    std::uint32_t callsite_file_offset = 0;
    std::uint32_t target_address = 0;
    std::uint32_t target_file_offset = 0;
    std::uint32_t terminal_return_address = 0;
    std::uint32_t byte_count = 0;
    std::string sha256;
    std::uint16_t first_opcode = 0;
    std::uint16_t return_opcode = 0;
};

// Strictly parses a genuine Atari ST PRG image, including its compact
// relocation byte stream.  It rejects malformed offsets rather than treating
// a different file as a compatible game executable.
[[nodiscard]] AtariStPrg parse_atari_st_prg(std::span<const std::uint8_t> bytes);

// Validates the exact earliest bootstrap path in Millennium's verified Atari
// ST executable. It never emits a relocated or unpacked image.
[[nodiscard]] MillenniumAtariBootstrap parse_millennium_atari_bootstrap(
    std::span<const std::uint8_t> bytes, const AtariStPrg& prg);

// Validates the next exact BSS-resident stub reached by the verified
// bootstrap. It reports literal MOVE/DBF/JMP facts only; it never creates its
// input buffer, applies PRG relocation, or executes 68000 instructions.
[[nodiscard]] MillenniumAtariBssEntry parse_millennium_atari_bss_entry(
    std::span<const std::uint8_t> bytes, const AtariStPrg& prg,
    const MillenniumAtariBootstrap& bootstrap);

// Materializes only the precise source state requested by the BSS entry's
// first copy: original PRG DATA where the bootstrap proves it was copied, then
// the remaining loader-zeroed BSS. This validates the calculated load base
// and refuses any layout that would make either provenance ambiguous.
[[nodiscard]] MillenniumAtariBssSource materialize_millennium_atari_bss_source(
    std::span<const std::uint8_t> bytes, const AtariStPrg& prg,
    const MillenniumAtariBootstrap& bootstrap, const MillenniumAtariBssEntry& entry);

// Performs the second proven MOVE.W/DBF transfer in memory and validates the
// literal first target instructions from the verified Equinox executable. No
// PRG relocation, disk extraction, write, or 68000 execution is involved.
[[nodiscard]] MillenniumAtariMaterializedTarget materialize_millennium_atari_target(
    const MillenniumAtariBssSource& source, const MillenniumAtariBssEntry& entry);

// Validates the original Fopen trap and immediate post-trap setup after the
// materialized jump. It makes no GEMDOS call or OS emulation; the parser only
// reports the literal service numbers, original filename bytes and the
// negative-return retry branch.
[[nodiscard]] MillenniumAtariTrapEntry parse_millennium_atari_trap_entry(
    const MillenniumAtariBssSource& source, const MillenniumAtariMaterializedTarget& target);

// Looks for exactly the filename requested by the verified Fopen sequence in
// one already-parsed supplied FAT12 image.  For a present regular file it
// reads the exact FAT chain in memory to preserve its hash and leading machine
// words; it never treats absence as permission to generate a configuration.
[[nodiscard]] MillenniumAtariConfigEvidence probe_millennium_atari_config(
    const Fat12Disk& disk);

// Validates the complete 13-file regular-root inventory of the exact Equinox
// filesystem. It follows only original FAT chains in memory and fails closed
// on any renamed, added, missing, relocated, resized, or byte-altered file.
[[nodiscard]] MillenniumAtariRootInventory inventory_millennium_atari_equinox_root(
    const Fat12Disk& disk);

// Reads the exact FAT12 chain of MILL22B.INF and validates its isolated
// MILL22E.INF NUL-terminated literal. No call reachability is inferred.
[[nodiscard]] MillenniumAtariAuxiliaryResourceNameEvidence
probe_millennium_atari_auxiliary_resource_name(const Fat12Disk& disk);

// Validates an exact static candidate control block named by MILL22A.inf's
// leading JMP on the verified Equinox disk. It reports literal XBIOS
// selectors, arguments, JSR destinations and PEA target only. The native
// load address remains unresolved; no TOS/XBIOS/GEMDOS service is emulated,
// no file is unpacked and no supplied media is changed.
[[nodiscard]] MillenniumAtariConfigEntry parse_millennium_atari_config_entry(
    std::span<const std::uint8_t> payload);

// Validates the exact byte range referenced by the entry's second literal
// TRAP argument. This is preservation evidence only: it does not invoke a
// service, open either filename, or advance the config control path.
[[nodiscard]] MillenniumAtariConfigTrapArgumentStrings
parse_millennium_atari_config_trap_argument_strings(
    std::span<const std::uint8_t> payload, const MillenniumAtariConfigEntry& entry);

// Validates the exact first JSR destination referenced by the verified entry
// block. It does not invoke it, infer the dynamic-bit instruction's inputs,
// or emulate TOS/XBIOS/GEMDOS state.
[[nodiscard]] MillenniumAtariConfigFirstJsr parse_millennium_atari_config_first_jsr(
    std::span<const std::uint8_t> payload, const MillenniumAtariConfigEntry& entry);

// Validates the second direct JSR destination referenced by the verified
// entry block. It never executes either successor call, evaluates the
// conditional branch, or models platform state.
[[nodiscard]] MillenniumAtariConfigSecondJsr parse_millennium_atari_config_second_jsr(
    std::span<const std::uint8_t> payload, const MillenniumAtariConfigEntry& entry);

// Validates the small common target of the second config routine. It neither
// executes the Line-A instruction nor models the RAM slots it references.
[[nodiscard]] MillenniumAtariConfigJoinJsr parse_millennium_atari_config_join_jsr(
    std::span<const std::uint8_t> payload, const MillenniumAtariConfigSecondJsr& second);

// Validates the repeated $2aa0c forwarding target and its immediate local
// body. It does not invoke TRAP #14 or infer any XBIOS/firmware effect.
[[nodiscard]] MillenniumAtariConfigForwardedJsr parse_millennium_atari_config_forwarded_jsr(
    std::span<const std::uint8_t> payload, const MillenniumAtariConfigEntry& entry);

// Validates only the first local gate in direct target $2b2be and the original
// bytes at its taken branch destination. It never chooses a D0 value or
// executes either side of the branch.
[[nodiscard]] MillenniumAtariConfigThirdJsr parse_millennium_atari_config_third_jsr(
    std::span<const std::uint8_t> payload, const MillenniumAtariConfigEntry& entry);

[[nodiscard]] MillenniumAtariConfigThirdRoutine parse_millennium_atari_config_third_routine(
    std::span<const std::uint8_t> payload, const MillenniumAtariConfigEntry& entry);

// Validates the direct $2b448 target's initial register/address setup only.
// It does not execute the ensuing loops, traps, or any data pointed to here.
[[nodiscard]] MillenniumAtariConfigFourthJsr parse_millennium_atari_config_fourth_jsr(
    std::span<const std::uint8_t> payload, const MillenniumAtariConfigEntry& entry);

// Validates the static contiguous predecessor of the verified $2b448 setup.
// It does not establish that the predecessor is called or either DBF loop is
// dynamically entered.
[[nodiscard]] MillenniumAtariConfigFourthPrelude parse_millennium_atari_config_fourth_prelude(
    std::span<const std::uint8_t> payload, const MillenniumAtariConfigFourthJsr& setup);

// Validates the first exact $2b448 loop body and its DBF backedge only. It
// never runs iterations, reads pointed-to data, or translates loop effects
// into a replacement runtime state.
[[nodiscard]] MillenniumAtariConfigFourthLoop parse_millennium_atari_config_fourth_loop(
    std::span<const std::uint8_t> payload, const MillenniumAtariConfigFourthJsr& setup);

// Validates only the post-inner-loop outer-DBF path and its target prefix. It
// never performs either loop, follows data pointers, or invokes native calls.
[[nodiscard]] MillenniumAtariConfigFourthPostLoop parse_millennium_atari_config_fourth_post_loop(
    std::span<const std::uint8_t> payload, const MillenniumAtariConfigFourthLoop& loop);

// Validates the immediate setup at the outer DBF target and its direct fall
// through to the previously proven inner-loop body. It never iterates either
// loop or accesses data.
[[nodiscard]] MillenniumAtariConfigFourthOuterSetup parse_millennium_atari_config_fourth_outer_setup(
    std::span<const std::uint8_t> payload, const MillenniumAtariConfigFourthPostLoop& post_loop);

// Validates the first strict post-outer-loop TRAP #14 boundary. It does not
// emulate, invoke, or infer any service effect beyond the literal arguments.
[[nodiscard]] MillenniumAtariConfigFourthPostOuterBoundary parse_millennium_atari_config_fourth_post_outer_boundary(
    std::span<const std::uint8_t> payload, const MillenniumAtariConfigFourthPostLoop& post_loop);

// Hash-validates the exact native-dependent tail after the preceding TRAP #14
// opcode. It neither emulates the trap nor interprets, runs, or exposes any
// post-trap state transition.
[[nodiscard]] MillenniumAtariConfigFourthPostOuterTail parse_millennium_atari_config_fourth_post_outer_tail(
    std::span<const std::uint8_t> payload,
    const MillenniumAtariConfigFourthPostOuterBoundary& boundary);

// Validates the exact static setup reached by the tail's DBF backedge. It
// establishes only a byte-level return to the already proven loop body.
[[nodiscard]] MillenniumAtariConfigFourthPostOuterRecurrence parse_millennium_atari_config_fourth_post_outer_recurrence(
    std::span<const std::uint8_t> payload, const MillenniumAtariConfigFourthPostOuterTail& tail,
    const MillenniumAtariConfigFourthLoop& loop);

// Finds and validates all exact 0x4eb9 absolute-JSR encodings in the verified
// payload. It does not claim that every byte pattern is reachable code.
[[nodiscard]] MillenniumAtariConfigAbsoluteJsrInventory inventory_millennium_atari_config_absolute_jsrs(
    std::span<const std::uint8_t> payload);

// Hash-locks the complete static body at the inventory-only JSR encoding
// +0xdac -> $2b576. It neither executes the body nor promotes the raw
// encoding to a proven call path.
[[nodiscard]] MillenniumAtariConfigResidualJsrBody parse_millennium_atari_config_residual_jsr_body(
    std::span<const std::uint8_t> payload,
    const MillenniumAtariConfigAbsoluteJsrInventory& inventory);

} // namespace eon
