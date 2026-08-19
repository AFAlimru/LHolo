#pragma once

#include <filesystem>
#include <optional>

namespace lholo::ui {

std::optional<std::filesystem::path> openStructureFile(std::filesystem::path const& current);
std::optional<std::filesystem::path> saveMcstructureFile();

} // namespace lholo::ui
