#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
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

// The initial, directly-addressed control block in the genuine Equinox
// MILL22A.inf payload.  The file is not a host configuration format: its
// leading JMP and all reported addresses are original 68000 facts.  This
// parser never calls the traps, follows the JSRs, or projects any of its
// mutable words into a replacement game state.
struct MillenniumAtariConfigEntry {
    std::uint32_t proven_load_base = 0;
    std::uint32_t entry_address = 0;
    std::uint32_t entry_file_offset = 0;
    std::uint16_t initial_trap_selector = 0;
    std::uint32_t initial_trap_longword_argument = 0;
    std::uint16_t palette_trap_selector = 0;
    std::uint32_t palette_trap_longword_argument = 0;
    std::vector<std::uint32_t> jsr_targets;
    std::uint32_t final_pea_address = 0;
    std::uint16_t final_trap_selector = 0;
    std::uint32_t return_offset = 0;
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

// Validates the exact initial control path reached by MILL22A.inf's leading
// JMP on the verified Equinox disk.  It reports literal XBIOS selectors,
// arguments, JSR destinations and PEA target only.  No TOS/XBIOS/GEMDOS
// service is emulated, no file is unpacked and no supplied media is changed.
[[nodiscard]] MillenniumAtariConfigEntry parse_millennium_atari_config_entry(
    std::span<const std::uint8_t> payload);

// Validates the exact first JSR destination referenced by the verified entry
// block. It does not invoke it, infer the dynamic-bit instruction's inputs,
// or emulate TOS/XBIOS/GEMDOS state.
[[nodiscard]] MillenniumAtariConfigFirstJsr parse_millennium_atari_config_first_jsr(
    std::span<const std::uint8_t> payload, const MillenniumAtariConfigEntry& entry);

} // namespace eon
