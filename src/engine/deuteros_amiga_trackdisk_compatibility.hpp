#pragma once

#include "data/amiga_adf.hpp"
#include "data/sha256.hpp"
#include "engine/native_runtime_memory.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>

namespace eon {

enum class DeuterosAmigaTrackdiskServiceOrigin : std::uint8_t {
    native_compatibility,
};

// An Eon-owned task identity. Its token is not an Amiga Task pointer or an
// observed FindTask result.
struct DeuterosAmigaNativeTaskHandle {
    std::uint64_t runtime_generation = 0;
    std::uint64_t service_sequence = 0;
    std::uint32_t call_address = 0;
    std::uint32_t return_address = 0;
    std::uint64_t opaque_token = 0;
    DeuterosAmigaTrackdiskServiceOrigin origin =
        DeuterosAmigaTrackdiskServiceOrigin::native_compatibility;
};

// Eon registers the exact MsgPort fields initialized by the title's local
// code. The guest address remains a source-derived address, not a host object
// or a claim that Exec's intrusive port list was observed.
struct DeuterosAmigaNativeTrackdiskPortRegistration {
    std::uint64_t runtime_generation = 0;
    std::uint64_t service_sequence = 0;
    std::uint32_t call_address = 0;
    std::uint32_t port_address = 0;
    std::uint32_t name_address = 0;
    std::uint64_t task_token = 0;
    std::uint8_t node_type = 0;
    std::int8_t node_priority = 0;
    std::uint8_t signal_bit = 0;
    DeuterosAmigaTrackdiskServiceOrigin origin =
        DeuterosAmigaTrackdiskServiceOrigin::native_compatibility;
};

// This records Eon's device-service availability decision, not an Exec
// OpenDevice D0/SR return. Media identity remains independent of availability.
struct DeuterosAmigaNativeTrackdiskServiceBinding {
    std::uint64_t runtime_generation = 0;
    std::uint64_t service_sequence = 0;
    std::uint32_t call_address = 0;
    std::uint32_t request_address = 0;
    std::uint32_t reply_port_address = 0;
    std::uint8_t unit = 0;
    std::uint32_t flags = 0;
    bool device_service_available = false;
    // Host-side route for Eon's later I/O handler. Never written to IORequest.
    std::uint64_t opaque_device_token = 0;
    std::string media_sha256;
    DeuterosAmigaTrackdiskServiceOrigin origin =
        DeuterosAmigaTrackdiskServiceOrigin::native_compatibility;
};

// The previous motor state is the exact io_Actual effect of TD_MOTOR.
// This result is never recorded as a captured DoIO observation.
struct DeuterosAmigaNativeTrackdiskMotorResult {
    std::uint64_t runtime_generation = 0;
    std::uint64_t service_sequence = 0;
    std::uint32_t call_address = 0;
    std::uint32_t request_address = 0;
    std::uint64_t opaque_device_token = 0;
    std::uint16_t command = 0;
    std::uint32_t requested_length = 0;
    std::uint32_t io_error = 0;
    std::uint32_t previous_motor_state = 0;
    DeuterosAmigaTrackdiskServiceOrigin origin =
        DeuterosAmigaTrackdiskServiceOrigin::native_compatibility;
};

struct DeuterosAmigaNativeTrackdiskReadResult {
    std::uint64_t runtime_generation = 0;
    std::uint64_t service_sequence = 0;
    std::uint32_t call_address = 0;
    std::uint32_t request_address = 0;
    std::uint32_t buffer_address = 0;
    std::uint32_t disk_offset = 0;
    std::uint32_t actual_bytes = 0;
    std::uint32_t io_error = 0;
    std::uint32_t result_d0 = 0;
    std::uint8_t unit = 0;
    std::string media_sha256;
    std::string payload_sha256;
    DeuterosAmigaTrackdiskServiceOrigin origin =
        DeuterosAmigaTrackdiskServiceOrigin::native_compatibility;
};

// Native, source-bounded compatibility for the task/port/trackdisk setup at
// $206d4..$207ca and the two recovered TD_MOTOR calls. Media attachment is
// explicit so this service never guesses which ADF belongs to a unit. It
// implements only the four recovered title ETD_READ chunks. Write/update
// requests remain outside this compatibility contract.
class DeuterosAmigaTrackdiskCompatibilitySession {
public:
    explicit DeuterosAmigaTrackdiskCompatibilitySession(
        const std::uint64_t runtime_generation) noexcept
        : runtime_generation_(runtime_generation) {}

