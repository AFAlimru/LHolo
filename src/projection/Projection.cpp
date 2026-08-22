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
#include "projection/ProjectionCorrections.h"
#include "projection/ProjectionInternalTypes.h"
#include "projection/ProjectionInvalidation.h"
#include "projection/ProjectionMeshScheduler.h"
#include "projection/ProjectionMeshUpload.h"
#include "projection/ProjectionMeshWorker.h"
#include "projection/ProjectionPlacement.h"
#include "projection/ProjectionProgress.h"
#include "projection/ProjectionQueries.h"
#include "projection/ProjectionRenderer.h"
#include "projection/ProjectionRules.h"
#include "projection/ProjectionSectionBuilder.h"
#include "projection/ProjectionState.h"
#include "projection/ProjectionVirtualWorld.h"
#include "projection/ProjectionWorldEvents.h"

#include "overlay/BoundsWireframe.h"
#include "plugin/LHolo.h"
#include "overlay/ImGuiOverlay.h"
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
#include <optional>
#include <memory>
#include <map>
#include <set>
#include <string>
#include <string_view>
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
#include "mc/client/renderer/block/BlockTessellator.h"
#include "mc/client/renderer/block/BlockGraphics.h"
#include "mc/client/renderer/blockactor/BlockActorRenderDispatcher.h"
#include "mc/deps/minecraft_renderer/framebuilder/dragon/RenderMetadata.h"
#include "mc/client/renderer/game/ItemInHandRenderer.h"
#include "mc/client/renderer/game/LevelRenderer.h"
#include "mc/client/renderer/game/LevelRendererPlayer.h"
#include "mc/client/renderer/texture/TextureUVCoordinateSet.h"
#include "mc/client/world/level/biome/biome_color_sampling/TessellationPolicy.h"
#include "mc/client/player/LocalPlayer.h"
#include "mc/deps/core/math/Color.h"
#include "mc/deps/minecraft_renderer/resources/ClientTexture.h"
#include "mc/deps/minecraft_renderer/resources/ServerTexture.h"
#include "mc/deps/minecraft_renderer/renderer/IsMissingTexture.h"
#include "mc/deps/minecraft_renderer/renderer/Mesh.h"
#include "mc/deps/minecraft_renderer/renderer/TexturePtr.h"
#include "mc/network/LoopbackPacketSender.h"
#include "mc/network/MinecraftPacketIds.h"
#include "mc/network/Packet.h"
#include "mc/network/packet/TextPacket.h"
#include "mc/world/actor/Actor.h"
#include "mc/world/Facing.h"
#include "mc/world/level/ILevel.h"
#include "mc/world/level/Level.h"
#include "mc/world/level/BlockSource.h"
#include "mc/world/level/BlockSourceListener.h"
#include "mc/world/level/LevelListener.h"
#include "mc/world/level/block/Block.h"
#include "mc/world/level/block/BlockRenderLayer.h"
#include "mc/world/level/block/actor/BlockActor.h"
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
using detail::updateProjectionCorrections;
using detail::attachProjectionWorldEvents;
using detail::disableMeshWorkerForSession;
using detail::detachProjectionWorldEvents;
using detail::findTessellationBlock;
using detail::findTessellationBlockActor;
using detail::getProjectionMirror;
using detail::getProjectionRotation;
using detail::meshWorkerIsDisabledForSession;
using detail::projectionStatesMatch;
using detail::ProjectionState;
using detail::ProjectionSectionBuildSettings;
using detail::RenderBucket;
using detail::startMeshWorker;
using detail::stopMeshWorker;
using detail::SubChunkKey;
using detail::ScopedTessellationBlocks;

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
    // Finish CPU mesh work before detaching any world-owned objects captured by
    // the task's private ChunkViewSource/BlockSource snapshot.
    stopMeshWorker();
    detachProjectionWorldEvents();
    detail::resetPublishedBuildProgress();
    gState = ProjectionState{};
}

void clearProjectionState() {
    std::lock_guard lock(gStateMutex);
    clearProjectionStateLocked();
}

