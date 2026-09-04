#pragma once

#include "platform/game_data.hpp"

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace eon {

enum class NativeCodeAddressBasis { dos_com_linear_0x100, runtime_absolute,
                                    image_relative_unrelocated, disk_relative };
enum class NativeCodeLoadStatus { address_basis_declared, unproven };

struct NativeCodeImageDescriptor {
    std::string_view release_sha256;
    std::string_view image_id;
    std::string_view range_id;
    std::string_view source_sha256;
    std::uint64_t source_offset;
    std::uint64_t length;
    NativeCodeAddressBasis address_basis;
    NativeCodeLoadStatus load_status;
};

struct NativeCodeImageView {
    NativeCodeImageDescriptor descriptor;
    std::span<const std::uint8_t> bytes;
};

struct NativeCodeImageAdmissionResult {
    std::optional<NativeCodeImageView> view;
    std::string error;
    [[nodiscard]] bool accepted() const { return view.has_value(); }
};

[[nodiscard]] std::span<const NativeCodeImageDescriptor> native_code_image_manifest();
[[nodiscard]] NativeCodeImageAdmissionResult admit_native_code_image(
    const VerifiedReleaseMedia& media, std::string_view image_id,
    std::string_view range_id);

} // namespace eon
