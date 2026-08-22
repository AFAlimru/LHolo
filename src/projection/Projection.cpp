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
#include "projection/ProjectionMeshWorker.h"
#include "projection/ProjectionPlacement.h"
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
#include "mc/deps/minecraft_renderer/renderer/RenderMaterial.h"
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
#include "mc/world/level/chunk/LevelChunk.h"
#include "mc/world/level/chunk/ChunkViewSource.h"
#include "mc/world/level/chunk/ChunkSourceViewGenerateMode.h"
#include "mc/world/level/levelgen/structure/LegacyStructureSettings.h"
#include "mc/world/level/levelgen/structure/LegacyStructureTemplate.h"
#include "mc/world/level/material/Material.h"
#include "mc/util/Mirror.h"
#include "mc/util/Rotation.h"

#include "ll/api/memory/Hook.h"
#include "ll/api/mod/NativeMod.h"
#include "ll/api/thread/ThreadPoolExecutor.h"

namespace lholo::projection {
namespace {

using detail::CorrectionState;
using detail::updateProjectionCorrections;
using detail::AsyncSectionBuildResult;
using detail::attachProjectionWorldEvents;
using detail::disableMeshWorkerForSession;
using detail::detachProjectionWorldEvents;
using detail::findTessellationBlock;
using detail::findTessellationBlockActor;
using detail::getProjectionMirror;
using detail::getProjectionRotation;
using detail::isLayerVisible;
using detail::meshWorkerIsBusy;
using detail::meshWorkerIsDisabledForSession;
using detail::projectionStatesMatch;
using detail::ProjectionState;
using detail::ProjectionSectionBuildSettings;
using detail::RenderBucket;
using detail::startMeshWorker;
using detail::stopMeshWorker;
using detail::SubChunkKey;
using detail::ScopedTessellationBlocks;
using detail::submitMeshWorkerTask;
using detail::takeCompletedSectionBuilds;
using detail::transformStructurePosition;

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
std::atomic_uint64_t gBuildProgressPlaced{0};
std::atomic_uint64_t gBuildProgressTotal{0};
std::atomic_uint64_t gBuildProgressVisiblePlaced{0};
std::atomic_uint64_t gBuildProgressVisibleTotal{0};
std::atomic_uint64_t gBuildProgressWrongType{0};
std::atomic_uint64_t gBuildProgressWrongState{0};
std::mutex       gStateMutex;
ProjectionState  gState;
overlay::BoundsWireframe gCaptureBounds;

void markSectionDirty(ProjectionState& state, std::size_t section, bool incremental) {
    if (section >= state.sectionDirty.size()) return;
    state.sectionDirty[section] = true;
    state.sectionIncrementalDirty[section]
        = state.sectionIncrementalDirty[section] || incremental;
    ++state.sectionRequestedRevision[section];
}

void markAllSectionsDirty(ProjectionState& state, bool incremental) {
    for (std::size_t section = 0; section < state.sectionDirty.size(); ++section) {
        markSectionDirty(state, section, incremental);
    }
}

void clearProjectionStateLocked() {
    // Finish CPU mesh work before detaching any world-owned objects captured by
    // the task's private ChunkViewSource/BlockSource snapshot.
    stopMeshWorker();
    detachProjectionWorldEvents();
    gBuildProgressPlaced.store(0, std::memory_order_relaxed);
    gBuildProgressVisiblePlaced.store(0, std::memory_order_relaxed);
    gBuildProgressVisibleTotal.store(0, std::memory_order_relaxed);
    gBuildProgressWrongType.store(0, std::memory_order_relaxed);
    gBuildProgressWrongState.store(0, std::memory_order_relaxed);
    gBuildProgressTotal.store(0, std::memory_order_release);
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
    gBuildProgressPlaced.store(0, std::memory_order_relaxed);
    gBuildProgressVisiblePlaced.store(0, std::memory_order_relaxed);
    gBuildProgressVisibleTotal.store(
        gState.structure->renderBlocks.size(), std::memory_order_relaxed
    );
    gBuildProgressWrongType.store(0, std::memory_order_relaxed);
    gBuildProgressWrongState.store(0, std::memory_order_relaxed);
    gBuildProgressTotal.store(gState.structure->renderBlocks.size(), std::memory_order_release);
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
        auto const layerIsVisible = [&](int layer) {
            return isLayerVisible(layer, layerDisplayMode, displayLayer);
        };
        auto const structureOpacity = gOpacity.load(std::memory_order_relaxed);
        auto const correctionFillOpacity = gCorrectionFillOpacity.load(std::memory_order_relaxed);
        auto const correctionOutlineOpacity = gCorrectionOutlineOpacity.load(std::memory_order_relaxed);
        bool const geometryTransformChanged = gState.cachedRotation != rotationTurns
            || gState.cachedMirror != mirrorMode;
        bool const placementMoved = gState.cachedOffsetX != offsetX
            || gState.cachedOffsetY != offsetY || gState.cachedOffsetZ != offsetZ;
        bool const layerChanged = gState.cachedLayerDisplayMode != layerDisplayMode
            || gState.cachedDisplayLayer != displayLayer || gState.cachedLayerAxis != layerAxis;
        bool const opacityChanged = std::abs(gState.cachedOpacity - structureOpacity) > 0.0001f;
        bool const correctionStyleChanged
            = std::abs(gState.cachedCorrectionFillOpacity - correctionFillOpacity) > 0.0001f
            || std::abs(gState.cachedCorrectionOutlineOpacity - correctionOutlineOpacity) > 0.0001f;
        if (geometryTransformChanged || placementMoved || layerChanged || opacityChanged || correctionStyleChanged) {
            if (geometryTransformChanged || layerChanged || opacityChanged || correctionStyleChanged) {
                gState.meshPreflightDone = false;
            }
            if (geometryTransformChanged || opacityChanged || correctionStyleChanged) {
                markAllSectionsDirty(gState, false);
            }
            if (placementMoved && !geometryTransformChanged) {
                // Local geometry survives an XYZ move, but a task already
                // sampling the previous world position must not be accepted.
                for (std::size_t section = 0; section < gState.sectionDirty.size(); ++section) {
                    ++gState.sectionRequestedRevision[section];
                    if (gState.sectionBuildInFlight[section]) gState.sectionDirty[section] = true;
                }
            }
            // LayerRange changes only invalidate sections containing blocks
            // whose visibility crossed the old/new boundary. This mirrors
            // Litematica's section/range intersection instead of rebuilding
            // the whole structure for a one-layer step.
            if (layerChanged && !geometryTransformChanged) {
                auto const oldLayerVisible = [&](structure::LoadedStructure::RenderBlock const& entry) {
                    if (gState.cachedLayerDisplayMode < 0 || gState.cachedLayerAxis < 0) return false;
                    auto const layer = gState.cachedLayerAxis == 1 ? entry.x : entry.y;
                    return isLayerVisible(
                        layer, gState.cachedLayerDisplayMode, gState.cachedDisplayLayer
                    );
                };
                for (std::size_t index = 0; index < gState.structure->renderBlocks.size(); ++index) {
                    auto const& entry = gState.structure->renderBlocks[index];
                    auto const visible = layerIsVisible(layerAxis == 1 ? entry.x : entry.y);
                    if (oldLayerVisible(entry) == visible) continue;
                    markSectionDirty(gState, gState.blockToSection[index], false);
                    gState.correctionStates[index] = visible
                        ? CorrectionState::Unknown
                        : CorrectionState::Correct;
                }
            }

            gState.cachedRotation = rotationTurns;
            gState.cachedMirror = mirrorMode;
            gState.cachedOffsetX = offsetX;
            gState.cachedOffsetY = offsetY;
            gState.cachedOffsetZ = offsetZ;
            gState.cachedLayerDisplayMode = layerDisplayMode;
            gState.cachedDisplayLayer = displayLayer;
            gState.cachedLayerAxis = layerAxis;
            gState.cachedOpacity = structureOpacity;
            gState.cachedCorrectionFillOpacity = correctionFillOpacity;
            gState.cachedCorrectionOutlineOpacity = correctionOutlineOpacity;
            // Rotation/mirror alter local block models, so only those require
            // throwing away every GPU mesh. XYZ movement is represented by the
            // world matrix and keeps the existing section meshes alive.
            if (geometryTransformChanged) {
                std::fill(
                    gState.correctionStates.begin(),
                    gState.correctionStates.end(),
                    CorrectionState::Unknown
                );
                for (auto& meshes : gState.sectionMeshes) {
                    for (auto& mesh : meshes) mesh.reset();
                }
                for (auto& mesh : gState.warningFillSectionMeshes) mesh.reset();
                for (auto& mesh : gState.correctionOutlineSectionMeshes) mesh.reset();
                for (auto& mesh : gState.liquidProxySectionMeshes) mesh.reset();
                for (auto& mesh : gState.blockEntityPlaceholderSectionMeshes) mesh.reset();
                gState.structureBoundsMesh.reset();
                std::fill(gState.progressCorrect.begin(), gState.progressCorrect.end(), 0);
                std::fill(gState.progressErrorKind.begin(), gState.progressErrorKind.end(), 0);
                gState.progressCorrectCount = 0;
                gState.progressVisibleCorrectCount = 0;
                gState.progressWrongTypeCount = 0;
                gState.progressWrongStateCount = 0;
                gBuildProgressPlaced.store(0, std::memory_order_release);
                gBuildProgressVisiblePlaced.store(0, std::memory_order_release);
                gBuildProgressWrongType.store(0, std::memory_order_release);
                gBuildProgressWrongState.store(0, std::memory_order_release);
            }

            if (geometryTransformChanged || layerChanged) {
                gState.progressVisibleCorrectCount = 0;
                std::uint64_t visibleTotal{};
                for (std::size_t index = 0; index < gState.structure->renderBlocks.size(); ++index) {
                    auto const& entry = gState.structure->renderBlocks[index];
                    if (!layerIsVisible(layerAxis == 1 ? entry.x : entry.y)) continue;
                    ++visibleTotal;
                    if (gState.progressCorrect[index] != 0) {
                        ++gState.progressVisibleCorrectCount;
                    }
                }
                gBuildProgressVisiblePlaced.store(
                    gState.progressVisibleCorrectCount, std::memory_order_release
                );
                gBuildProgressVisibleTotal.store(
                    visibleTotal, std::memory_order_release
                );
            }

            if (geometryTransformChanged || placementMoved || layerChanged) {
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
            gBuildProgressPlaced.store(gState.progressCorrectCount, std::memory_order_release);
        }
        if (correctionChanges.visible) {
            gBuildProgressVisiblePlaced.store(
                gState.progressVisibleCorrectCount, std::memory_order_release
            );
        }
        if (correctionChanges.errors) {
            gBuildProgressWrongType.store(gState.progressWrongTypeCount, std::memory_order_release);
            gBuildProgressWrongState.store(gState.progressWrongStateCount, std::memory_order_release);
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

        // This is the single geometry implementation used by both the worker
        // (UploadMode::Never) and the compatibility fallback (Buffered).
        auto const buildSectionCpu = [sectionBuildSettings](
            ProjectionState& projectionState,
            Tessellator& tessellator,
            BlockTessellator& blockTessellator,
            BlockSource& region,
            std::size_t section,
            Tessellator::UploadMode uploadMode
        ) {
            detail::buildProjectionSection(
                projectionState,
                tessellator,
                blockTessellator,
                region,
                section,
                uploadMode,
                sectionBuildSettings
            );
        };

        if (tessellator.isTessellating()) tessellator.cancel();

        // Upload completed CPU meshes on the render thread. A result is valid
        // only for the exact worker lifetime, structure, section, and revision.
        if (auto bufferService = tessellator.mBufferResourceService.lock()) {
            auto const uploadStarted = std::chrono::steady_clock::now();
            for (std::size_t uploaded = 0; uploaded < 2; ++uploaded) {
                if (std::chrono::steady_clock::now() - uploadStarted >= std::chrono::milliseconds(1)) break;
                auto completed = takeCompletedSectionBuilds(1);
                if (completed.empty()) break;
                auto result = std::move(completed.front());
                if (result.workerGeneration != gState.meshWorkerGeneration
                    || result.structureGeneration != gState.structureGeneration
                    || result.section >= gState.sectionDirty.size()) {
                    continue;
                }

                auto const section = result.section;
                gState.sectionBuildInFlight[section] = false;
                if (!result.success) {
                    markSectionDirty(gState, section, true);
                    logger().warn(
                        "Projection mesh worker section {} revision {} failed: {} (expected {}, actual {}; snapshot {} us, build {} us)",
                        section,
                        result.revision,
                        result.failureReason.empty() ? "unknown failure" : result.failureReason,
                        result.expectedVertexCount,
                        result.actualVertexCount,
                        result.snapshotPrepareMicros,
                        result.workerBuildMicros
                    );
                    if (++gState.consecutiveMeshWorkerFailures >= 3) {
                        gState.asyncMeshBuildingEnabled = false;
                        disableMeshWorkerForSession();
                        stopMeshWorker();
                        logger().warn("Projection mesh worker failed three times; using synchronous fallback for this session");
                    }
                    continue;
                }
                if (result.revision != gState.sectionRequestedRevision[section]) {
                    gState.sectionDirty[section] = true;
                    continue;
                }

                auto const sectionUploadStarted = std::chrono::steady_clock::now();
                auto uploadCpuMesh = [&](std::unique_ptr<mce::Mesh> cpuMesh, std::string_view name) {
                    if (!cpuMesh || cpuMesh->mMeshData.get().size() == 0) {
                        return std::unique_ptr<mce::Mesh>{};
                    }
                    auto data = std::move(cpuMesh->mMeshData.get());
                    return std::make_unique<mce::Mesh>(bufferService, std::move(data), false, name);
                };
                try {
                    constexpr std::array<std::string_view, static_cast<std::size_t>(RenderBucket::Count)>
                        bucketNames{"LHoloOpaque", "LHoloAlphaTest", "LHoloBlend", "LHoloBlendToOpaque"};
                    std::array<std::unique_ptr<mce::Mesh>, static_cast<std::size_t>(RenderBucket::Count)>
                        uploadedMeshes;
                    for (std::size_t bucket = 0; bucket < uploadedMeshes.size(); ++bucket) {
                        uploadedMeshes[bucket] = uploadCpuMesh(std::move(result.sectionMeshes[bucket]), bucketNames[bucket]);
                    }
                    auto warningFill = uploadCpuMesh(std::move(result.warningFillMesh), "LHoloWarningFill");
                    auto correctionOutline = uploadCpuMesh(
                        std::move(result.correctionOutlineMesh), "LHoloCorrectionOutline"
                    );
                    auto liquidProxy = uploadCpuMesh(std::move(result.liquidProxyMesh), "LHoloLiquidProxy");
                    auto blockEntityPlaceholder = uploadCpuMesh(
                        std::move(result.blockEntityPlaceholderMesh), "LHoloBlockEntityPlaceholder"
                    );

                    for (std::size_t bucket = 0; bucket < uploadedMeshes.size(); ++bucket) {
                        gState.sectionMeshes[bucket][section] = std::move(uploadedMeshes[bucket]);
                    }
                    gState.warningFillSectionMeshes[section] = std::move(warningFill);
                    gState.correctionOutlineSectionMeshes[section] = std::move(correctionOutline);
                    gState.liquidProxySectionMeshes[section] = std::move(liquidProxy);
                    gState.blockEntityPlaceholderSectionMeshes[section] = std::move(blockEntityPlaceholder);
                    gState.sectionUploadedRevision[section] = result.revision;
                    gState.sectionIncrementalDirty[section] = false;
                    gState.sectionDirty[section] = false;
                    gState.consecutiveMeshWorkerFailures = 0;
                    gState.meshPreflightDone = false;
                    auto const uploadMicros = static_cast<std::uint64_t>(
                        std::chrono::duration_cast<std::chrono::microseconds>(
                            std::chrono::steady_clock::now() - sectionUploadStarted
                        ).count()
                    );
                    ++gState.meshWorkerUploadedSections;
                    gState.meshWorkerSnapshotMicros += result.snapshotPrepareMicros;
                    gState.meshWorkerSnapshotDataMicros += result.snapshotDataMicros;
                    gState.meshWorkerChunkViewMicros += result.chunkViewMicros;
                    gState.meshWorkerBuildMicros += result.workerBuildMicros;
                    gState.meshWorkerUploadMicros += uploadMicros;
                    gState.meshWorkerPeakSnapshotMicros = std::max(
                        gState.meshWorkerPeakSnapshotMicros, result.snapshotPrepareMicros
                    );
                    gState.meshWorkerPeakSnapshotDataMicros = std::max(
                        gState.meshWorkerPeakSnapshotDataMicros, result.snapshotDataMicros
                    );
                    gState.meshWorkerPeakChunkViewMicros = std::max(
                        gState.meshWorkerPeakChunkViewMicros, result.chunkViewMicros
                    );
                    gState.meshWorkerPeakBuildMicros = std::max(
                        gState.meshWorkerPeakBuildMicros, result.workerBuildMicros
                    );
                    gState.meshWorkerPeakUploadMicros = std::max(
                        gState.meshWorkerPeakUploadMicros, uploadMicros
                    );
                    if (gState.meshWorkerUploadedSections % 64 == 0) {
                        auto const count = gState.meshWorkerUploadedSections;
                        logger().debug(
                            "Projection mesh worker: {} sections; snapshot {}/{} us (data {}/{}, chunkView {}/{}), build {}/{} us, upload {}/{} us",
                            count,
                            gState.meshWorkerSnapshotMicros / count,
                            gState.meshWorkerPeakSnapshotMicros,
                            gState.meshWorkerSnapshotDataMicros / count,
                            gState.meshWorkerPeakSnapshotDataMicros,
                            gState.meshWorkerChunkViewMicros / count,
                            gState.meshWorkerPeakChunkViewMicros,
                            gState.meshWorkerBuildMicros / count,
                            gState.meshWorkerPeakBuildMicros,
                            gState.meshWorkerUploadMicros / count,
                            gState.meshWorkerPeakUploadMicros
                        );
                    }
                } catch (std::exception const& exception) {
                    logger().warn(
                        "Projection mesh upload for section {} revision {} failed: {}",
                        section, result.revision, exception.what()
                    );
                    markSectionDirty(gState, section, true);
                    if (++gState.consecutiveMeshWorkerFailures >= 3) {
                        gState.asyncMeshBuildingEnabled = false;
                        disableMeshWorkerForSession();
                        stopMeshWorker();
                        logger().warn("Projection mesh upload failed three times; using synchronous fallback for this session");
                    }
                } catch (...) {
                    logger().warn(
                        "Projection mesh upload for section {} revision {} failed with a non-standard exception",
                        section, result.revision
                    );
                    markSectionDirty(gState, section, true);
                    if (++gState.consecutiveMeshWorkerFailures >= 3) {
                        gState.asyncMeshBuildingEnabled = false;
                        disableMeshWorkerForSession();
                        stopMeshWorker();
                        logger().warn("Projection mesh upload failed three times; using synchronous fallback for this session");
                    }
                }
            }
        }

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

        // A single in-flight task gives each BlockTessellator exclusive access
        // and naturally coalesces repeated changes into the latest revision.
        if (gState.asyncMeshBuildingEnabled && !meshWorkerIsBusy()) {
            auto const& cameraPosition = renderContext.getCameraPosition();
            std::optional<std::size_t> selected;
            bool selectedIncremental{};
            float selectedDistance = std::numeric_limits<float>::max();
            for (std::size_t section = 0; section < gState.sectionDirty.size(); ++section) {
                if (!gState.sectionDirty[section] || gState.sectionBuildInFlight[section]) continue;
                auto const incremental = gState.sectionIncrementalDirty[section];
                auto const center = gState.sectionCenters[section] + Vec3{
                    static_cast<float>(gState.anchor.x + offsetX),
                    static_cast<float>(gState.anchor.y + offsetY),
                    static_cast<float>(gState.anchor.z + offsetZ)
                };
                auto const delta = center - cameraPosition;
                auto const distance = delta.x * delta.x + delta.y * delta.y + delta.z * delta.z;
                if (!selected || (incremental && !selectedIncremental)
                    || (incremental == selectedIncremental && distance < selectedDistance)) {
                    selected = section;
                    selectedIncremental = incremental;
                    selectedDistance = distance;
                }
            }

            if (selected) {
                auto const snapshotStarted = std::chrono::steady_clock::now();
                auto const section = *selected;
                BlockPos minimum{INT_MAX, INT_MAX, INT_MAX};
                BlockPos maximum{INT_MIN, INT_MIN, INT_MIN};
                for (auto const index : gState.sectionBlockIndices[section]) {
                    auto const local = transformStructurePosition(
                        gState.structure->renderBlocks[index], *gState.structure, mirrorMode, rotationTurns
                    );
                    BlockPos const world{
                        gState.anchor.x + offsetX + local.x,
                        gState.anchor.y + offsetY + local.y,
                        gState.anchor.z + offsetZ + local.z
                    };
                    minimum.x = std::min(minimum.x, world.x);
                    minimum.y = std::min(minimum.y, world.y);
                    minimum.z = std::min(minimum.z, world.z);
                    maximum.x = std::max(maximum.x, world.x);
                    maximum.y = std::max(maximum.y, world.y);
                    maximum.z = std::max(maximum.z, world.z);
                }
                minimum = BlockPos{minimum.x - 2, minimum.y - 2, minimum.z - 2};
                maximum = BlockPos{maximum.x + 2, maximum.y + 2, maximum.z + 2};

                auto snapshot = std::make_shared<ProjectionState>();
                snapshot->level = gState.level;
                snapshot->dimension = gState.dimension;
                snapshot->structureGeneration = gState.structureGeneration;
                snapshot->anchor = gState.anchor;
                // Structure data and the virtual projected world are immutable
                // for one placement generation. Share them with the worker
                // instead of rebuilding allocation-heavy section/halo maps for
                // every task. Only correction bytes can change incrementally,
                // so those receive a task-local copy.
                snapshot->structure = gState.structure;
                snapshot->correctionStates = gState.correctionStates;
                snapshot->blockActorRendererAvailable = gState.blockActorRendererAvailable;
                snapshot->sectionBlockIndices = {gState.sectionBlockIndices[section]};
                snapshot->expectedWorldBlocks = gState.expectedWorldBlocks;
                snapshot->expectedWorldBlockActors = gState.expectedWorldBlockActors;
                snapshot->expectedWorldBlockIndices = gState.expectedWorldBlockIndices;
                for (auto& meshes : snapshot->sectionMeshes) meshes.resize(1);
                snapshot->warningFillSectionMeshes.resize(1);
                snapshot->correctionOutlineSectionMeshes.resize(1);
                snapshot->liquidProxySectionMeshes.resize(1);
                snapshot->blockEntityPlaceholderSectionMeshes.resize(1);

                auto const snapshotDataFinished = std::chrono::steady_clock::now();
                auto const snapshotDataMicros = static_cast<std::uint64_t>(
                    std::chrono::duration_cast<std::chrono::microseconds>(
                        snapshotDataFinished - snapshotStarted
                    ).count()
                );
                auto chunkView = std::make_shared<ChunkViewSource>(
                    region.getChunkSource(), ChunkSource::LoadMode::Deferred
                );
                chunkView->move(
                    minimum,
                    maximum,
                    false,
                    ChunkSourceViewGenerateMode::DontGenerateOnlyGet,
                    [](gsl::span<std::shared_ptr<LevelChunk>>) {},
                    nullptr
                );
                auto const chunkViewMicros = static_cast<std::uint64_t>(
                    std::chrono::duration_cast<std::chrono::microseconds>(
                        std::chrono::steady_clock::now() - snapshotDataFinished
                    ).count()
                );
                auto const snapshotPrepareMicros = static_cast<std::uint64_t>(
                    std::chrono::duration_cast<std::chrono::microseconds>(
                        std::chrono::steady_clock::now() - snapshotStarted
                    ).count()
                );

                auto const workerGeneration = gState.meshWorkerGeneration;
                auto const structureGeneration = gState.structureGeneration;
                auto const revision = gState.sectionRequestedRevision[section];
                auto const weakBufferService = tessellator.mBufferResourceService;
                auto* level = gState.level;
                auto* dimension = gState.dimension;
                auto submitted = submitMeshWorkerTask(
                    workerGeneration,
                    [buildSectionCpu, snapshot, chunkView, level, dimension, weakBufferService,
                     workerGeneration, structureGeneration, revision, section,
                     snapshotPrepareMicros, snapshotDataMicros, chunkViewMicros]() mutable {
                        AsyncSectionBuildResult result;
                        result.workerGeneration = workerGeneration;
                        result.structureGeneration = structureGeneration;
                        result.revision = revision;
                        result.section = section;
                        result.snapshotPrepareMicros = snapshotPrepareMicros;
                        result.snapshotDataMicros = snapshotDataMicros;
                        result.chunkViewMicros = chunkViewMicros;
                        auto const workerStarted = std::chrono::steady_clock::now();
                        try {
                            auto localRegion = std::make_unique<BlockSource>(
                                *level, *dimension, *chunkView, false, true, false
                            );
                            BlockTessellator localBlockTessellator(localRegion.get());
                            localBlockTessellator.mCachedGetBlock.get()
                                = [snapshot, region = localRegion.get()](BlockPos const& position) -> Block const& {
                                    auto const found = snapshot->expectedWorldBlocks->find(
                                        std::tuple{position.x, position.y, position.z}
                                    );
                                    return found == snapshot->expectedWorldBlocks->end()
                                        ? region->getBlock(position) : *found->second;
                                };
                            Tessellator localTessellator(weakBufferService);
                            buildSectionCpu(
                                *snapshot, localTessellator, localBlockTessellator, *localRegion,
                                0, Tessellator::UploadMode::Never
                            );
                            result.workerBuildMicros = static_cast<std::uint64_t>(
                                std::chrono::duration_cast<std::chrono::microseconds>(
                                    std::chrono::steady_clock::now() - workerStarted
                                ).count()
                            );
                            for (std::size_t bucket = 0; bucket < result.sectionMeshes.size(); ++bucket) {
                                result.sectionMeshes[bucket] = std::move(snapshot->sectionMeshes[bucket][0]);
                            }
                            result.warningFillMesh = std::move(snapshot->warningFillSectionMeshes[0]);
                            result.correctionOutlineMesh = std::move(snapshot->correctionOutlineSectionMeshes[0]);
                            result.liquidProxyMesh = std::move(snapshot->liquidProxySectionMeshes[0]);
                            result.blockEntityPlaceholderMesh
                                = std::move(snapshot->blockEntityPlaceholderSectionMeshes[0]);
                            auto failMeshValidation = [&](std::string_view meshName, std::string_view reason,
                                                          std::uint64_t expected, std::uint64_t actual) {
                                result.failureReason.assign(meshName);
                                result.failureReason.append(": ");
                                result.failureReason.append(reason);
                                result.expectedVertexCount = expected;
                                result.actualVertexCount = actual;
                                return false;
                            };
                            auto meshDataIsConsistent = [&](std::unique_ptr<mce::Mesh> const& mesh,
                                                            std::string_view meshName) {
                                if (!mesh) return true;
                                auto const& data = mesh->mMeshData.get();
                                auto const vertexCount = data.mPositions.get().size();
                                if (vertexCount == 0) {
                                    return failMeshValidation(meshName, "empty position field", 1, 0);
                                }
                                if (data.size() == 0) {
                                    return failMeshValidation(meshName, "MeshData::size is zero", 1, 0);
                                }
                                // UploadMode::Never intentionally leaves the
                                // upload-side Mesh vertex count unset. At this
                                // stage mPositions is authoritative; validate
                                // every enabled CPU attribute against it.
                                auto fieldIsConsistent = [&](auto const& field, std::string_view fieldName) {
                                    if (field.empty() || field.size() == vertexCount) return true;
                                    std::string reason{"attribute count mismatch: "};
                                    reason.append(fieldName);
                                    return failMeshValidation(meshName, reason, vertexCount, field.size());
                                };
                                if (!fieldIsConsistent(data.mNormals.get(), "normals")
                                    || !fieldIsConsistent(data.mTangents.get(), "tangents")
                                    || !fieldIsConsistent(data.mColors.get(), "colors")
                                    || !fieldIsConsistent(data.mBoneId0s.get(), "boneId0")
                                    || !fieldIsConsistent(data.mPBRTextureIndices.get(), "pbrTextureIndices")
                                    || !fieldIsConsistent(data.mMERS.get(), "mers")
                                    || !fieldIsConsistent(data.mGeoType.get(), "geoType")) return false;
                                for (std::size_t uv = 0; uv < std::size(data.mTextureUVs); ++uv) {
                                    std::string fieldName{"textureUV"};
                                    fieldName += static_cast<char>('0' + uv);
                                    if (!fieldIsConsistent(data.mTextureUVs[uv].get(), fieldName)) return false;
                                }
                                return true;
                            };
                            constexpr std::array<std::string_view, 4> meshNames{
                                "opaque", "alpha", "alphaOneSided", "blend"
                            };
                            result.success = true;
                            for (std::size_t bucket = 0; bucket < result.sectionMeshes.size(); ++bucket) {
                                if (!meshDataIsConsistent(result.sectionMeshes[bucket], meshNames[bucket])) {
                                    result.success = false;
                                    break;
                                }
                            }
                            result.success = result.success
                                && meshDataIsConsistent(result.warningFillMesh, "warningFill")
                                && meshDataIsConsistent(result.correctionOutlineMesh, "correctionOutline")
                                && meshDataIsConsistent(result.liquidProxyMesh, "liquidProxy")
                                && meshDataIsConsistent(result.blockEntityPlaceholderMesh, "blockEntityPlaceholder");
                        } catch (std::exception const& exception) {
                            result.workerBuildMicros = static_cast<std::uint64_t>(
                                std::chrono::duration_cast<std::chrono::microseconds>(
                                    std::chrono::steady_clock::now() - workerStarted
                                ).count()
                            );
                            result.success = false;
                            result.failureReason = std::string{"worker exception: "} + exception.what();
                        } catch (...) {
                            result.workerBuildMicros = static_cast<std::uint64_t>(
                                std::chrono::duration_cast<std::chrono::microseconds>(
                                    std::chrono::steady_clock::now() - workerStarted
                                ).count()
                            );
                            result.success = false;
                            result.failureReason = "worker non-standard exception";
                        }
                        return result;
                    }
                );
                if (submitted) {
                    gState.sectionBuildInFlight[section] = true;
                    gState.sectionDirty[section] = false;
                } else if (++gState.consecutiveMeshWorkerFailures >= 3) {
                    gState.asyncMeshBuildingEnabled = false;
                    disableMeshWorkerForSession();
                    stopMeshWorker();
                    logger().warn("Projection mesh task submission failed three times; using synchronous fallback for this session");
                }
            }
        }

        // Compatibility path: preserve one synchronous section per frame when
        // worker creation or three consecutive worker operations fail.
        if (!gState.asyncMeshBuildingEnabled) {
            for (std::size_t attempt = 0; attempt < gState.sectionDirty.size(); ++attempt) {
                auto const section = gState.dirtySectionCursor++ % gState.sectionDirty.size();
                if (!gState.sectionDirty[section]) continue;
                gState.sectionDirty[section] = false;
                buildSectionCpu(
                    gState, tessellator, blockTessellator, region, section,
                    Tessellator::UploadMode::Buffered
                );
                gState.sectionUploadedRevision[section] = gState.sectionRequestedRevision[section];
                gState.sectionIncrementalDirty[section] = false;
                gState.meshPreflightDone = false;
                break;
            }
        }
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
        {
            struct VisibleMesh { std::size_t bucket; std::size_t section; };
            auto worldCenter = [&](std::size_t section) {
                return Vec3{
                    static_cast<float>(renderOrigin.x) + gState.sectionCenters[section].x,
                    static_cast<float>(renderOrigin.y) + gState.sectionCenters[section].y,
                    static_cast<float>(renderOrigin.z) + gState.sectionCenters[section].z
                };
            };
            auto distanceSquared = [&](Vec3 const& point) {
                auto const dx = point.x - camera.x;
                auto const dy = point.y - camera.y;
                auto const dz = point.z - camera.z;
                return dx * dx + dy * dy + dz * dz;
            };
            auto sortBackToFront = [&](std::vector<VisibleMesh>& meshes) {
                std::sort(meshes.begin(), meshes.end(), [&](VisibleMesh const& lhs, VisibleMesh const& rhs) {
                    return distanceSquared(worldCenter(lhs.section))
                        > distanceSquared(worldCenter(rhs.section));
                });
            };
            auto renderMeshes = [&](std::vector<VisibleMesh> const& meshes, mce::MaterialPtr const& material) {
                if (!material) return;
                for (auto const& visible : meshes) {
                    auto& mesh = *gState.sectionMeshes[visible.bucket][visible.section];
                    mesh.renderMesh(
                        renderContext.getScreenContext(),
                        material,
                        *gState.terrainTextureVariant,
                        0,
                        static_cast<uint>(mesh.getMeshVertexCount()),
                        renderContext.mOffscreenCaptureDescription.get(),
                        nullptr
                    );
                }
            };
            auto collectBucket = [&](std::size_t bucket) {
                std::vector<VisibleMesh> result;
                auto const& meshes = gState.sectionMeshes[bucket];
                result.reserve(meshes.size());
                for (std::size_t section = 0; section < meshes.size(); ++section) {
                    if (meshes[section] && meshes[section]->isValid()) {
                        result.push_back({bucket, section});
                    }
                }
                return result;
            };

            auto const opaqueBucket = static_cast<std::size_t>(RenderBucket::Opaque);
            auto const alphaBucket = static_cast<std::size_t>(RenderBucket::Alpha);
            auto const alphaOneSidedBucket = static_cast<std::size_t>(RenderBucket::AlphaOneSided);
            auto const blendBucket = static_cast<std::size_t>(RenderBucket::Blend);
            if (structureOpacity >= 0.999f) {
                auto opaqueMeshes = collectBucket(opaqueBucket);
                auto alphaMeshes = collectBucket(alphaBucket);
                auto alphaOneSidedMeshes = collectBucket(alphaOneSidedBucket);
                auto transparentMeshes = collectBucket(blendBucket);
                sortBackToFront(transparentMeshes);

                auto const& opaqueMaterial = itemRenderer.mMatOpaqueBlock.get();
                auto const& alphaMaterial = itemRenderer.mMatAlphaBlock.get();
                auto const& alphaOneSidedMaterial = itemRenderer.mMatAlphaOneSidedBlock.get();
                if (!renderAlphaLayer) {
                    renderMeshes(opaqueMeshes, opaqueMaterial ? opaqueMaterial : blendMaterial);
                    renderMeshes(alphaMeshes, alphaMaterial ? alphaMaterial : blendMaterial);
                    renderMeshes(
                        alphaOneSidedMeshes,
                        alphaOneSidedMaterial ? alphaOneSidedMaterial : (alphaMaterial ? alphaMaterial : blendMaterial)
                    );
                } else {
                    renderMeshes(transparentMeshes, blendMaterial);
                }
            } else if (renderAlphaLayer) {
                // True projection transparency needs a blending material even
                // for normally opaque/cutout blocks. Sort all buckets together
                // so changing opacity never reintroduces inter-section flicker.
                std::vector<VisibleMesh> transparentMeshes;
                for (std::size_t bucket = 0; bucket < gState.sectionMeshes.size(); ++bucket) {
                    auto bucketMeshes = collectBucket(bucket);
                    transparentMeshes.insert(
                        transparentMeshes.end(), bucketMeshes.begin(), bucketMeshes.end()
                    );
                }
                sortBackToFront(transparentMeshes);
                renderMeshes(transparentMeshes, blendMaterial);
            }

            // Textured liquid hulls travel the proven glass path: blend-block
            // material plus the terrain atlas, sorted back to front by
            // section like the other transparent meshes.
            if (renderAlphaLayer) {
                std::vector<std::size_t> liquidSections;
                for (std::size_t liquidSection = 0;
                     liquidSection < gState.liquidProxySectionMeshes.size();
                     ++liquidSection) {
                    auto const& mesh = gState.liquidProxySectionMeshes[liquidSection];
                    if (mesh && mesh->isValid()) liquidSections.push_back(liquidSection);
                }
                std::sort(
                    liquidSections.begin(),
                    liquidSections.end(),
                    [&](std::size_t lhs, std::size_t rhs) {
                        return distanceSquared(worldCenter(lhs))
                            > distanceSquared(worldCenter(rhs));
                    }
                );
                for (auto const liquidSection : liquidSections) {
                    auto& mesh = *gState.liquidProxySectionMeshes[liquidSection];
                    mesh.renderMesh(
                        renderContext.getScreenContext(),
                        blendMaterial,
                        *gState.terrainTextureVariant,
                        0,
                        static_cast<uint>(mesh.getMeshVertexCount()),
                        renderContext.mOffscreenCaptureDescription.get(),
                        nullptr
                    );
                }

                // Textured placeholder hulls for block-entity blocks.
                for (auto const& placeholder : gState.blockEntityPlaceholderSectionMeshes) {
                    if (!placeholder || !placeholder->isValid()) continue;
                    placeholder->renderMesh(
                        renderContext.getScreenContext(),
                        blendMaterial,
                        *gState.terrainTextureVariant,
                        0,
                        static_cast<uint>(placeholder->getMeshVertexCount()),
                        renderContext.mOffscreenCaptureDescription.get(),
                        nullptr
                    );
                }
            }
        }

        if (renderAlphaLayer) {
            auto* levelRenderer = client.getLevelRenderer();
            auto const& outlineMaterial = levelRenderer
                ? levelRenderer->getLevelRendererPlayer().mOutlineSelectionMaterial.get()
                : renderContext.getItemInHandRenderer().mMatBlendBlock.get();
            if (outlineMaterial && gStructureBoundsEnabled.load(std::memory_order_relaxed)
                && gState.structureBoundsMesh && gState.structureBoundsMesh->isValid()) {
                gState.structureBoundsMesh->renderMesh(
                    renderContext.getScreenContext(),
                    outlineMaterial,
                    0,
                    static_cast<uint>(gState.structureBoundsMesh->getMeshVertexCount()),
                    renderContext.mOffscreenCaptureDescription.get(),
                    nullptr
                );
            }
            auto const& warningMaterial = levelRenderer
                ? levelRenderer->getLevelRendererPlayer().selectionBlockEntityOverlayColorMaterial.get()
                : renderContext.getItemInHandRenderer().mMatBlendBlockNoColor.get();
            auto renderOverlayMeshes = [&](
                std::vector<std::unique_ptr<mce::Mesh>> const& meshes, mce::MaterialPtr const& material
            ) {
                if (!material) return;
                for (auto const& overlay : meshes) {
                    if (!overlay || !overlay->isValid()) continue;
                    overlay->renderMesh(
                        renderContext.getScreenContext(),
                        material,
                        0,
                        static_cast<uint>(overlay->getMeshVertexCount()),
                        renderContext.mOffscreenCaptureDescription.get(),
                        nullptr
                    );
                }
            };

            if (static_cast<bool>(itemRenderer.mIsDeferredEnabled)) {
                // The colored outline material is already proven stable and
                // correctly exposed by Vibrant Visuals. Reuse the same shader
                // for the hull, changing only its primitive and blend state
                // for this submission. Restoring immediately keeps the normal
                // correction and structure outlines untouched.
                auto* renderMaterial = outlineMaterial
                    ? const_cast<mce::RenderMaterial*>(outlineMaterial.operator->())
                    : nullptr;
                if (renderMaterial) {
                    auto const savedPrimitive = renderMaterial->mPrimitiveMode;
                    auto const savedBlend = renderMaterial->blendStateDescription.get();
                    auto const savedDepthBias = renderMaterial->mDepthBias;
                    auto const savedSlopeBias = renderMaterial->mSlopeScaledDepthBias;
                    renderMaterial->mPrimitiveMode = mce::PrimitiveMode::QuadList;
                    renderMaterial->blendStateDescription.get()
                        = blendMaterial->blendStateDescription.get();
                    renderMaterial->mDepthBias = 100.0f;
                    renderMaterial->mSlopeScaledDepthBias = 15.0f;
                    renderOverlayMeshes(gState.warningFillSectionMeshes, outlineMaterial);
                    renderMaterial->mPrimitiveMode = savedPrimitive;
                    renderMaterial->blendStateDescription.get() = savedBlend;
                    renderMaterial->mDepthBias = savedDepthBias;
                    renderMaterial->mSlopeScaledDepthBias = savedSlopeBias;
                }
            } else if (warningMaterial) {
                // Bedrock's selection overlay already supplies the D3D depth
                // bias equivalent of Java's polygon offset, but its default
                // blend equation is multiplicative. Temporarily borrow the
                // standard SourceAlpha/OneMinusSourceAlpha state from the
                // vanilla blend-block material so the 0x4C overlay preserves
                // the real block texture beneath it.
                auto* renderMaterial = levelRenderer
                    ? const_cast<mce::RenderMaterial*>(warningMaterial.operator->())
                    : nullptr;
                struct MaterialStateRestore {
                    mce::RenderMaterial* material{};
                    std::optional<mce::BlendStateDescription> blend;
                    float depthBias{};
                    float slopeBias{};
                    ~MaterialStateRestore() {
                        if (!material || !blend) return;
                        material->blendStateDescription.get() = *blend;
                        material->mDepthBias = depthBias;
                        material->mSlopeScaledDepthBias = slopeBias;
                    }
                } restore;
                if (renderMaterial && blendMaterial) {
                    restore.material = renderMaterial;
                    restore.blend = renderMaterial->blendStateDescription.get();
                    restore.depthBias = renderMaterial->mDepthBias;
                    restore.slopeBias = renderMaterial->mSlopeScaledDepthBias;
                    renderMaterial->blendStateDescription.get()
                        = blendMaterial->blendStateDescription.get();
                    renderMaterial->mDepthBias = 100.0f;
                    renderMaterial->mSlopeScaledDepthBias = 15.0f;
                }
                renderOverlayMeshes(gState.warningFillSectionMeshes, warningMaterial);
            }
            if (outlineMaterial) {
                for (auto const& correctionOutline : gState.correctionOutlineSectionMeshes) {
                    if (!correctionOutline || !correctionOutline->isValid()) continue;
                    correctionOutline->renderMesh(
                        renderContext.getScreenContext(),
                        outlineMaterial,
                        0,
                        static_cast<uint>(correctionOutline->getMeshVertexCount()),
                        renderContext.mOffscreenCaptureDescription.get(),
                        nullptr
                    );
                }
            }
        }
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
    BuildProgress result;
    result.total = gBuildProgressTotal.load(std::memory_order_acquire);
    result.placed = gBuildProgressPlaced.load(std::memory_order_acquire);
    result.visibleTotal = gBuildProgressVisibleTotal.load(std::memory_order_acquire);
    result.visiblePlaced = gBuildProgressVisiblePlaced.load(std::memory_order_acquire);
    result.wrongType = gBuildProgressWrongType.load(std::memory_order_acquire);
    result.wrongState = gBuildProgressWrongState.load(std::memory_order_acquire);
    if (result.placed > result.total) result.placed = result.total;
    if (result.visiblePlaced > result.visibleTotal) result.visiblePlaced = result.visibleTotal;
    if (result.wrongType > result.total) result.wrongType = result.total;
    if (result.wrongState > result.total) result.wrongState = result.total;
    return result;
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
    auto const key = std::tuple{worldPos.x, worldPos.y, worldPos.z};
    auto const foundIndex = gState.expectedWorldBlockIndices->find(key);
    if (foundIndex == gState.expectedWorldBlockIndices->end()) return {nullptr, false};
    auto const foundBlock = gState.expectedWorldBlocks->find(key);
    Block const* block = foundBlock == gState.expectedWorldBlocks->end() ? nullptr : foundBlock->second;
    // Liquids have no normal block item, so they are never a valid place target.
    if (block && block->getMaterial().isLiquid()) block = nullptr;
    bool const missing = gState.correctionStates[foundIndex->second]
        == CorrectionState::Missing;
    return {block, missing};
}

std::vector<RangeCandidate> queryMissingCellsInRange(LocalPlayer& player, Vec3 const& center, float radius) {
    std::vector<RangeCandidate> result;
    std::unique_lock lock(gStateMutex);
    if (!gState.enabled || !gState.structure) return result;
    if (gState.level != &player.getLevel() || gState.dimension != &player.getDimension()) {
        clearProjectionStateLocked();
        lock.unlock();
        structure::clear();
        return result;
    }
    // Only visit cells in the axis-aligned box around the center: with a small
    // radius this is far cheaper than walking the whole virtual-world map.
    int const r = static_cast<int>(std::ceil(radius));
    int const minX = static_cast<int>(std::floor(center.x)) - r;
    int const maxX = static_cast<int>(std::floor(center.x)) + r;
    int const minY = static_cast<int>(std::floor(center.y)) - r;
    int const maxY = static_cast<int>(std::floor(center.y)) + r;
    int const minZ = static_cast<int>(std::floor(center.z)) - r;
    int const maxZ = static_cast<int>(std::floor(center.z)) + r;
    float const r2 = radius * radius;
    for (int y = minY; y <= maxY; ++y) {
        for (int z = minZ; z <= maxZ; ++z) {
            for (int x = minX; x <= maxX; ++x) {
                auto const key = std::tuple{x, y, z};
                auto const foundIndex = gState.expectedWorldBlockIndices->find(key);
                if (foundIndex == gState.expectedWorldBlockIndices->end()) continue;
                if (gState.correctionStates[foundIndex->second] != CorrectionState::Missing) continue;
                float const dx = static_cast<float>(x) + 0.5f - center.x;
                float const dy = static_cast<float>(y) + 0.5f - center.y;
                float const dz = static_cast<float>(z) + 0.5f - center.z;
                if (dx * dx + dy * dy + dz * dz > r2) continue;
                auto const foundBlock = gState.expectedWorldBlocks->find(key);
                Block const* block = foundBlock == gState.expectedWorldBlocks->end() ? nullptr : foundBlock->second;
                // Liquids have no normal block item, so they are never a place target.
                if (block && block->getMaterial().isLiquid()) block = nullptr;
                if (!block) continue;
                result.push_back({x, y, z, block});
            }
        }
    }
    std::sort(result.begin(), result.end(), [&center](RangeCandidate const& a, RangeCandidate const& b) {
        auto const distSq = [&center](RangeCandidate const& c) {
            float const dx = static_cast<float>(c.x) + 0.5f - center.x;
            float const dy = static_cast<float>(c.y) + 0.5f - center.y;
            float const dz = static_cast<float>(c.z) + 0.5f - center.z;
            return dx * dx + dy * dy + dz * dz;
        };
        return distSq(a) < distSq(b);
    });
    return result;
}

} // namespace lholo::projection
