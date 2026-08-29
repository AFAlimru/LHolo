// LHolo - Client-side projection renderer for Minecraft Bedrock Windows
// Copyright (C) 2026  MarmieQi

#include "projection/runtime/ProjectionFramePipeline.h"

#include "projection/correction/ProjectionCorrectionTracker.h"
#include "projection/mesh/ProjectionMeshScheduler.h"
#include "projection/mesh/ProjectionMeshUpload.h"
#include "projection/runtime/ProjectionProgress.h"
#include "projection/runtime/ProjectionSession.h"
#include "projection/core/ProjectionState.h"
#include "structure/StructureLoader.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <memory>
#include <tuple>
#include <vector>

#include "mc/client/renderer/Tessellator.h"
#include "mc/client/renderer/block/BlockTessellator.h"
#include "mc/world/level/BlockPos.h"
#include "mc/world/level/BlockSource.h"
#include "mc/world/level/block/Block.h"
#include "mc/world/level/levelgen/structure/LegacyStructureSettings.h"

namespace lholo::projection::detail {
namespace {

void ensureStructureBoundsMesh(
    ProjectionState& state,
    Tessellator&     tessellator,
    int              rotationTurns
) {
    // The bounds are only 24 line vertices and are intentionally generated
    // once on the render thread; section BlockTessellator work stays async.
    if (!state.asyncMeshBuildingEnabled || state.structureBoundsMesh) return;

    auto const rotated = rotationTurns == 1 || rotationTurns == 3;
    auto const width = static_cast<float>(
        rotated ? state.structure->sizeZ : state.structure->sizeX
    );
    auto const height = static_cast<float>(state.structure->sizeY);
    auto const depth = static_cast<float>(
        rotated ? state.structure->sizeX : state.structure->sizeZ
    );
    constexpr float expansion = 0.01f;
    float const x0 = -expansion, y0 = -expansion, z0 = -expansion;
    float const x1 = width + expansion, y1 = height + expansion, z1 = depth + expansion;
    tessellator.begin(
        Tessellator::DebugContextCallback{}, mce::PrimitiveMode::LineList, 24, false
    );
    tessellator.colorABGR(static_cast<int>(0xFFFFD633U));
    auto addBoundsEdge = [&](Vec3 const& first, Vec3 const& second) {
        tessellator.vertex(first);
        tessellator.vertex(second);
    };
    addBoundsEdge({x0,y0,z0},{x1,y0,z0}); addBoundsEdge({x1,y0,z0},{x1,y1,z0});
    addBoundsEdge({x1,y1,z0},{x0,y1,z0}); addBoundsEdge({x0,y1,z0},{x0,y0,z0});
    addBoundsEdge({x0,y0,z1},{x1,y0,z1}); addBoundsEdge({x1,y0,z1},{x1,y1,z1});
    addBoundsEdge({x1,y1,z1},{x0,y1,z1}); addBoundsEdge({x0,y1,z1},{x0,y0,z1});
    addBoundsEdge({x0,y0,z0},{x0,y0,z1}); addBoundsEdge({x1,y0,z0},{x1,y0,z1});
    addBoundsEdge({x1,y1,z0},{x1,y1,z1}); addBoundsEdge({x0,y1,z0},{x0,y1,z1});
    state.structureBoundsMesh = std::make_unique<mce::Mesh>(tessellator.end(
        Tessellator::UploadMode::Buffered,
        "LHoloStructureBounds",
        Tessellator::SupplementaryFieldAutoGenerationMode::None
    ));
}

// Real world blocks sitting where the projection expects air, scanned in a cube
// around the player and outlined magenta. Runs on the render thread (like the
// bounds mesh), throttled, and bounded to the structure's world box so ordinary
// world blocks outside the projection are never flagged.
void ensureExtraBlockMesh(
    ProjectionState& state,
    Tessellator&     tessellator,
    BlockSource&     region,
    Vec3 const&      camera,
    BlockPos const&  renderOrigin,
    int              rotationTurns
) {
    if (!ProjectionSession::getInstance().extraBlocksEnabled() || !state.structure) {
        state.extraBlockMesh.reset();
        return;
    }
    auto const now = static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()
        ).count()
    );
    constexpr std::uint64_t kScanIntervalMs = 200;
    if (state.extraBlockMesh && now - state.extraBlockScanAt < kScanIntervalMs) return;
    state.extraBlockScanAt = now;

