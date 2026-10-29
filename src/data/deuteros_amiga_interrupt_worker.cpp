#include "data/deuteros_amiga_interrupt_worker.hpp"
#include "data/sha256.hpp"
#include "engine/deuteros_amiga_owned_commands.hpp"

#include <array>
#include <functional>
#include <limits>
#include <map>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace eon {
namespace {

using ReadByte = std::function<std::optional<std::uint8_t>(std::uint32_t)>;

class SparseMemory {
public:
    explicit SparseMemory(ReadByte read_byte) : read_byte_(std::move(read_byte)) {}

    [[nodiscard]] std::uint8_t read(const std::uint32_t address) {
        if (const auto changed = changes_.find(address); changed != changes_.end()) {
            return changed->second;
        }
        if (const auto original = originals_.find(address); original != originals_.end()) {
            return original->second;
        }
        const auto value = read_byte_(address);
        if (!value) throw std::runtime_error("Deuteros interrupt worker read is not owned");
        originals_.emplace(address, *value);
        return *value;
    }

    [[nodiscard]] std::uint16_t read_word(const std::uint32_t address) {
        if ((address & 1U) != 0 || address == std::numeric_limits<std::uint32_t>::max()) {
            throw std::runtime_error("Deuteros interrupt worker word read is unaligned or overflows");
        }
        return static_cast<std::uint16_t>((static_cast<std::uint16_t>(read(address)) << 8U)
            | read(address + 1U));
    }

    [[nodiscard]] std::uint32_t read_long(const std::uint32_t address) {
        if ((address & 1U) != 0 || address > std::numeric_limits<std::uint32_t>::max() - 3U) {
            throw std::runtime_error("Deuteros interrupt worker long read is unaligned or overflows");
        }
        return (static_cast<std::uint32_t>(read(address)) << 24U)
            | (static_cast<std::uint32_t>(read(address + 1U)) << 16U)
            | (static_cast<std::uint32_t>(read(address + 2U)) << 8U)
            | read(address + 3U);
    }

    [[nodiscard]] std::uint32_t read_value(const std::uint32_t address,
        const MemoryTransferElementWidth width) {
        switch (width) {
        case MemoryTransferElementWidth::byte: return read(address);
        case MemoryTransferElementWidth::word: return read_word(address);
        case MemoryTransferElementWidth::longword: return read_long(address);
        }
        throw std::runtime_error("Invalid Deuteros VERTB read width");
    }

    void write_word(const std::uint32_t address, const std::uint16_t value) {
        if ((address & 1U) != 0 || address == std::numeric_limits<std::uint32_t>::max()) {
            throw std::runtime_error("Deuteros interrupt worker word write is unaligned or overflows");
        }
        static_cast<void>(read(address));
        static_cast<void>(read(address + 1U));
        changes_[address] = static_cast<std::uint8_t>(value >> 8U);
        changes_[address + 1U] = static_cast<std::uint8_t>(value);
    }

    void write_long(const std::uint32_t address, const std::uint32_t value) {
        if ((address & 1U) != 0 || address > std::numeric_limits<std::uint32_t>::max() - 3U) {
            throw std::runtime_error("Deuteros interrupt worker long write is unaligned or overflows");
        }
        for (std::uint32_t byte = 0; byte < 4; ++byte) {
            static_cast<void>(read(address + byte));
        }
        changes_[address] = static_cast<std::uint8_t>(value >> 24U);
        changes_[address + 1U] = static_cast<std::uint8_t>(value >> 16U);
        changes_[address + 2U] = static_cast<std::uint8_t>(value >> 8U);
        changes_[address + 3U] = static_cast<std::uint8_t>(value);
    }

    void write_byte(const std::uint32_t address, const std::uint8_t value) {
        static_cast<void>(read(address));
        changes_[address] = value;
    }

