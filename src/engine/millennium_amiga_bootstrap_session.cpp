#include "engine/millennium_amiga_bootstrap_session.hpp"

#include "data/sha256.hpp"

#include <stdexcept>

namespace eon {
namespace {

std::vector<std::uint8_t> require_defjam_millennium_adf(
    std::vector<std::uint8_t> disk_image) {
    constexpr std::string_view defjam_millennium_adf_sha256 =
        "8263e19b431b61c3c34363bb282703476145a45259c94132be82b529ec13b53c";
    if (to_hex(sha256(disk_image)) != defjam_millennium_adf_sha256) {
        throw std::runtime_error("Unsupported Millennium Amiga Defjam ADF");
    }
    return disk_image;
}

} // namespace

MillenniumAmigaBootstrapSession::MillenniumAmigaBootstrapSession(
    std::vector<std::uint8_t> disk_image) {
    const AmigaAdf disk(require_defjam_millennium_adf(std::move(disk_image)));
    plan_ = parse_millennium_amiga_load_plan(disk);
    shared_resident_ = parse_millennium_amiga_shared_resident_layout(disk.bytes(0,
        AmigaAdf::standard_size));
    if (plan_.resident_stage.disk_offset != shared_resident_.disk_offset
        || plan_.resident_stage.length != shared_resident_.length
        || plan_.resident_stage.destination != shared_resident_.destination
        || plan_.resident_stage.raw_sha256 != shared_resident_.raw_sha256) {
        throw std::runtime_error("Millennium Amiga raw resident plan differs from shared evidence");
    }
    resident_evidence_.entry = parse_millennium_amiga_resident_entry(disk, plan_);
    opaque_invocation_boundary_ =
        parse_millennium_amiga_bootstrap_opaque_invocation_boundary(disk, plan_);
    relocation_boundary_ = parse_millennium_amiga_bootstrap_relocation_boundary(disk, plan_);
    first_stage_source_anchors_ =
        parse_millennium_amiga_first_stage_source_anchor_boundary(disk, plan_);
    resident_evidence_.splitter = parse_millennium_amiga_resident_word_splitter(disk, plan_);
    resident_evidence_.helper_boundary = parse_millennium_amiga_resident_helper_raw_boundary(
        disk, plan_, resident_evidence_.splitter);
    resident_evidence_.setup_helper_boundary =
        parse_millennium_amiga_resident_setup_helper_raw_boundary(disk, plan_);
    resident_evidence_.staging_callsites = parse_millennium_amiga_resident_helper_staging_callsites(
        disk, plan_, resident_evidence_.splitter);
    resident_evidence_.first_post_helper_chain =
        parse_millennium_amiga_resident_first_post_helper_static_chain(
            disk, plan_, resident_evidence_.staging_callsites.front());
    resident_evidence_.second_post_helper_chain =
        parse_millennium_amiga_resident_second_post_helper_static_chain(
            disk, plan_, resident_evidence_.staging_callsites.back());
    resident_evidence_.staging_reachability =
        parse_millennium_amiga_resident_staging_direct_reachability_boundary(
            disk, plan_, resident_evidence_.staging_callsites);
    resident_evidence_.separate_entry = parse_millennium_amiga_resident_separate_entry_gate(disk, plan_);
    resident_evidence_.separate_branch = parse_millennium_amiga_resident_separate_branch_boundary(
        disk, plan_, resident_evidence_.separate_entry);
    resident_evidence_.separate_post_call = parse_millennium_amiga_resident_separate_post_call_boundary(
        disk, plan_, resident_evidence_.separate_branch);
    resident_evidence_.separate_post_call_tail =
        parse_millennium_amiga_resident_separate_post_call_tail_boundary(
            disk, plan_, resident_evidence_.separate_post_call);
    resident_evidence_.separate_post_call_tail_branch =
        parse_millennium_amiga_resident_separate_post_call_tail_branch_boundary(
            disk, plan_, resident_evidence_.separate_post_call_tail);
    resident_evidence_.separate_comparison = parse_millennium_amiga_resident_separate_comparison_boundary(
        disk, plan_, resident_evidence_.separate_post_call_tail_branch);
    resident_evidence_.separate_byte_gate = parse_millennium_amiga_resident_separate_byte_gate_boundary(
        disk, plan_, resident_evidence_.separate_comparison);
    resident_evidence_.separate_byte_gate_target =
        parse_millennium_amiga_resident_separate_byte_gate_target_boundary(
            disk, plan_, resident_evidence_.separate_byte_gate);
    resident_evidence_.separate_byte_gate_convergence =
        parse_millennium_amiga_resident_separate_byte_gate_convergence_boundary(
            disk, plan_, resident_evidence_.separate_byte_gate_target);
    resident_evidence_.separate_byte_gate_taken_branch =
        parse_millennium_amiga_resident_separate_byte_gate_taken_branch_boundary(
            disk, plan_, resident_evidence_.separate_byte_gate_convergence);
    resident_evidence_.separate_byte_gate_fallthrough =
        parse_millennium_amiga_resident_separate_byte_gate_fallthrough_boundary(
            disk, plan_, resident_evidence_.separate_byte_gate_convergence);
    resident_evidence_.separate_post_external_call =
        parse_millennium_amiga_resident_separate_post_external_call_boundary(
            disk, plan_, resident_evidence_.separate_byte_gate_taken_branch);
    resident_evidence_.separate_terminal_jump_target =
        parse_millennium_amiga_resident_separate_terminal_jump_raw_target_boundary(
            disk, plan_, resident_evidence_.separate_post_external_call);
    resident_evidence_.independent_entry =
        parse_millennium_amiga_resident_independent_entry_gate(disk, plan_);
    resident_evidence_.negative_d3 = parse_millennium_amiga_resident_negative_d3_continuation(
        disk, plan_, resident_evidence_.independent_entry);
    resident_evidence_.negative_d3_terminal = parse_millennium_amiga_resident_negative_d3_terminal(
        disk, plan_, resident_evidence_.negative_d3);
    resident_evidence_.post_negative_d3_terminal =
        parse_millennium_amiga_resident_post_negative_d3_terminal(
            disk, plan_, resident_evidence_.negative_d3_terminal);
    resident_evidence_.post_negative_d3_continuation =
        parse_millennium_amiga_resident_post_negative_d3_continuation_boundary(
            disk, plan_, resident_evidence_.post_negative_d3_terminal);

    // Keep the diagnostic boundary joined to the same session-owned plan.
    // This guards against a future caller validating one plan but reporting
    // a continuation/anchor record derived from another image or range.
    if (opaque_invocation_boundary_.first_stage_target != plan_.first_stage.destination
        || opaque_invocation_boundary_.resident_stage_target != plan_.resident_entry
        || relocation_boundary_.raw_continuation_source_address
            != opaque_invocation_boundary_.entry_address
        || first_stage_source_anchors_.raw_disk_offset != plan_.first_stage.disk_offset
        || first_stage_source_anchors_.byte_count != plan_.first_stage.length
        || first_stage_source_anchors_.sha256 != plan_.first_stage.raw_sha256
        || resident_evidence_.post_negative_d3_terminal.nonnegative_branch_target
            != resident_evidence_.post_negative_d3_continuation.entry_address
        || resident_evidence_.post_negative_d3_continuation.entry_address != 0x6861a
        || resident_evidence_.separate_terminal_jump_target.jump_address
            != resident_evidence_.separate_post_external_call.terminal_jump_address
        || resident_evidence_.separate_terminal_jump_target.target_address
            != resident_evidence_.separate_post_external_call.terminal_jump_target) {
        throw std::runtime_error("Millennium Amiga bootstrap evidence is detached from load plan");
    }
}

} // namespace eon
