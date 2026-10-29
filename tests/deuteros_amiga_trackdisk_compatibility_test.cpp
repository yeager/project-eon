#include "engine/deuteros_amiga_trackdisk_compatibility.hpp"

#include <cassert>

int main() {
    using namespace eon;

    DeuterosAmigaTrackdiskCompatibilitySession session(42);
    const auto task = session.find_title_task(4, 0x206de, -0x126, 0x206e2, 0);
    assert(task);
    assert(task->runtime_generation == 42);
    assert(task->origin == DeuterosAmigaTrackdiskServiceOrigin::native_compatibility);

    const auto first_port = session.add_title_port(4, 0x2070c, -0x162, 0x20710,
        0x2061c, 0x206ac, task->opaque_token, 4, 0x7f, 1);
    assert(first_port);
    assert(first_port->task_token == task->opaque_token);
    assert(session.port_registered(0x2061c));

    // Exec opens the resident trackdisk device independently of whether an
    // image has been attached to this unit.
    const auto unit0 = session.bind_title_unit_service(4, 0x20732, -0x1bc, 0x20736,
        0x206ac, "trackdisk.device", 0x205e4, 0x2061c, 0, 0);
    assert(unit0 && unit0->device_service_available);
    assert(unit0->opaque_device_token != 0);
    assert(unit0->media_sha256.empty());

    const auto second_port = session.add_title_port(4, 0x2077a, -0x162, 0x2077e,
        0x20676, 0x206ac, task->opaque_token, 4, 0x7f, 1);
    assert(second_port);
    const auto unit1 = session.bind_title_unit_service(4, 0x207a0, -0x1bc, 0x207a4,
        0x206ac, "trackdisk.device", 0x2063e, 0x20676, 1, 0);
    assert(unit1 && unit1->device_service_available);
    assert(unit1->opaque_device_token != unit0->opaque_device_token);

    const auto motor_on = session.do_title_motor(0x207ea, 0x205e4,
        unit0->opaque_device_token, 9, 1, 0);
    assert(motor_on && motor_on->previous_motor_state == 0);
    const auto motor_off = session.do_title_motor(0x2080a, 0x205e4,
        unit0->opaque_device_token, 9, 0, 0);
    assert(motor_off && motor_off->previous_motor_state == 1);

    assert(!session.bind_title_unit_service(4, 0x20732, -0x1bc, 0x20736,
        0x206ac, "trackdisk.device", 0x205e4, 0x2061c, 0, 1));
    assert(!session.do_title_motor(0x207ea, 0x205e4,
        unit0->opaque_device_token, 9, 0, 0));
    assert(!session.do_title_motor(0x207ea, 0x2063e,
        unit1->opaque_device_token, 9, 1, 0));
    assert(!session.attach_unit_media(2, "0000000000000000000000000000000000000000000000000000000000000000"));
}
