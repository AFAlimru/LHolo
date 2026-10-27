#pragma once
#include "structure/StructureSession.h"
#include <filesystem>
#include <cstdint>
#include <optional>

namespace lholo::app::detail {
std::filesystem::path taskStorePath();
std::optional<structure::detail::StructureLoadOrigin> taskPlayerOrigin();
std::uint64_t taskWorldRevision();
}