    // Structure world box: renderOrigin + transformed size (rotation swaps X/Z).
    bool const rotated = rotationTurns == 1 || rotationTurns == 3;
    int const minX = renderOrigin.x;
    int const minY = renderOrigin.y;
    int const minZ = renderOrigin.z;
    int const maxX = minX + (rotated ? state.structure->sizeZ : state.structure->sizeX);
    int const maxY = minY + state.structure->sizeY;
    int const maxZ = minZ + (rotated ? state.structure->sizeX : state.structure->sizeZ);

    constexpr int kScanRadius = 6;
    int const camX = static_cast<int>(std::floor(camera.x));
    int const camY = static_cast<int>(std::floor(camera.y));
    int const camZ = static_cast<int>(std::floor(camera.z));

    std::vector<BlockPos> extras;
    for (int x = std::max(minX, camX - kScanRadius); x < std::min(maxX, camX + kScanRadius + 1); ++x) {
        for (int y = std::max(minY, camY - kScanRadius); y < std::min(maxY, camY + kScanRadius + 1); ++y) {
            for (int z = std::max(minZ, camZ - kScanRadius); z < std::min(maxZ, camZ + kScanRadius + 1); ++z) {
                // A structure block cell is handled by the normal correction scan.
                if (state.expectedWorldBlockIndices->find(std::tuple{x, y, z})
                    != state.expectedWorldBlockIndices->end()) {
                    continue;
                }
                BlockPos const pos{x, y, z};
                if (region.getBlock(pos).isAir()) continue;
                extras.push_back(pos);
            }
        }
    }

    if (extras.empty()) {
        state.extraBlockMesh.reset();
        return;
    }

    tessellator.begin(
        Tessellator::DebugContextCallback{}, mce::PrimitiveMode::LineList,
        static_cast<int>(extras.size() * 24), false
    );
    tessellator.colorABGR(static_cast<int>(0xFFFF00FFU)); // magenta, opaque
    auto edge = [&](Vec3 const& a, Vec3 const& b) {
        tessellator.vertex(a);
        tessellator.vertex(b);
    };
    for (auto const& p : extras) {
        float const x0 = static_cast<float>(p.x - renderOrigin.x);
        float const y0 = static_cast<float>(p.y - renderOrigin.y);
        float const z0 = static_cast<float>(p.z - renderOrigin.z);
        float const x1 = x0 + 1.0f, y1 = y0 + 1.0f, z1 = z0 + 1.0f;
        edge({x0,y0,z0},{x1,y0,z0}); edge({x1,y0,z0},{x1,y1,z0});
        edge({x1,y1,z0},{x0,y1,z0}); edge({x0,y1,z0},{x0,y0,z0});
        edge({x0,y0,z1},{x1,y0,z1}); edge({x1,y0,z1},{x1,y1,z1});
        edge({x1,y1,z1},{x0,y1,z1}); edge({x0,y1,z1},{x0,y0,z1});
        edge({x0,y0,z0},{x0,y0,z1}); edge({x1,y0,z0},{x1,y0,z1});
        edge({x1,y1,z0},{x1,y1,z1}); edge({x0,y1,z0},{x0,y1,z1});
    }
    state.extraBlockMesh = std::make_unique<mce::Mesh>(tessellator.end(
        Tessellator::UploadMode::Buffered,
        "LHoloExtraBlocks",
        Tessellator::SupplementaryFieldAutoGenerationMode::None
    ));
}

} // namespace

void processProjectionOpaqueFrame(
    ProjectionState&                      state,
    Tessellator&                          tessellator,
    BlockSource&                          region,
    Vec3 const&                           cameraPosition,
    LegacyStructureSettings const&        transformSettings,
    ProjectionSectionBuildSettings const& buildSettings,
    int                                   layerDisplayMode,
    int                                   displayLayer,
    int                                   layerAxis
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
        publishErrorProgress(state.progressWrongTypeCount, state.progressWrongStateCount);
    }

    if (tessellator.isTessellating()) tessellator.cancel();

    // Consume completed CPU data only in the opaque render pass.
    uploadCompletedProjectionMeshes(state, tessellator);
    ensureStructureBoundsMesh(state, tessellator, buildSettings.rotationTurns);
    BlockPos const extraRenderOrigin{
        state.anchor.x + buildSettings.offsetX,
        state.anchor.y + buildSettings.offsetY,
        state.anchor.z + buildSettings.offsetZ
    };
    ensureExtraBlockMesh(
        state, tessellator, region, cameraPosition, extraRenderOrigin, buildSettings.rotationTurns
    );
    scheduleProjectionMeshBuild(
        state, tessellator, region, cameraPosition, buildSettings
    );
    buildNextProjectionSectionSynchronously(
        state, tessellator, blockTessellator, region, buildSettings
    );
}

} // namespace lholo::projection::detail
