// LHolo - Client-side projection renderer for Minecraft Bedrock Windows
// Copyright (C) 2026  MarmieQi
//
// Collection boundary for real-world block and subchunk notifications. This
// module queues facts only; Projection.cpp decides how they invalidate meshes.

#pragma once

#include "projection/ProjectionInternalTypes.h"

#include <cstddef>
#include <vector>

#include "mc/world/level/BlockPos.h"

class BlockSource;
class Level;

namespace lholo::projection::detail {

void attachProjectionWorldEvents(Level& level, BlockSource& blockSource);
void detachProjectionWorldEvents();

std::vector<BlockPos>    takePendingBlockChanges(std::size_t limit);
std::vector<SubChunkKey> takePendingLoadedSubChunks(std::size_t limit);

} // namespace lholo::projection::detail