bool isMenuCommand(std::string_view message) {
    constexpr std::string_view command{"lholo"};
    if (message.size() != command.size()) return false;
    for (std::size_t index = 0; index < command.size(); ++index) {
        auto character = message[index];
        if (character >= 'A' && character <= 'Z') character += 'a' - 'A';
        if (character != command[index]) return false;
    }
    return true;
}

bool filterProjectionPacket(Packet& packet) {
    if (packet.getId() != MinecraftPacketIds::Text) return false;
    auto& textPacket = static_cast<TextPacket&>(packet);
    if (textPacket.getType() != TextPacketType::Chat || !isMenuCommand(textPacket.getMessage())) return false;

    if (overlay::ensureInstalled()) {
        structure::requestOpenGui();
    } else {
        logger().error("Could not initialize the injected ImGui overlay");
    }
    return true;
}

bool resolveTerrainTexture(IClientInstance& client, ProjectionState& state) {
    auto* levelRenderer = client.getLevelRenderer();
    if (!levelRenderer) return false;
    auto const& atlasTexture = levelRenderer->mAtlasTexture.get();
    if (!atlasTexture || atlasTexture.isMissingTexture() == IsMissingTexture::Yes) return false;
    state.terrainTexture.emplace(atlasTexture);
    state.terrainTextureVariant.emplace(*state.terrainTexture);
    return true;
}

