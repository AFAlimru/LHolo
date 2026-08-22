// LHolo - Client-side projection renderer for Minecraft Bedrock Windows
// Copyright (C) 2026  MarmieQi
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#include "projection/Projection.h"
#include "projection/ProjectionFramePipeline.h"
#include "projection/ProjectionGameHooks.h"
#include "projection/ProjectionInternalTypes.h"
#include "projection/ProjectionInvalidation.h"
#include "projection/ProjectionLifecycle.h"
#include "projection/mesh/ProjectionMeshWorker.h"
#include "projection/ProjectionPlacement.h"
#include "projection/ProjectionProgress.h"
#include "projection/ProjectionQueries.h"
#include "projection/mesh/ProjectionRenderer.h"
#include "projection/ProjectionRules.h"
#include "projection/mesh/ProjectionSectionBuilder.h"
#include "projection/ProjectionState.h"
#include "projection/ProjectionWorldEvents.h"

#include "overlay/BoundsWireframe.h"
#include "plugin/LHolo.h"
#include "structure/capture/StructureCapture.h"
#include "structure/StructureLoader.h"

#include <atomic>
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <deque>
#include <exception>
#include <functional>
#include <limits>
#include <mutex>
#include <memory>
#include <set>
#include <string>
#include <thread>
#include <tuple>
#include <utility>
#include <variant>
#include <vector>

#include "mc/client/game/IClientInstance.h"
#include "mc/client/gui/screens/ScreenContext.h"
#include "mc/client/renderer/BaseActorRenderContext.h"
#include "mc/client/renderer/Tessellator.h"
#include "mc/client/renderer/TextureGroup.h"
#include "mc/client/renderer/block/BlockGraphics.h"
#include "mc/client/renderer/game/ItemInHandRenderer.h"
#include "mc/client/renderer/game/LevelRenderer.h"
#include "mc/client/renderer/game/LevelRendererPlayer.h"
#include "mc/client/renderer/texture/TextureUVCoordinateSet.h"
#include "mc/client/world/level/biome/biome_color_sampling/TessellationPolicy.h"
#include "mc/client/player/LocalPlayer.h"
#include "mc/deps/core/math/Color.h"
#include "mc/deps/minecraft_renderer/resources/ClientTexture.h"
#include "mc/deps/minecraft_renderer/resources/ServerTexture.h"
#include "mc/deps/minecraft_renderer/renderer/Mesh.h"
#include "mc/deps/minecraft_renderer/renderer/TexturePtr.h"
#include "mc/world/actor/Actor.h"
#include "mc/world/Facing.h"
#include "mc/world/level/ILevel.h"
#include "mc/world/level/Level.h"
#include "mc/world/level/BlockSource.h"
#include "mc/world/level/BlockSourceListener.h"
#include "mc/world/level/LevelListener.h"
#include "mc/world/level/block/Block.h"
#include "mc/world/level/block/BlockRenderLayer.h"
#include "mc/world/level/biome/biome_color_sampling/BiomeColorSampling.h"
#include "mc/world/level/levelgen/structure/LegacyStructureSettings.h"
#include "mc/world/level/levelgen/structure/LegacyStructureTemplate.h"
#include "mc/util/Mirror.h"
#include "mc/util/Rotation.h"

#include "ll/api/memory/Hook.h"
#include "ll/api/mod/NativeMod.h"
#include "ll/api/thread/ThreadPoolExecutor.h"

