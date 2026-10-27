#pragma once
#include "structure/StructureSession.h"
#include <filesystem>
#include <optional>

namespace lholo::app::detail {
std::filesystem::path taskStorePath();
std::optional<structure::detail::StructureLoadOrigin> taskPlayerOrigin();
}
