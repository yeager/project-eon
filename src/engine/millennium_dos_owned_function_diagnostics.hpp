#pragma once

#include "engine/runtime_session.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

namespace eon {

enum class MillenniumDosOwnedFunctionBoundaryKind {
    runtime_word, runtime_byte, call_return, register_value, local_return,
};

struct MillenniumDosOwnedFunctionBoundaryDiagnostic {
    MillenniumDosOwnedFunctionBoundaryKind kind =
        MillenniumDosOwnedFunctionBoundaryKind::runtime_word;
    std::uint16_t instruction_address = 0;
    std::optional<std::uint16_t> runtime_address;
    std::optional<std::uint32_t> call_target;
};

struct MillenniumDosOwnedFunctionDiagnosticInput {
    RuntimeSessionSnapshot session;
    std::string game_executable_sha256;
    std::size_t function_key_index = 0;
    std::uint16_t handler_address = 0;
    MillenniumDosOwnedFunctionBoundaryDiagnostic boundary;
};

struct MillenniumDosOwnedFunctionDiagnostics {
    std::string release_sha256;
    std::string game_executable_sha256;
    RuntimeSessionKind session_kind = RuntimeSessionKind::millennium_dos_sixth_function;
    std::string function_id;
    std::size_t function_key_index = 0;
    std::uint16_t handler_address = 0;
    MillenniumDosOwnedFunctionBoundaryDiagnostic boundary;
    std::string mode = "typed-observation";
};

[[nodiscard]] std::optional<MillenniumDosOwnedFunctionDiagnostics>
make_millennium_dos_owned_function_diagnostics(
    const MillenniumDosOwnedFunctionDiagnosticInput& input);

} // namespace eon