    void write_value(const std::uint32_t address, const MemoryTransferElementWidth width,
        const std::uint32_t value) {
        switch (width) {
        case MemoryTransferElementWidth::byte:
            write_byte(address, static_cast<std::uint8_t>(value)); return;
        case MemoryTransferElementWidth::word:
            write_word(address, static_cast<std::uint16_t>(value)); return;
        case MemoryTransferElementWidth::longword:
            write_long(address, value); return;
        }
        throw std::runtime_error("Invalid Deuteros VERTB write width");
    }

    [[nodiscard]] std::vector<DeuterosAmigaInterruptMemoryWrite> changed_bytes() const {
        std::vector<DeuterosAmigaInterruptMemoryWrite> result;
        result.reserve(changes_.size());
        for (const auto& [address, value] : changes_) {
            const auto original = originals_.find(address);
            if (original == originals_.end()) {
                throw std::runtime_error("Deuteros interrupt worker wrote an unread byte");
            }
            if (original->second != value) {
                result.push_back({result.size() + 1, address, value});
            }
        }
        return result;
    }

private:
    ReadByte read_byte_;
    std::map<std::uint32_t, std::uint8_t> originals_;
    std::map<std::uint32_t, std::uint8_t> changes_;
};

void append_write(std::vector<DeuterosAmigaInterruptRegisterWrite>& writes,
    const std::uint32_t address, const DeuterosAmigaInterruptRegisterWidth width,
    const std::uint32_t value) {
    writes.push_back({writes.size() + 1, address, width, value});
}

void validate_worker(const DeuterosAmigaInstalledInterruptWorker& worker) {
    constexpr std::string_view expected_hash =
        "661854d6976ab520b0398e2545003d3fe59692fc0de54f0f810f379cf25ccaf8";
    if (worker.caller_address != 0x224ea || worker.entry_address != 0x22816
        || worker.source_disk_offset != 0x8016 || worker.source_length != 0x1d2
        || worker.return_instruction_address != 0x229e6
        || worker.raw_sha256 != expected_hash) {
        throw std::runtime_error("Unexpected Deuteros installed interrupt worker identity");
    }
}

DeuterosAmigaInstalledInterruptWorkerSparseResult translate_worker(
    const DeuterosAmigaInstalledInterruptWorker& worker, SparseMemory& memory) {
    validate_worker(worker);
    DeuterosAmigaInstalledInterruptWorkerSparseResult result;
    result.register_writes.reserve(17);

    constexpr std::array<std::uint32_t, 4> addend_cells{0x229ea, 0x229ec, 0x229ee, 0x229f0};
    constexpr std::array<std::uint32_t, 4> accumulator_cells{0x22a02, 0x22a04, 0x22a06, 0x22a08};
    constexpr std::array<std::uint32_t, 4> period_registers{0xdff0a6, 0xdff0b6, 0xdff0c6, 0xdff0d6};
    for (std::size_t index = 0; index < addend_cells.size(); ++index) {
        const auto sum = static_cast<std::uint16_t>(memory.read_word(accumulator_cells[index])
            + memory.read_word(addend_cells[index]));
        memory.write_word(accumulator_cells[index], sum);
        append_write(result.register_writes, period_registers[index],
            DeuterosAmigaInterruptRegisterWidth::word, sum);
    }

    constexpr std::array<std::uint32_t, 4> pointer_cells{0x229f2, 0x229f6, 0x229fa, 0x229fe};
    constexpr std::array<std::uint32_t, 4> location_registers{0xdff0a0, 0xdff0b0, 0xdff0c0, 0xdff0d0};
    constexpr std::array<std::uint32_t, 4> length_registers{0xdff0a4, 0xdff0b4, 0xdff0c4, 0xdff0d4};
    auto flags = memory.read(0x229e8);
    for (std::size_t index = 0; index < pointer_cells.size(); ++index) {
        const auto mask = static_cast<std::uint8_t>(1U << index);
        if ((flags & mask) == 0) continue;
        flags = static_cast<std::uint8_t>(flags & static_cast<std::uint8_t>(~mask));
        memory.write_byte(0x229e8, flags);
        const auto pointer = memory.read_long(pointer_cells[index]);
        const auto location_address = static_cast<std::uint64_t>(pointer) + 8U;
        const auto length_address = static_cast<std::uint64_t>(pointer) + 12U;
        if (length_address > std::numeric_limits<std::uint32_t>::max()) {
            throw std::runtime_error("Deuteros interrupt worker pointer addition overflows");
        }
        append_write(result.register_writes, location_registers[index],
            DeuterosAmigaInterruptRegisterWidth::longword,
            memory.read_long(static_cast<std::uint32_t>(location_address)));
        append_write(result.register_writes, length_registers[index],
            DeuterosAmigaInterruptRegisterWidth::word,
            memory.read_word(static_cast<std::uint32_t>(length_address)));
        memory.write_long(pointer_cells[index], 0);
    }

    std::uint16_t enable_word = 0x8000;
    flags = memory.read(0x229e8);
    for (std::size_t index = 0; index < pointer_cells.size(); ++index) {
        if (memory.read_long(pointer_cells[index]) == 0) continue;
        const auto mask = static_cast<std::uint8_t>(1U << index);
        flags = static_cast<std::uint8_t>(flags | mask);
        enable_word = static_cast<std::uint16_t>(enable_word | mask);
    }
    memory.write_byte(0x229e8, flags);
    append_write(result.register_writes, 0xdff096,
        DeuterosAmigaInterruptRegisterWidth::word, enable_word);

    const auto scale = memory.read_word(0x2229a);
    constexpr std::array<std::uint32_t, 4> volume_cells{0x22a0a, 0x22a0c, 0x22a0e, 0x22a10};
    constexpr std::array<std::uint32_t, 4> volume_registers{0xdff0a8, 0xdff0b8, 0xdff0c8, 0xdff0d8};
    for (std::size_t index = 0; index < volume_cells.size(); ++index) {
        const auto product = static_cast<std::uint32_t>(scale) * memory.read_word(volume_cells[index]);
        // MULU.W fills D0.L; the following LSR.W affects only D0's low word.
        const auto value = static_cast<std::uint16_t>((product & 0xffffU) >> 8U);
        append_write(result.register_writes, volume_registers[index],
            DeuterosAmigaInterruptRegisterWidth::word, value);
    }
    result.memory_writes = memory.changed_bytes();
    return result;
}

} // namespace