    [[nodiscard]] bool attach_unit_media(const std::uint8_t unit,
        const std::string_view media_sha256) {
        if (runtime_generation_ == 0 || runtime_generation_ > maximum_generation
            || unit > 1 || media_sha256.size() != 64
            || !is_sha256(media_sha256) || units_[unit].attached
            || units_[unit].service_bound) {
            return false;
        }
        units_[unit].attached = true;
        units_[unit].media_sha256.assign(media_sha256);
        return true;
    }

    [[nodiscard]] std::optional<DeuterosAmigaNativeTaskHandle>
    find_title_task(const std::uint32_t exec_base_source_address,
        const std::uint32_t call_address, const std::int16_t vector,
        const std::uint32_t return_address, const std::uint32_t name_address) noexcept {
        if (!valid_generation() || task_token_ != 0 || exec_base_source_address != 4
            || call_address != 0x206de || vector != -0x126
            || return_address != 0x206e2 || name_address != 0) {
            return std::nullopt;
        }
        task_token_ = make_token(0x1U);
        return DeuterosAmigaNativeTaskHandle{runtime_generation_, next_sequence(),
            call_address, return_address, task_token_};
    }

    [[nodiscard]] std::optional<DeuterosAmigaNativeTrackdiskPortRegistration>
    add_title_port(const std::uint32_t exec_base_source_address,
        const std::uint32_t call_address, const std::int16_t vector,
        const std::uint32_t return_address, const std::uint32_t port_address,
        const std::uint32_t name_address, const std::uint64_t task_token,
        const std::uint8_t node_type, const std::int8_t node_priority,
        const std::uint8_t signal_bit) noexcept {
        if (!valid_generation() || task_token_ == 0 || task_token != task_token_
            || exec_base_source_address != 4 || vector != -0x162
            || name_address != 0x206ac || node_type != 4
            || node_priority != 0x7f || signal_bit != 1) {
            return std::nullopt;
        }

        std::size_t slot = 0;
        if (call_address == 0x2070c && return_address == 0x20710
            && port_address == 0x2061c && !ports_[0]) {
            slot = 0;
        } else if (call_address == 0x2077a && return_address == 0x2077e
            && port_address == 0x20676 && ports_[0] && units_[0].service_bound
            && !ports_[1]) {
            slot = 1;
        } else {
            return std::nullopt;
        }

        ports_[slot] = true;
        return DeuterosAmigaNativeTrackdiskPortRegistration{
            runtime_generation_, next_sequence(), call_address, port_address,
            name_address, task_token_, node_type, node_priority, signal_bit};
    }

    [[nodiscard]] std::optional<DeuterosAmigaNativeTrackdiskServiceBinding>
    bind_title_unit_service(const std::uint32_t exec_base_source_address,
        const std::uint32_t call_address, const std::int16_t vector,
        const std::uint32_t return_address, const std::uint32_t device_name_address,
        const std::string_view device_name, const std::uint32_t request_address,
        const std::uint32_t reply_port_address, const std::uint32_t unit,
        const std::uint32_t flags) noexcept {
        if (!valid_generation() || exec_base_source_address != 4 || vector != -0x1bc
            || device_name_address != 0x206ac || device_name != "trackdisk.device"
            || flags != 0 || unit > 1) {
            return std::nullopt;
        }

        std::size_t slot = 0;
        if (call_address == 0x20732 && return_address == 0x20736 && unit == 0
            && request_address == 0x205e4 && reply_port_address == 0x2061c
            && ports_[0] && !units_[0].service_bound) {
            slot = 0;
        } else if (call_address == 0x207a0 && return_address == 0x207a4
            && unit == 1 && request_address == 0x2063e
            && reply_port_address == 0x20676 && ports_[1]
            && !units_[1].service_bound) {
            slot = 1;
        } else {
            return std::nullopt;
        }

        DeuterosAmigaNativeTrackdiskServiceBinding result{
            runtime_generation_, next_sequence(), call_address, request_address,
            reply_port_address, static_cast<std::uint8_t>(unit), flags, true,
            0, units_[slot].media_sha256};
        units_[slot].service_bound = true;
        units_[slot].device_token = make_token(0x10U + unit);
        result.opaque_device_token = units_[slot].device_token;
        return result;
    }

