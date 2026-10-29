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

    DeuterosAmigaTitleDescriptorInputs in;
    in.selected_row = {{0, 0, 0, 1, 0x11, 0x22, 0x33, 0x44,
                        0x55, 0x66, 0x77, 0x88, 0x99, 0xaa}};
    in.flags_preimage = 0x1230;
    const auto plan = evaluate_deuteros_amiga_title_descriptor(in);
    assert(plan && plan->size() == 3);
    assert((*plan)[0] == (DeuterosAmigaTitleDescriptorWrite{
        0x3f7be, {0x12, 0x3c}}));
    assert((*plan)[1] == (DeuterosAmigaTitleDescriptorWrite{
        0x3f7de, {0, 0, 0, 1, 0x11, 0x22, 0x33, 0x44,
                  0x55, 0x66, 0x77, 0x88, 0x99, 0xaa}}));
    assert((*plan)[2] == (DeuterosAmigaTitleDescriptorWrite{
        0x3f7ec, {0, 0, 0, 1, 0x11, 0x22, 0x33, 0x44,
                  0x55, 0x66, 0x77, 0x88, 0x99, 0xaa}}));

    auto rejected = in;
    rejected.gate_byte = 1;
    assert(!evaluate_deuteros_amiga_title_descriptor(rejected));
    rejected = in; rejected.caller_d0++;
    assert(!evaluate_deuteros_amiga_title_descriptor(rejected));
    rejected = in; rejected.caller_d1++;
    assert(!evaluate_deuteros_amiga_title_descriptor(rejected));
    rejected = in; rejected.selected_row_address++;
    assert(!evaluate_deuteros_amiga_title_descriptor(rejected));
    rejected = in; rejected.selected_row[0] = 0x80;
    assert(!evaluate_deuteros_amiga_title_descriptor(rejected));
}