DeuterosAmigaVertbSession::DeuterosAmigaVertbSession(
    DeuterosAmigaVertbHandlerProfile profile) : profile_(std::move(profile)) {
    if (profile_.entry_address != 0x207ec || profile_.source_disk_offset != 0x5fec
        || profile_.source_length != 0x100
        || profile_.raw_sha256 !=
            "21356e0e8cfe8b1323a7ffa3a59c8b98720794330251862e9978447c91006d0f") {
        throw std::runtime_error("Unexpected Deuteros VERTB handler profile");
    }
}

void DeuterosAmigaVertbSession::observe_registration(
    const DeuterosAmigaVertbRegistrationObservation& observation) {
    if (registration_sequence_ != 0 || observation.trace_sequence == 0
        || observation.call_address != 0x208b4 || observation.return_address != 0x208b8
        || observation.vector != -0xa8 || observation.interrupt_level != 5
        || observation.interrupt_node != 0x2086a || observation.handler_address != 0x207ec) {
        throw std::runtime_error("Deuteros VERTB registration return does not match source");
    }
    registration_sequence_ = observation.trace_sequence;
}

DeuterosAmigaVertbCounterEffect DeuterosAmigaVertbSession::observe_idle_invocation(
    const DeuterosAmigaVertbInvocationObservation& observation) {
    if (registration_sequence_ == 0 || observation.trace_sequence <= registration_sequence_
        || observation.trace_sequence <= last_invocation_sequence_
        || observation.instruction_address != profile_.entry_address
        || observation.buffer_update_enabled != 0 || observation.sound_enabled != 0) {
        throw std::runtime_error("Deuteros VERTB invocation is not an idle registered entry");
    }
    last_invocation_sequence_ = observation.trace_sequence;
    return {observation.trace_sequence, 0x2079e, observation.counter_before,
        static_cast<std::uint32_t>(observation.counter_before + 4U)};
}