    [[nodiscard]] std::optional<DeuterosAmigaNativeTrackdiskMotorResult>
    do_title_motor(const std::uint32_t call_address,
        const std::uint32_t request_address, const std::uint64_t device_token,
        const std::uint16_t command, const std::uint32_t requested_length,
        const std::uint8_t io_flags) noexcept {
        if (!valid_generation() || command != 9 || io_flags != 0
            || (call_address != 0x207ea && call_address != 0x2080a)
            || (call_address == 0x207ea && request_address != 0x205e4)
            || (call_address == 0x207ea && requested_length != 1)
            || (call_address == 0x2080a && requested_length != 0)) {
            return std::nullopt;
        }

        std::size_t slot = 0;
        if (request_address == 0x205e4 && units_[0].service_bound
            && device_token == units_[0].device_token) {
            slot = 0;
        } else if (request_address == 0x2063e && units_[1].service_bound
            && device_token == units_[1].device_token) {
            slot = 1;
        } else {
            return std::nullopt;
        }

        const auto previous = units_[slot].motor_on ? 1U : 0U;
        units_[slot].motor_on = requested_length == 1;
        return DeuterosAmigaNativeTrackdiskMotorResult{
            runtime_generation_, next_sequence(), call_address, request_address,
            device_token, command, requested_length,
            0, previous};
    }

    // Services the four source-profile ETD_READ calls using one explicitly
    // attached unit-0 image. Request fields come from owned native memory;
    // the media identity is checked against the attachment, and memory plus
    // session progress commit together.
    [[nodiscard]] std::optional<DeuterosAmigaNativeTrackdiskReadResult>
    do_title_read(const std::uint32_t call_address,
        const std::uint32_t request_address, const std::uint64_t device_token,
        const std::span<const std::uint8_t> media_image,
        NativeRuntimeMemory& runtime_memory) {
        constexpr std::uint32_t chunk_bytes = 0x1600U;
        constexpr std::uint32_t first_offset = 0x14000U;
        constexpr std::uint32_t io_request_pointer_cell = 0x206a0U;
        constexpr std::uint32_t io_data_pointer_cell = 0x206a4U;
        constexpr std::uint32_t request_bytes = 0x34U;
        constexpr std::array<std::string_view, 4> chunk_hashes{
            "21e34a4162953998ab45dc1469b280f1d48dd9288596393fcd445e2aabd8a10d",
            "63306d78650d5958dcf8e093b10ea37b9c7721b559a09ff51407d3dee64ce9d6",
            "08505e304e73338409a0a28ffca915e6d75c8dc5955ab70281bf74b6258a5ddd",
            "579c42ce56eb5b88321e93e32f6e817a29ee0936b66124e2e5d25db01fe3347c",
        };
        if (!valid_generation() || title_read_ordinal_ >= chunk_hashes.size()
            || call_address != 0x20934U || request_address != 0x205e4U
            || !units_[0].attached || !units_[0].service_bound
            || device_token == 0 || device_token != units_[0].device_token
            || media_image.size() != AmigaAdf::standard_size
            || to_hex(sha256(media_image)) != units_[0].media_sha256) {
            return std::nullopt;
        }

        const auto request = runtime_memory.read_linear_range(request_address, request_bytes);
        const auto owned_request = runtime_memory.read_linear_range(io_request_pointer_cell, 4);
        const auto ordinal = static_cast<std::uint32_t>(title_read_ordinal_);
        const auto expected_offset = first_offset + ordinal * chunk_bytes;
        if (!request || !owned_request) return std::nullopt;
        const auto read_be = [&request](const std::size_t offset, const std::size_t size) {
            std::uint32_t value = 0;
            for (std::size_t i = 0; i < size; ++i) {
                value = (value << 8U) | (*request)[offset + i];
            }
            return value;
        };
        const auto io_data = read_be(0x28, 4);
        const auto pointer_to_request =
            (static_cast<std::uint32_t>((*owned_request)[0]) << 24U)
            | (static_cast<std::uint32_t>((*owned_request)[1]) << 16U)
            | (static_cast<std::uint32_t>((*owned_request)[2]) << 8U)
            | static_cast<std::uint32_t>((*owned_request)[3]);
        // $206a4 is read by the original helper immediately before it fills
        // io_Data. Its value is runtime state; require it to agree with the
        // owned pointer cell rather than assuming it equals the caller's
        // eventual destination ($26cc0).
        const auto owned_io_data = runtime_memory.read_linear_range(io_data_pointer_cell, 4);
        if (!owned_io_data) return std::nullopt;
        const auto pointer_value = (static_cast<std::uint32_t>((*owned_io_data)[0]) << 24U)
            | (static_cast<std::uint32_t>((*owned_io_data)[1]) << 16U)
            | (static_cast<std::uint32_t>((*owned_io_data)[2]) << 8U)
            | static_cast<std::uint32_t>((*owned_io_data)[3]);
        if (pointer_to_request != request_address || io_data != pointer_value
            || io_data > 0x01000000U - chunk_bytes
            || !runtime_memory.read_linear_range(io_data, chunk_bytes)
            || read_be(0x1c, 2) != 0x8002U || read_be(0x1e, 1) != 0
            || read_be(0x24, 4) != chunk_bytes
            || read_be(0x2c, 4) != expected_offset
            || read_be(0x30, 4) != 0xffffffffU) {
            return std::nullopt;
        }

        const auto payload = media_image.subspan(expected_offset, chunk_bytes);
        const auto payload_hash = to_hex(sha256(payload));
        if (payload_hash != chunk_hashes[ordinal]) return std::nullopt;

        NativeRuntimeEffectBatch batch{
            "deuteros-amiga-native-etd-read-" + std::to_string(runtime_generation_)
                + "-" + std::to_string(title_read_ordinal_ + 1U), true, {}};
        batch.effects.reserve(payload.size() + 2U);
        for (std::size_t i = 0; i < payload.size(); ++i) {
            batch.effects.push_back({i + 1U,
                {NativeRuntimeAddressSpace::linear, std::nullopt, io_data + i},
                MemoryTransferElementWidth::byte, NativeRuntimeByteOrder::big_endian,
                payload[i]});
        }
        const auto order = payload.size();
        batch.effects.push_back({order + 1U,
            {NativeRuntimeAddressSpace::linear, std::nullopt, request_address + 0x1fU},
            MemoryTransferElementWidth::byte, NativeRuntimeByteOrder::big_endian, 0});
        batch.effects.push_back({order + 2U,
            {NativeRuntimeAddressSpace::linear, std::nullopt, request_address + 0x20U},
            MemoryTransferElementWidth::longword, NativeRuntimeByteOrder::big_endian,
            chunk_bytes});
        auto next_memory = runtime_memory;
        const auto applied = next_memory.apply(batch);
        if (!applied.accepted) return std::nullopt;

        const auto sequence = next_sequence();
        ++title_read_ordinal_;
        runtime_memory = std::move(next_memory);
        return DeuterosAmigaNativeTrackdiskReadResult{
            runtime_generation_, sequence, call_address, request_address,
            io_data, expected_offset, chunk_bytes, 0, 0, 0,
            units_[0].media_sha256, payload_hash};
    }

