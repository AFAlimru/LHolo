// LHolo - Client-side projection renderer for Minecraft Bedrock Windows
// Copyright (C) 2026  MarmieQi

#include "projection/runtime/ProjectionFramePipeline.h"

#include "projection/correction/ProjectionCorrectionTracker.h"
#include "projection/mesh/ProjectionMeshScheduler.h"
#include "projection/mesh/ProjectionMeshUpload.h"
#include "projection/runtime/ProjectionProgress.h"
#include "projection/runtime/ProjectionVisibility.h"
#include "projection/runtime/ProjectionSession.h"
#include "projection/core/ProjectionState.h"
#include "structure/StructureLoader.h"
#include "render/BoxWireframeMesh.h"
#include <vector>

#include "mc/client/renderer/block/BlockTessellator.h"
#include "mc/world/level/BlockSource.h"
#include "mc/world/level/levelgen/structure/LegacyStructureSettings.h"

namespace lholo::projection::detail {
namespace {

void ensureStructureBoundsMesh(
    ProjectionState& state,
    Tessellator&     tessellator,
    int              rotationTurns
) {
    // Cache 24 line vertices per visible file in one GPU batch. Bounds are
    // independent of winner cells, slicing and the selected editor file.
    if (state.structureBoundsMesh) return;

    if (state.structure->composed) {
        std::vector<render::BoxWireframeBounds> boxes;
        boxes.reserve(state.structure->sourceBounds.size());
        for (auto const& bounds : state.structure->sourceBounds)
            boxes.push_back({
                {static_cast<float>(bounds.minimum[0]), static_cast<float>(bounds.minimum[1]),
                 static_cast<float>(bounds.minimum[2])},
                {static_cast<float>(bounds.maximum[0]), static_cast<float>(bounds.maximum[1]),
                 static_cast<float>(bounds.maximum[2])}});
        state.structureBoundsMesh = render::buildBoxWireframesMesh(tessellator, boxes, "LHoloFileBounds");
        return;
    }

    auto const rotated = rotationTurns == 1 || rotationTurns == 3;
    auto const width = static_cast<float>(
        rotated ? state.structure->sizeZ : state.structure->sizeX
    );
    auto const height = static_cast<float>(state.structure->sizeY);
    auto const depth = static_cast<float>(
        rotated ? state.structure->sizeX : state.structure->sizeZ
    );
    state.structureBoundsMesh = render::buildBoxWireframeMesh(tessellator,
        {0,0,0}, {width,height,depth}, "LHoloStructureBounds");
}

} // namespace

void processProjectionOpaqueFrame(
    ProjectionState&                      state,
    BaseActorRenderContext&               renderContext,
    Tessellator&                          tessellator,
    BlockSource&                          region,
    Vec3 const&                           cameraPosition,
    LegacyStructureSettings const&        transformSettings,
    ProjectionSectionBuildSettings const& buildSettings,
    structure::LayerDisplayMode           layerDisplayMode,
    int                                   displayLayer,
    structure::LayerAxis                  layerAxis
) {
    auto& blockTessellator = *state.blockTessellator;
    blockTessellator.setRegion(region);

    auto const correctionChanges = updateCorrectionTracker(
        state,
        region,
        transformSettings,
        buildSettings.mirrorMode,
        buildSettings.rotationTurns,
        buildSettings.offsetX,
        buildSettings.offsetY,
        buildSettings.offsetZ,
        layerDisplayMode,
        displayLayer,
        layerAxis
    );
    if (correctionChanges.overall) {
        publishPlacedProgress(state.progressCorrectCount);
    }
    if (correctionChanges.visible) {
        publishVisiblePlacedProgress(state.progressVisibleCorrectCount);
    }
    if (correctionChanges.errors) {
        publishErrorProgress(
            state.progressWrongTypeCount,
            state.progressWrongStateCount,
            state.progressExtraCount
        );
    }

    // Consume completed CPU data only in the opaque render pass.
    uploadCompletedProjectionMeshes(state, tessellator);
    // Classify first so pending visible tint work has its own retry budget.
    updateProjectionVisibility(state, renderContext, cameraPosition);
    refreshProjectionBiomes(state.sections, state.biomeRefreshVisibleCursor, state.biomeRefreshCursor,
        [&](ProjectionBiomeRefresh const& refresh) {
            return region.areChunksFullyLoaded(
                BlockPos{refresh.minimum[0], refresh.minimum[1], refresh.minimum[2]},
                BlockPos{refresh.maximum[0], refresh.maximum[1], refresh.maximum[2]});
        },
        [&](std::size_t section) {
            auto& target = state.sections[section];
            target.dirty = true;
            target.incrementalDirty = true;
            ++target.requestedRevision;
        }, ++state.biomeRefreshFrame);
    if (ProjectionSession::getInstance().structureBoundsEnabled())
        ensureStructureBoundsMesh(state, tessellator, buildSettings.rotationTurns);
    scheduleProjectionMeshBuild(
        state, tessellator, region, cameraPosition, buildSettings
    );
    buildNextProjectionSectionSynchronously(
        state, tessellator, blockTessellator, region, cameraPosition, buildSettings
    );
    if (!state.asyncMeshBuildingEnabled) {
        updateProjectionVisibility(state, renderContext, cameraPosition);
    }
}

} // namespace lholo::projection::detail