namespace lholo::projection {
namespace {

using detail::CorrectionState;
using detail::attachProjectionWorldEvents;
using detail::disableMeshWorkerForSession;
using detail::getProjectionMirror;
using detail::getProjectionRotation;
using detail::meshWorkerIsDisabledForSession;
using detail::projectionStatesMatch;
using detail::ProjectionState;
using detail::ProjectionSectionBuildSettings;
using detail::RenderBucket;
using detail::startMeshWorker;
using detail::SubChunkKey;

auto& logger() {
    return LHolo::getInstance().getSelf().getLogger();
}

std::atomic<float> gOpacity{1.0f};
std::atomic<float> gCorrectionFillOpacity{0.15f};
std::atomic<float> gCorrectionOutlineOpacity{1.0f};
std::atomic_bool   gStructureBoundsEnabled{true};
std::atomic_bool   gPendingStructureAnchor{false};
std::atomic_int    gPendingStructureAnchorX{0};
std::atomic_int    gPendingStructureAnchorY{0};
std::atomic_int    gPendingStructureAnchorZ{0};
std::mutex       gStateMutex;
ProjectionState  gState;
overlay::BoundsWireframe gCaptureBounds;

void clearProjectionStateLocked() {
    detail::resetProjectionState(gState);
}

void clearProjectionState() {
    std::lock_guard lock(gStateMutex);
    clearProjectionStateLocked();
}

bool enableStructureProjection(
    BaseActorRenderContext& renderContext,
    std::shared_ptr<structure::LoadedStructure const> loaded
) {
    ProjectionState next;
    if (!detail::prepareProjectionState(next, renderContext, std::move(loaded))) return false;
    auto& client = renderContext.getClient();
    auto* player = client.getLocalPlayer();
    if (gPendingStructureAnchor.exchange(false, std::memory_order_acq_rel)) {
        next.anchor = BlockPos{
            gPendingStructureAnchorX.load(std::memory_order_relaxed),
            gPendingStructureAnchorY.load(std::memory_order_relaxed),
            gPendingStructureAnchorZ.load(std::memory_order_relaxed)
        };
    } else {
        auto const& position = player->getPosition();
        // Player position is the feet/air cell. Default a newly loaded
        // structure to the supporting ground cell directly below it.
        next.anchor = BlockPos(
            std::floor(position.x),
            std::floor(position.y) - 1,
            std::floor(position.z)
        );
    }
    gState = std::move(next);
    gState.enabled = true;
    try {
        if (meshWorkerIsDisabledForSession()) {
            gState.asyncMeshBuildingEnabled = false;
        } else {
            gState.meshWorkerGeneration = startMeshWorker();
        }
    } catch (std::exception const& exception) {
        gState.asyncMeshBuildingEnabled = false;
        disableMeshWorkerForSession();
        logger().warn("Projection mesh worker initialization failed; using synchronous fallback: {}", exception.what());
    } catch (...) {
        gState.asyncMeshBuildingEnabled = false;
        disableMeshWorkerForSession();
        logger().warn("Projection mesh worker initialization failed; using synchronous fallback");
    }
    attachProjectionWorldEvents(player->getLevel(), player->getDimensionBlockSource());
    detail::initializePublishedBuildProgress(gState.structure->renderBlocks.size());
    structure::recordProjectionAnchor(gState.anchor.x, gState.anchor.y, gState.anchor.z);
    logger().info(
        "Structure projection enabled: {} renderable blocks at ({}, {}, {})",
        gState.structure->renderBlocks.size(), gState.anchor.x, gState.anchor.y, gState.anchor.z
    );
    return true;
}

bool contextIsValid(IClientInstance& client, Actor* player) {
    return detail::projectionContextMatches(gState, client, player);
}

void renderProjection(BaseActorRenderContext& renderContext, bool renderAlphaLayer) {
    auto& client = renderContext.getClient();
    auto* player  = client.getLocalPlayer();

    auto& tessellator = renderContext.getTessellator();
    tessellator.begin(Tessellator::DebugContextCallback{}, 128, false);

    if (!gState.blockTessellator) {
        tessellator.cancel();
        return;
    }
    if (!renderAlphaLayer) {
        auto const mirrorMode = structure::getMirrorMode();
        auto const rotationTurns = structure::getRotationQuarterTurns();
        auto const mirror = getProjectionMirror(mirrorMode);
        auto const rotation = getProjectionRotation(rotationTurns);
        LegacyStructureSettings transformSettings;
        transformSettings.setMirror(mirror);
        transformSettings.setRotation(rotation);
        bool const identityTransform = mirrorMode == 0 && rotationTurns == 0;
        auto const offsetX = structure::getOffsetX();
        auto const offsetY = structure::getOffsetY();
        auto const offsetZ = structure::getOffsetZ();
        auto const layerDisplayMode = structure::getLayerDisplayMode();
        auto const layerAxis = structure::getLayerAxis();
        auto const maxLayer = std::max(
            0,
            (layerAxis == 1 ? gState.structure->sizeX : gState.structure->sizeY) - 1
        );
        auto const displayLayer = std::clamp(
            structure::getDisplayLayer(), 0, maxLayer
        );
        auto const structureOpacity = gOpacity.load(std::memory_order_relaxed);
        auto const correctionFillOpacity = gCorrectionFillOpacity.load(std::memory_order_relaxed);
        auto const correctionOutlineOpacity = gCorrectionOutlineOpacity.load(std::memory_order_relaxed);
        auto const invalidation = detail::reconcileProjectionInvalidation(
            gState,
            detail::ProjectionInvalidationSettings{
                .mirrorMode               = mirrorMode,
                .rotationTurns            = rotationTurns,
                .offsetX                  = offsetX,
                .offsetY                  = offsetY,
                .offsetZ                  = offsetZ,
                .layerDisplayMode         = layerDisplayMode,
                .displayLayer             = displayLayer,
                .layerAxis                = layerAxis,
                .structureOpacity         = structureOpacity,
                .correctionFillOpacity    = correctionFillOpacity,
                .correctionOutlineOpacity = correctionOutlineOpacity
            }
        );
        if (invalidation.placementViewChanged()) {
            detail::rebuildProjectionPlacement(
                gState,
                player->getDimensionBlockSource(),
                renderContext.mBlockEntityRenderDispatcher,
                transformSettings,
                detail::ProjectionPlacementSettings{
                    .mirrorMode        = mirrorMode,
                    .rotationTurns     = rotationTurns,
                    .offsetX           = offsetX,
                    .offsetY           = offsetY,
                    .offsetZ           = offsetZ,
                    .layerDisplayMode  = layerDisplayMode,
                    .displayLayer      = displayLayer,
                    .layerAxis         = layerAxis,
                    .identityTransform = identityTransform
                }
            );
        }
        ProjectionSectionBuildSettings const sectionBuildSettings{
            .mirror                   = mirror,
            .rotation                 = rotation,
            .mirrorMode               = mirrorMode,
            .rotationTurns            = rotationTurns,
            .offsetX                  = offsetX,
            .offsetY                  = offsetY,
            .offsetZ                  = offsetZ,
            .structureOpacity         = structureOpacity,
            .correctionFillOpacity    = correctionFillOpacity,
            .correctionOutlineOpacity = correctionOutlineOpacity,
            .identityTransform        = identityTransform
        };
        detail::processProjectionOpaqueFrame(
            gState,
            tessellator,
            player->getDimensionBlockSource(),
            renderContext.getCameraPosition(),
            transformSettings,
            sectionBuildSettings,
            layerDisplayMode,
            displayLayer,
            layerAxis
        );
    }

    // Keep vanilla world queries at their real BlockPos, but do not upload large
    // absolute coordinates to the GPU. Render vertices relative to the projection
    // origin, matching the strategy used by chunk meshes.
    BlockPos const renderOrigin{
        gState.anchor.x + structure::getOffsetX(),
        gState.anchor.y + structure::getOffsetY(),
        gState.anchor.z + structure::getOffsetZ()
    };
    auto const structureOpacity = gOpacity.load(std::memory_order_relaxed);
    auto const& camera = renderContext.getCameraPosition();
    if (renderAlphaLayer) {
        // The transparent pass only submits meshes built during the preceding
        // opaque pass. Do not leave the shared immediate tessellator active.
        tessellator.cancel();
    }

    detail::submitProjectedBlockActorPass(
        gState,
        renderContext,
        player->getDimensionBlockSource(),
        camera,
        renderAlphaLayer
    );

    auto matrix = renderContext.getWorldMatrix().push(false);
    matrix->translate(
        static_cast<float>(renderOrigin.x) - camera.x,
        static_cast<float>(renderOrigin.y) - camera.y,
        static_cast<float>(renderOrigin.z) - camera.z
    );

    auto& itemRenderer = renderContext.getItemInHandRenderer();
    auto const& blendMaterial = itemRenderer.mMatBlendBlock.get();
    if (!blendMaterial) {
        tessellator.cancel();
        return;
    }

    if (!gState.terrainTextureVariant) {
        tessellator.cancel();
        logger().error("Projection terrain texture is not available");
        return;
    }

    if (!gState.meshPreflightDone) {
        auto const countValid = [](auto const& meshes) {
            return std::count_if(meshes.begin(), meshes.end(), [](auto const& mesh) {
                return mesh && mesh->isValid();
            });
        };
        std::size_t normalMeshes{};
        for (auto const& meshes : gState.sectionMeshes) {
            normalMeshes += countValid(meshes);
        }
        auto const warningMeshes = countValid(gState.warningFillSectionMeshes);
        auto const outlineMeshes = countValid(gState.correctionOutlineSectionMeshes);
        auto const liquidMeshes = countValid(gState.liquidProxySectionMeshes);
        auto const placeholderMeshes = countValid(gState.blockEntityPlaceholderSectionMeshes);
        if (normalMeshes + warningMeshes + outlineMeshes + liquidMeshes + placeholderMeshes != 0) {
            gState.meshPreflightDone = true;
        }
    }

    try {
        detail::submitProjectionMeshPass(
            gState,
            renderContext,
            client,
            renderOrigin,
            camera,
            structureOpacity,
            renderAlphaLayer,
            gStructureBoundsEnabled.load(std::memory_order_relaxed)
        );
    } catch (std::exception const& exception) {
        logger().error("Projection immediate mesh submission failed: {}", exception.what());
        tessellator.cancel();
        clearProjectionStateLocked();
        return;
    } catch (...) {
        logger().error("Projection immediate mesh submission failed with an unknown exception");
        tessellator.cancel();
        clearProjectionStateLocked();
        return;
    }

}

LL_TYPE_INSTANCE_HOOK(
    LevelRendererPlayerRenderHitSelectHook,
    ll::memory::HookPriority::Normal,
    LevelRendererPlayer,
    &LevelRendererPlayer::renderHitSelect,
    void,
    BaseActorRenderContext& renderContext,
    BlockSource&            region,
    BlockPos const&         pos,
    bool                    fancyGraphics
) {
    {
        std::lock_guard lock(gStateMutex);
        if (gState.enabled && gState.structure) {
            auto const found = gState.expectedWorldBlockIndices->find(
                std::tuple{pos.x, pos.y, pos.z}
            );
            if (found != gState.expectedWorldBlockIndices->end()) {
                auto const state = gState.correctionStates[found->second];
                if (state == CorrectionState::WrongType
                    || state == CorrectionState::WrongState) {
                    // LHolo already renders a complete red/yellow hull and
                    // outline for this cell. Vanilla's coincident hit-select
                    // overlay adds a second surface only while the crosshair
                    // targets it, producing the observed flicker.
                    return;
                }
            }
        }
    }
    origin(renderContext, region, pos, fancyGraphics);
}

LL_TYPE_INSTANCE_HOOK(
    LevelRendererPlayerRenderBlockEntitiesHook,
    ll::memory::HookPriority::Normal,
    LevelRendererPlayer,
    &LevelRendererPlayer::$renderBlockEntities,
    void,
    BaseActorRenderContext& renderContext,
    bool                      renderAlphaLayer
) {
    origin(renderContext, renderAlphaLayer);

    std::unique_lock lock(gStateMutex);

    if (auto const bounds = structure::capture::getBounds()) {
        gCaptureBounds.setBounds(
            BlockPos{bounds->min.x, bounds->min.y, bounds->min.z},
            BlockPos{bounds->max.x, bounds->max.y, bounds->max.z},
            0xFF0000FF
        );
    } else {
        gCaptureBounds.clear();
    }
    gCaptureBounds.render(renderContext, renderAlphaLayer);

    if (auto loaded = structure::getLoaded(); loaded && loaded->generation != gState.structureGeneration) {
        clearProjectionStateLocked();
        if (!enableStructureProjection(renderContext, std::move(loaded))) {
            clearProjectionStateLocked();
            logger().error("Could not enable loaded structure projection");
        }
    }

    if (!gState.enabled) return;
    auto& client = renderContext.getClient();
    if (!contextIsValid(client, client.getLocalPlayer())) {
        clearProjectionStateLocked();
        lock.unlock();
        structure::clear();
        return;
    }
    renderProjection(renderContext, renderAlphaLayer);
}

} // namespace

bool installHook() {
    if (!detail::installProjectionGameHooks()) return false;
    if (LevelRendererPlayerRenderHitSelectHook::hook() < 0) {
        detail::uninstallProjectionGameHooks();
        return false;
    }
    if (LevelRendererPlayerRenderBlockEntitiesHook::hook() < 0) {
        LevelRendererPlayerRenderHitSelectHook::unhook();
        detail::uninstallProjectionGameHooks();
        return false;
    }
    return true;
}

void uninstallHook() {
    LevelRendererPlayerRenderBlockEntitiesHook::unhook();
    LevelRendererPlayerRenderHitSelectHook::unhook();
    detail::uninstallProjectionGameHooks();
    std::lock_guard lock(gStateMutex);
    gCaptureBounds.clear();
}

void disable() {
    clearProjectionState();
}

float getOpacity() {
    return gOpacity.load(std::memory_order_relaxed);
}

void setOpacity(float opacity) {
    gOpacity.store(std::clamp(opacity, 0.0f, 1.0f), std::memory_order_relaxed);
}

float getCorrectionFillOpacity() {
    return gCorrectionFillOpacity.load(std::memory_order_relaxed);
}

void setCorrectionFillOpacity(float opacity) {
    gCorrectionFillOpacity.store(std::clamp(opacity, 0.0f, 1.0f), std::memory_order_relaxed);
}

float getCorrectionOutlineOpacity() {
    return gCorrectionOutlineOpacity.load(std::memory_order_relaxed);
}

void setCorrectionOutlineOpacity(float opacity) {
    gCorrectionOutlineOpacity.store(std::clamp(opacity, 0.0f, 1.0f), std::memory_order_relaxed);
}

bool getStructureBoundsEnabled() {
    return gStructureBoundsEnabled.load(std::memory_order_relaxed);
}

void setStructureBoundsEnabled(bool enabled) {
    gStructureBoundsEnabled.store(enabled, std::memory_order_relaxed);
}

void requestNextStructureAnchor(int x, int y, int z) {
    gPendingStructureAnchorX.store(x, std::memory_order_relaxed);
    gPendingStructureAnchorY.store(y, std::memory_order_relaxed);
    gPendingStructureAnchorZ.store(z, std::memory_order_relaxed);
    gPendingStructureAnchor.store(true, std::memory_order_release);
}

BuildProgress getBuildProgress() {
    return detail::getPublishedBuildProgress();
}

ProjectionQuery queryProjection(LocalPlayer& player, BlockPos const& worldPos) {
    std::unique_lock lock(gStateMutex);
    if (!gState.enabled || !gState.structure) return {nullptr, false};
    if (gState.level != &player.getLevel() || gState.dimension != &player.getDimension()) {
        clearProjectionStateLocked();
        lock.unlock();
        structure::clear();
        return {nullptr, false};
    }
    return detail::queryProjectionCell(gState, worldPos);
}

std::vector<RangeCandidate> queryMissingCellsInRange(LocalPlayer& player, Vec3 const& center, float radius) {
    std::unique_lock lock(gStateMutex);
    if (!gState.enabled || !gState.structure) return {};
    if (gState.level != &player.getLevel() || gState.dimension != &player.getDimension()) {
        clearProjectionStateLocked();
        lock.unlock();
        structure::clear();
        return {};
    }
    return detail::queryMissingProjectionCells(gState, center, radius);
}

} // namespace lholo::projection