    void reset() noexcept {
        task_token_ = 0;
        ports_ = {};
        const auto media0 = units_[0].media_sha256;
        const auto attached0 = units_[0].attached;
        const auto media1 = units_[1].media_sha256;
        const auto attached1 = units_[1].attached;
        units_ = {};
        units_[0].media_sha256 = media0;
        units_[0].attached = attached0;
        units_[1].media_sha256 = media1;
        units_[1].attached = attached1;
        title_read_ordinal_ = 0;
        sequence_ = 1;
    }

    [[nodiscard]] std::uint64_t runtime_generation() const noexcept {
        return runtime_generation_;
    }

    [[nodiscard]] bool port_registered(const std::uint32_t address) const noexcept {
        return (address == 0x2061c && ports_[0])
            || (address == 0x20676 && ports_[1]);
    }

    [[nodiscard]] bool unit_service_bound(const std::uint8_t unit) const noexcept {
        return unit <= 1 && units_[unit].service_bound;
    }

    [[nodiscard]] std::uint32_t next_title_read_ordinal() const noexcept {
        return static_cast<std::uint32_t>(title_read_ordinal_);
    }

private:
    struct UnitState {
        bool attached = false;
        bool service_bound = false;
        bool motor_on = false;
        std::uint64_t device_token = 0;
        std::string media_sha256;
    };

    static constexpr std::uint64_t maximum_generation = 0x00ffffffffffffffULL;

    [[nodiscard]] static bool is_sha256(const std::string_view value) noexcept {
        for (const auto ch : value) {
            if (!((ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f'))) return false;
        }
        return true;
    }

    [[nodiscard]] bool valid_generation() const noexcept {
        return runtime_generation_ != 0 && runtime_generation_ <= maximum_generation
            && sequence_ != 0;
    }

    [[nodiscard]] std::uint64_t next_sequence() noexcept {
        const auto current = sequence_++;
        return current;
    }

    [[nodiscard]] std::uint64_t make_token(const std::uint64_t kind) const noexcept {
        return 0xe100000000000000ULL | (runtime_generation_ << 8U) | kind;
    }

    std::uint64_t runtime_generation_ = 0;
    std::uint64_t sequence_ = 1;
    std::uint64_t task_token_ = 0;
    std::array<bool, 2> ports_{};
    std::array<UnitState, 2> units_{};
    std::size_t title_read_ordinal_ = 0;
};

} // namespace eon