bool enableStructureProjection(
    BaseActorRenderContext& renderContext,
    std::shared_ptr<structure::LoadedStructure const> loaded
) {
    auto& client = renderContext.getClient();
    auto* player = client.getLocalPlayer();
    if (!player || !loaded || loaded->renderBlocks.empty()) return false;

    ProjectionState next;
    next.client = &client;
    next.level = &player->getLevel();
    next.dimension = &player->getDimension();
    next.structure = std::move(loaded);
    next.structureGeneration = next.structure->generation;
    next.blockTessellator = std::make_unique<BlockTessellator>(&player->getDimensionBlockSource());
    next.correctionStates.resize(
        next.structure->renderBlocks.size(), CorrectionState::Unknown
    );
    next.progressCorrect.resize(next.structure->renderBlocks.size(), 0);
    next.progressErrorKind.resize(next.structure->renderBlocks.size(), 0);
    next.blockActorRendererAvailable.resize(next.structure->renderBlocks.size(), 0);
    // Force the first render pass to build the transformed virtual-world lookup.
    next.cachedRotation = -1;
    next.cachedMirror = -1;
    std::map<std::tuple<int, int, int>, std::size_t> sectionLookup;
    next.blockToSection.resize(next.structure->renderBlocks.size());
    for (std::size_t index = 0; index < next.structure->renderBlocks.size(); ++index) {
        auto const& entry = next.structure->renderBlocks[index];
        auto const key = std::tuple{entry.x / 16, entry.y / 16, entry.z / 16};
        auto [found, inserted] = sectionLookup.try_emplace(key, next.sectionBlockIndices.size());
        if (inserted) {
            next.sectionBlockIndices.emplace_back();
            auto const [sx, sy, sz] = key;
            next.sectionCenters.emplace_back(
                static_cast<float>(sx * 16 + 8),
                static_cast<float>(sy * 16 + 8),
                static_cast<float>(sz * 16 + 8)
            );
        }
        next.blockToSection[index] = found->second;
        next.sectionBlockIndices[found->second].push_back(index);
    }
    for (auto& meshes : next.sectionMeshes) meshes.resize(next.sectionBlockIndices.size());
    next.warningFillSectionMeshes.resize(next.sectionBlockIndices.size());
    next.correctionOutlineSectionMeshes.resize(next.sectionBlockIndices.size());
    next.liquidProxySectionMeshes.resize(next.sectionBlockIndices.size());
    next.blockEntityPlaceholderSectionMeshes.resize(next.sectionBlockIndices.size());
    next.sectionDirty.assign(next.sectionBlockIndices.size(), true);
    next.sectionIncrementalDirty.assign(next.sectionBlockIndices.size(), false);
    next.sectionBuildInFlight.assign(next.sectionBlockIndices.size(), false);
    next.sectionRequestedRevision.assign(next.sectionBlockIndices.size(), 1);
    next.sectionUploadedRevision.assign(next.sectionBlockIndices.size(), 0);
    if (!resolveTerrainTexture(client, next)) return false;
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
    if (!player) return false;
    return gState.client == &client && gState.level == &player->getLevel()
        && gState.dimension == &player->getDimension();
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
        auto& blockTessellator = *gState.blockTessellator;
        blockTessellator.setRegion(player->getDimensionBlockSource());

        auto& region = player->getDimensionBlockSource();
        auto const correctionChanges = updateProjectionCorrections(
            gState,
            region,
            transformSettings,
            mirrorMode,
            rotationTurns,
            offsetX,
            offsetY,
            offsetZ,
            layerDisplayMode,
            displayLayer,
            layerAxis
        );
        if (correctionChanges.overall) {
            detail::publishPlacedProgress(gState.progressCorrectCount);
        }
        if (correctionChanges.visible) {
            detail::publishVisiblePlacedProgress(gState.progressVisibleCorrectCount);
        }
        if (correctionChanges.errors) {
            detail::publishErrorProgress(
                gState.progressWrongTypeCount, gState.progressWrongStateCount
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

        if (tessellator.isTessellating()) tessellator.cancel();

        // Consume completed CPU data only in the opaque render pass.
        detail::uploadCompletedProjectionMeshes(gState, tessellator);
        // The bounds are only 24 line vertices and are intentionally generated
        // once on the render thread; section BlockTessellator work stays async.
        if (gState.asyncMeshBuildingEnabled && !gState.structureBoundsMesh) {
            auto const rotated = rotationTurns == 1 || rotationTurns == 3;
            auto const width = static_cast<float>(rotated ? gState.structure->sizeZ : gState.structure->sizeX);
            auto const height = static_cast<float>(gState.structure->sizeY);
            auto const depth = static_cast<float>(rotated ? gState.structure->sizeX : gState.structure->sizeZ);
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
            gState.structureBoundsMesh = std::make_unique<mce::Mesh>(tessellator.end(
                Tessellator::UploadMode::Buffered,
                "LHoloStructureBounds",
                Tessellator::SupplementaryFieldAutoGenerationMode::None
            ));
        }

        detail::scheduleProjectionMeshBuild(
            gState,
            tessellator,
            region,
            renderContext.getCameraPosition(),
            sectionBuildSettings
        );
        detail::buildNextProjectionSectionSynchronously(
            gState,
            tessellator,
            blockTessellator,
            region,
            sectionBuildSettings
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

    if (!gState.projectedBlockActors.empty()) {
        alignas(mce::MaterialPtr) static const std::byte sNoForcedMaterialStorage[sizeof(mce::MaterialPtr)]{};
        auto const& noForcedMaterial = *reinterpret_cast<mce::MaterialPtr const*>(sNoForcedMaterialStorage);
        auto& dispatcher = renderContext.mBlockEntityRenderDispatcher;
        auto& region = player->getDimensionBlockSource();
        ScopedTessellationBlocks blockActorWorldScope(
            *gState.expectedWorldBlocks,
            *gState.expectedWorldBlockActors
        );
        for (auto const& projected : gState.projectedBlockActors) {
            auto const state = gState.correctionStates[projected.structureIndex];
            if (state == CorrectionState::Correct
                || state == CorrectionState::WrongType
                || state == CorrectionState::WrongState
                || !projected.actor->isWithinRenderDistance(camera)) {
                continue;
            }
            dispatcher.render(
                renderContext,
                region,
                *projected.actor,
                *projected.block,
                renderAlphaLayer,
                noForcedMaterial,
                nullptr,
                -1,
                std::nullopt
            );
        }
    }

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
    BlockSourceGetBlockHook,
    ll::memory::HookPriority::Normal,
    BlockSource,
    static_cast<Block const& (BlockSource::*)(BlockPos const&) const>(&BlockSource::$getBlock),
    Block const&,
    BlockPos const& position
) {
    if (auto const* block = findTessellationBlock(position)) return *block;
    return origin(position);
}

LL_TYPE_INSTANCE_HOOK(
    BlockSourceGetBlockLayerHook,
    ll::memory::HookPriority::Normal,
    BlockSource,
    static_cast<Block const& (BlockSource::*)(BlockPos const&, uint) const>(&BlockSource::$getBlock),
    Block const&,
    BlockPos const& position,
    uint layer
) {
    if (layer == 0) {
        if (auto const* block = findTessellationBlock(position)) return *block;
    }
    return origin(position, layer);
}

LL_TYPE_INSTANCE_HOOK(
    BlockSourceGetBlockEntityHook,
    ll::memory::HookPriority::Normal,
    BlockSource,
    static_cast<BlockActor const* (BlockSource::*)(BlockPos const&) const>(&BlockSource::$getBlockEntity),
    BlockActor const*,
    BlockPos const& position
) {
    if (auto const* actor = findTessellationBlockActor(position)) return actor;
    return origin(position);
}

LL_TYPE_INSTANCE_HOOK(
    LoopbackPacketSenderSendToServerHook,
    ll::memory::HookPriority::Normal,
    LoopbackPacketSender,
    &LoopbackPacketSender::$sendToServer,
    void,
    Packet& packet
) {
    if (filterProjectionPacket(packet)) return;
    origin(packet);
}

LL_TYPE_INSTANCE_HOOK(
    LoopbackPacketSenderSendHook,
    ll::memory::HookPriority::Normal,
    LoopbackPacketSender,
    &LoopbackPacketSender::$send,
    void,
    Packet& packet
) {
    if (filterProjectionPacket(packet)) return;
    origin(packet);
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
    if (BlockSourceGetBlockHook::hook() < 0) return false;
    if (BlockSourceGetBlockLayerHook::hook() < 0) {
        BlockSourceGetBlockHook::unhook();
        return false;
    }
    if (BlockSourceGetBlockEntityHook::hook() < 0) {
        BlockSourceGetBlockLayerHook::unhook();
        BlockSourceGetBlockHook::unhook();
        return false;
    }
    if (LoopbackPacketSenderSendToServerHook::hook() < 0) {
        BlockSourceGetBlockEntityHook::unhook();
        BlockSourceGetBlockLayerHook::unhook();
        BlockSourceGetBlockHook::unhook();
        return false;
    }
    if (LoopbackPacketSenderSendHook::hook() < 0) {
        LoopbackPacketSenderSendToServerHook::unhook();
        BlockSourceGetBlockEntityHook::unhook();
        BlockSourceGetBlockLayerHook::unhook();
        BlockSourceGetBlockHook::unhook();
        return false;
    }
    if (LevelRendererPlayerRenderHitSelectHook::hook() < 0) {
        LoopbackPacketSenderSendHook::unhook();
        LoopbackPacketSenderSendToServerHook::unhook();
        BlockSourceGetBlockEntityHook::unhook();
        BlockSourceGetBlockLayerHook::unhook();
        BlockSourceGetBlockHook::unhook();
        return false;
    }
    if (LevelRendererPlayerRenderBlockEntitiesHook::hook() < 0) {
        LevelRendererPlayerRenderHitSelectHook::unhook();
        LoopbackPacketSenderSendHook::unhook();
        LoopbackPacketSenderSendToServerHook::unhook();
        BlockSourceGetBlockEntityHook::unhook();
        BlockSourceGetBlockLayerHook::unhook();
        BlockSourceGetBlockHook::unhook();
        return false;
    }
    return true;
}

void uninstallHook() {
    LevelRendererPlayerRenderBlockEntitiesHook::unhook();
    LevelRendererPlayerRenderHitSelectHook::unhook();
    LoopbackPacketSenderSendHook::unhook();
    LoopbackPacketSenderSendToServerHook::unhook();
    BlockSourceGetBlockEntityHook::unhook();
    BlockSourceGetBlockLayerHook::unhook();
    BlockSourceGetBlockHook::unhook();
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
