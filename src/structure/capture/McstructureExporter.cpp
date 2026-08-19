#include "structure/capture/McstructureExporter.h"

#include "mc/deps/core/file/Path.h"
#include "mc/world/level/levelgen/structure/StructureManager.h"
#include "mc/world/level/levelgen/structure/StructureTemplate.h"

namespace lholo::structure::capture {

bool exportMcstructure(StructureTemplate const& structure, std::filesystem::path const& output) {
    return StructureManager::exportStructure(structure, Core::Path{output});
}

} // namespace lholo::structure::capture