DeuterosAmigaVertbTranslation DeuterosAmigaVertbSession::observe_invocation(
    const std::uint64_t trace_sequence, const std::uint32_t instruction_address,
    const std::function<std::optional<std::uint8_t>(std::uint32_t)>& read_owned_byte) {
    if (registration_sequence_ == 0 || trace_sequence <= registration_sequence_
        || trace_sequence <= last_invocation_sequence_) {
        throw std::runtime_error("Deuteros VERTB entry precedes registration or repeats");
    }
    auto result = evaluate_deuteros_amiga_vertb_entry(profile_, trace_sequence,
        instruction_address, read_owned_byte);
    last_invocation_sequence_ = trace_sequence;
    return result;
}

DeuterosAmigaVertbTranslation evaluate_deuteros_amiga_vertb_entry(
    const DeuterosAmigaVertbHandlerProfile& profile, const std::uint64_t trace_sequence,
    const std::uint32_t instruction_address,
    const std::function<std::optional<std::uint8_t>(std::uint32_t)>& read_owned_byte) {
    constexpr std::string_view handler_hash =
        "21356e0e8cfe8b1323a7ffa3a59c8b98720794330251862e9978447c91006d0f";
    constexpr std::string_view audio_hash =
        "ac71cc9f850f2b1165a42d7583d22c8ef7091fbc960f5610537311ee732b3dc7";
    if (profile.entry_address != 0x207ec || profile.source_disk_offset != 0x5fec
        || profile.source_length != 0x100 || profile.raw_sha256 != handler_hash
        || trace_sequence == 0 || instruction_address != profile.entry_address
        || !read_owned_byte) {
        throw std::runtime_error("Deuteros VERTB entry identity is invalid");
    }
    SparseMemory memory(read_owned_byte);
    std::array<std::uint8_t, 0x100> handler_bytes{};
    for (std::size_t index = 0; index < handler_bytes.size(); ++index) {
        handler_bytes[index] = memory.read(profile.entry_address
            + static_cast<std::uint32_t>(index));
    }
    if (to_hex(sha256(handler_bytes)) != handler_hash) {
        throw std::runtime_error("Owned Deuteros VERTB handler bytes differ from admitted source");
    }

    auto counter = memory.read_long(0x2079e);
    counter = static_cast<std::uint32_t>(counter + 4U);
    memory.write_long(0x2079e, counter);

    if (memory.read(0x207ea) != 0) {
        auto buffer_index = static_cast<std::uint16_t>(memory.read_word(0x207b0) + 1U);
        memory.write_word(0x207b0, buffer_index);
        buffer_index = static_cast<std::uint16_t>(buffer_index & 3U);
        const auto record = 0x207b2U + static_cast<std::uint32_t>(buffer_index) * 10U;
        const auto first_a = memory.read_word(record);
        const auto first_b = memory.read_word(record + 2U);
        const auto second_a = memory.read_word(record + 4U);
        const auto second_b = memory.read_word(record + 6U);
        const auto buffer_a = memory.read_long(0x2197a);
        const auto buffer_b = memory.read_long(0x2197e);
        const auto checked_destination = [](const std::uint32_t pointer) {
            if (pointer > std::numeric_limits<std::uint32_t>::max() - 0x30U
                || ((pointer + 0x2aU) & 1U) != 0) {
                throw std::runtime_error("Deuteros VERTB buffer pointer is invalid");
            }
            return pointer + 0x2aU;
        };
        const auto destination_a = checked_destination(buffer_a);
        const auto destination_b = checked_destination(buffer_b);
        memory.write_word(destination_a, first_a);
        memory.write_word(destination_a + 4U, first_b);
        memory.write_word(destination_b, second_a);
        memory.write_word(destination_b + 4U, second_b);
    }

    std::vector<DeuterosAmigaInterruptRegisterWrite> register_writes;
    if (memory.read(0x207eb) != 0) {
        std::array<std::uint8_t, 0x438> audio_bytes{};
        for (std::size_t index = 0; index < audio_bytes.size(); ++index) {
            audio_bytes[index] = memory.read(0x22beaU + static_cast<std::uint32_t>(index));
        }
        if (to_hex(sha256(audio_bytes)) != audio_hash) {
            throw std::runtime_error("Owned Deuteros $22bea consumer bytes differ from admitted source");
        }
        const auto read = [&memory](const std::uint32_t address,
            const std::uint32_t width) {
            if (width == 1) return memory.read_value(address, MemoryTransferElementWidth::byte);
            if (width == 2) return memory.read_value(address, MemoryTransferElementWidth::word);
            if (width == 4) return memory.read_value(address, MemoryTransferElementWidth::longword);
            throw std::runtime_error("Deuteros VERTB audio read width is unsupported");
        };
        const auto write = [&memory, &register_writes](const std::uint32_t address,
            const MemoryTransferElementWidth width, const std::uint32_t value) {
            if (address >= 0xdff000U && address < 0xdff200U) {
                const auto register_width = width == MemoryTransferElementWidth::longword
                    ? DeuterosAmigaInterruptRegisterWidth::longword
                    : DeuterosAmigaInterruptRegisterWidth::word;
                append_write(register_writes, address, register_width, value);
            } else {
                memory.write_value(address, width, value);
            }
        };
        static_cast<void>(consume_deuteros_amiga_owned_audio(read, write));
    }

    return {trace_sequence, memory.changed_bytes(), std::move(register_writes)};
}

