#pragma once

#include <filesystem>

class StructureTemplate;

namespace lholo::structure::capture {

bool exportMcstructure(StructureTemplate const& structure, std::filesystem::path const& output);

} // namespace lholo::structure::capture
