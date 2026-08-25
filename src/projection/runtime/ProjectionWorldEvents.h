// LHolo - Client-side projection renderer for Minecraft Bedrock Windows
// Copyright (C) 2026  MarmieQi
//
// Collection boundary for real-world block and subchunk notifications. This
// module queues facts only; downstream projection stages decide how to apply
// them.

#pragma once

#include "projection/core/ProjectionInternalTypes.h"

#include <cstddef>
#include <cstdint>
#include <vector>

#include "mc/world/level/BlockPos.h"

class BlockSource;
class Level;

namespace lholo::projection::detail {

struct PendingBlockChange {
    BlockPos      position{};
    std::uint64_t destroyedAt{};
};

void attachProjectionWorldEvents(Level& level, BlockSource& blockSource);
void detachProjectionWorldEvents();

std::vector<PendingBlockChange> takePendingBlockChanges(std::size_t limit);
std::vector<SubChunkKey> takePendingLoadedSubChunks(std::size_t limit);

} // namespace lholo::projection::detail