DeuterosAmigaVertbWaitScenarioReport
evaluate_deuteros_amiga_vertb_wait_scenarios(
    const std::span<const std::uint8_t> source_wait_bytes,
    const std::span<const DeuterosAmigaScenarioMemoryCell> owned_memory) {
    constexpr std::uint32_t wait_address = 0x216f2;
    constexpr std::uint32_t port_address = 0xdff01f;
    constexpr std::uint8_t vertb_bit = 5;
    constexpr std::string_view expected_hash =
        "3ee8aeaeb214bfb7cf210c7575043595d87340370ea8f85eda94eb51d53961a7";
    constexpr std::size_t wait_length = 12;
    if (source_wait_bytes.size() != wait_length
        || to_hex(sha256(source_wait_bytes)) != expected_hash) {
        throw std::runtime_error("Unsupported Deuteros synthetic VERTB wait source");
    }

    // Materialize a sparse private clone of the supplied owned image. It is
    // used only to check that the caller's source-bound wait bytes are the
    // ones present in the selected snapshot; no cell is ever written back.
    std::map<std::uint32_t, std::uint8_t> cloned_memory;
    std::vector<std::uint8_t> image_digest_input;
    image_digest_input.reserve(owned_memory.size() * 5U);
    for (const auto& cell : owned_memory) {
        if (cell.address > 0xffffffU
            || !cloned_memory.emplace(cell.address, cell.value).second) {
            throw std::runtime_error("Invalid or duplicate Deuteros synthetic scenario memory cell");
        }
    }
    for (const auto& [address, value] : cloned_memory) {
        image_digest_input.push_back(static_cast<std::uint8_t>(address >> 24U));
        image_digest_input.push_back(static_cast<std::uint8_t>(address >> 16U));
        image_digest_input.push_back(static_cast<std::uint8_t>(address >> 8U));
        image_digest_input.push_back(static_cast<std::uint8_t>(address));
        image_digest_input.push_back(value);
    }
    for (std::size_t index = 0; index < wait_length; ++index) {
        const auto found = cloned_memory.find(wait_address + static_cast<std::uint32_t>(index));
        if (found == cloned_memory.end() || found->second != source_wait_bytes[index]) {
            throw std::runtime_error("Deuteros synthetic scenario lacks matching owned wait bytes");
        }
    }

    DeuterosAmigaVertbWaitScenarioReport result;
    result.source_sha256 = std::string(expected_hash);
    result.owned_image_sha256 = to_hex(sha256(image_digest_input));
    constexpr std::array<std::uint8_t, 2> synthetic_values{0x00, 0x20};
    constexpr std::array<std::string_view, 2> labels{
        "synthetic-bit-clear", "synthetic-bit-set"};
    for (std::size_t index = 0; index < synthetic_values.size(); ++index) {
        auto scenario_memory = cloned_memory;
        scenario_memory[port_address] = synthetic_values[index];
        const auto register_value = scenario_memory.at(port_address);
        const bool bit_set = ((register_value >> vertb_bit) & 1U) != 0;
        const bool remains_in_wait = !bit_set;
        result.scenarios[index] = {std::string(labels[index]), register_value, bit_set,
            remains_in_wait, remains_in_wait ? wait_address : 0x216fc};
    }
    result.missing_runtime_evidence = {{
        "AddIntServer return at $208b4->$208b8: vector -$a8, level 5, node $2086a, handler $207ec",
        "ordered registered handler entry at $207ec and its owned counter preimage/result at $2079e",
        "LoadView call/return $216ee/-$de->$216f2 with owned A1 view and A6 graphics.library base",
        "ordered real INTREQR sample at $216f2/$dff01f bit 5, including each repeated clear-bit poll",
    }};
    return result;
}

