#pragma once

#include "data/native_code_image_admission.hpp"
#include "engine/runtime_session.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

namespace eon {

// Copy-only, media-safe diagnostics for the declarative native-code registry.
// No source hash, release hash, path, byte span, or media owner crosses this
// boundary. `active` is present only where one runtime session kind identifies
// one exact manifest range without inference.
struct ActiveNativeCodeImageDiagnostics {
    std::string image_id;
    std::string range_id;
    NativeCodeAddressBasis address_basis = NativeCodeAddressBasis::runtime_absolute;
    NativeCodeLoadStatus load_status = NativeCodeLoadStatus::unproven;
};

struct NativeCodeImageRegistryDiagnostics {
    std::size_t mapped_descriptor_count = 0;
    std::size_t excluded_image_count = 0;
    std::optional<ActiveNativeCodeImageDiagnostics> active;
};

[[nodiscard]] std::string_view native_code_address_basis_label(NativeCodeAddressBasis basis);
[[nodiscard]] std::string_view native_code_load_status_label(NativeCodeLoadStatus status);
[[nodiscard]] NativeCodeImageRegistryDiagnostics native_code_image_registry_diagnostics(
    const std::optional<RuntimeSessionSnapshot>& session);

} // namespace eon
