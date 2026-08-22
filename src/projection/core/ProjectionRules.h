// LHolo - Client-side projection renderer for Minecraft Bedrock Windows
// Copyright (C) 2026  MarmieQi
//
// Pure projection rules. Functions in this module do not read projection
// runtime state and do not own Minecraft or rendering resources.

#pragma once

#include "projection/core/ProjectionInternalTypes.h"
#include "structure/StructureLoader.h"

#include "mc/util/Mirror.h"
#include "mc/util/Rotation.h"
#include "mc/world/level/block/BlockRenderLayer.h"

class Block;
class LegacyStructureSettings;

namespace lholo::projection::detail {

RenderBucket renderBucketFor(BlockRenderLayer layer);

Mirror   getProjectionMirror(int mirrorMode);
Rotation getProjectionRotation(int quarterTurns);

Block const* transformExpectedBlock(
    Block const*                   block,
    LegacyStructureSettings const& settings,
    bool                           identityTransform
);

bool projectionStatesMatch(Block const& expected, Block const& actual);
int  blockFrontFace(Block const& block);

BlockPos transformStructurePosition(
    structure::LoadedStructure::RenderBlock const& entry,
    structure::LoadedStructure const&              loaded,
    int                                             mirrorMode,
    int                                             rotation
);

bool isLayerVisible(int layer, int layerDisplayMode, int displayLayer);

} // namespace lholo::projection::detail