DeuterosAmigaInstalledInterruptWorkerSparseResult
evaluate_deuteros_amiga_installed_interrupt_worker_sparse(
    const DeuterosAmigaInstalledInterruptWorker& worker,
    const std::function<std::optional<std::uint8_t>(std::uint32_t)>& read_byte) {
    if (!read_byte) throw std::runtime_error("Deuteros interrupt worker requires an owned-memory reader");
    SparseMemory memory(read_byte);
    return translate_worker(worker, memory);
}

DeuterosAmigaInstalledInterruptWorkerResult
evaluate_deuteros_amiga_installed_interrupt_worker(
    const DeuterosAmigaInstalledInterruptWorker& worker,
    const std::uint32_t memory_base_address,
    const std::span<const std::uint8_t> memory) {
    if (memory.empty() || static_cast<std::uint64_t>(memory_base_address) + memory.size()
            > (std::uint64_t{1} << 32U)) {
        throw std::runtime_error("Invalid Deuteros interrupt worker scenario memory range");
    }
    auto translated = evaluate_deuteros_amiga_installed_interrupt_worker_sparse(worker,
        [memory_base_address, memory](const std::uint32_t address)
            -> std::optional<std::uint8_t> {
            if (address < memory_base_address) return std::nullopt;
            const auto offset = static_cast<std::uint64_t>(address) - memory_base_address;
            if (offset >= memory.size()) return std::nullopt;
            return memory[static_cast<std::size_t>(offset)];
        });
    DeuterosAmigaInstalledInterruptWorkerResult result{memory_base_address,
        {memory.begin(), memory.end()}, std::move(translated.register_writes)};
    for (const auto& write : translated.memory_writes) {
        if (write.address < memory_base_address
            || static_cast<std::uint64_t>(write.address) - memory_base_address >= result.memory.size()) {
            throw std::runtime_error("Deuteros interrupt worker write escaped scenario memory");
        }
        result.memory[static_cast<std::size_t>(write.address - memory_base_address)] = write.value;
    }
    return result;
}

} // namespace eon
