// LHolo - Client-side projection renderer for Minecraft Bedrock Windows
// Copyright (C) 2026  MarmieQi
//
// Bounded real-world comparison for projected cells: initial scan, block and
// subchunk refreshes, four-state correction, HUD counts and six-neighbor dirty
// propagation. Mesh generation and HUD publication remain outside this module.

#pragma once

class BlockSource;
class LegacyStructureSettings;

namespace lholo::projection::detail {

struct ProjectionState;

struct CorrectionProgressChanges {
    bool overall{};
    bool visible{};
    bool errors{};
};

CorrectionProgressChanges updateCorrectionTracker(
    ProjectionState&                state,
    BlockSource&                    region,
    LegacyStructureSettings const& transformSettings,
    int                             mirrorMode,
    int                             rotationTurns,
    int                             offsetX,
    int                             offsetY,
    int                             offsetZ,
    int                             layerDisplayMode,
    int                             displayLayer,
    int                             layerAxis
);

} // namespace lholo::projection::detail
