#include "engine/deuteros_amiga_title_descriptor_model.hpp"

#include <cassert>

int main() {
    using namespace eon;
    const auto& profile = deuteros_amiga_title_descriptor_profile();
    assert(profile.release_sha256
        == "f4dc8dd1c27c5d389837783becd9b95ab09b78baf40e94e39e2b7e590e470e04");
    assert(profile.routine_address == 0x3fbf8 && profile.routine_size == 0xb8
        && profile.caller_address == 0x1fbe6);
    assert(profile.routine_sha256
        == "a2301bf08c6c1c5615368687c620f17293d3f148e19934092854762ca91507b2");
    assert(profile.source_table_address == 0x3f7fa && profile.row_stride == 14);

    DeuterosAmigaTitleDescriptorInputs in;
    in.descriptor_marker = 1;
    in.source_row = {{0, 0, 0, 1, 0x11, 0x22, 0x33, 0x44,
                        0x55, 0x66, 0x77, 0x88, 0x99, 0xaa}};
    in.flags_preimage = 0x1230;
    const auto plan = evaluate_deuteros_amiga_title_descriptor(in);
    assert(plan && plan->size() == 4);
    assert((*plan)[0] == (DeuterosAmigaTitleDescriptorWrite{
        0x3f7be, {0x12, 0x34}}));
    assert((*plan)[1] == (DeuterosAmigaTitleDescriptorWrite{
        0x3f7de, {0, 0, 0, 1, 0x11, 0x22, 0x33, 0x44,
                  0x55, 0x66, 0x77, 0x88, 0x99, 0xaa}}));
    assert((*plan)[2] == (DeuterosAmigaTitleDescriptorWrite{
        0x3f7be, {0x12, 0x3c}}));
    assert((*plan)[3] == (DeuterosAmigaTitleDescriptorWrite{
        0x3f7ec, {0, 0, 0, 1, 0x11, 0x22, 0x33, 0x44,
                  0x55, 0x66, 0x77, 0x88, 0x99, 0xaa}}));

    // BPL takes the direct row when the marker is nonnegative; a negative
    // marker follows the longword at descriptor+4 before copying 14 bytes.
    auto indirect = in;
    indirect.descriptor_marker = 0x80000000U;
    indirect.descriptor_data_pointer = 0x61000;
    indirect.source_row_address = 0x61000;
    assert(evaluate_deuteros_amiga_title_descriptor(indirect) == plan);

    auto rejected = in;
    rejected.gate_byte = 1;
    assert(!evaluate_deuteros_amiga_title_descriptor(rejected));
    rejected = in; rejected.caller_d0++;
    assert(!evaluate_deuteros_amiga_title_descriptor(rejected));
    rejected = in; rejected.descriptor_address++;
    assert(!evaluate_deuteros_amiga_title_descriptor(rejected));
    rejected = in; rejected.source_row_address++;
    assert(!evaluate_deuteros_amiga_title_descriptor(rejected));
    rejected = in;
    rejected.descriptor_marker = 0x80000000U;
    rejected.descriptor_data_pointer = 0x61000;
    assert(!evaluate_deuteros_amiga_title_descriptor(rejected));

    auto general = in;
    general.caller_d0 = 2;
    general.caller_d1 = 0x0121; // only the first four selector bits are consumed
    general.descriptor_address = profile.source_table_address + 2U * profile.row_stride;
    general.descriptor_marker_source_address = general.descriptor_address;
    general.source_row_address = general.descriptor_address;
    const auto general_plan = evaluate_deuteros_amiga_title_descriptor(general);
    assert(general_plan && general_plan->size() == 2);
    assert((*general_plan)[0] == (DeuterosAmigaTitleDescriptorWrite{
        0x3f7be, {0x12, 0x31}}));
    assert((*general_plan)[1].address == 0x3f7c2);

    auto unbounded = general;
    unbounded.caller_d0 = 0xffff;
    unbounded.descriptor_address = profile.source_table_address
        + std::uint32_t{unbounded.caller_d0} * profile.row_stride;
    unbounded.descriptor_marker_source_address = unbounded.descriptor_address;
    unbounded.source_row_address = unbounded.descriptor_address;
    assert(!evaluate_deuteros_amiga_title_descriptor(unbounded));
}
