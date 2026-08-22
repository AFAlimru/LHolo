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
#include "projection/runtime/ProjectionFramePipeline.h"
#include "projection/hooks/ProjectionGameHooks.h"
#include "projection/hooks/ProjectionRenderHooks.h"
#include "projection/core/ProjectionInternalTypes.h"
#include "projection/runtime/ProjectionInvalidation.h"
#include "projection/runtime/ProjectionLifecycle.h"
#include "projection/mesh/ProjectionMeshWorker.h"
#include "projection/world/ProjectionPlacement.h"
#include "projection/runtime/ProjectionProgress.h"
#include "projection/world/ProjectionQueries.h"
#include "projection/mesh/ProjectionRenderer.h"
#include "projection/core/ProjectionRules.h"
#include "projection/mesh/ProjectionSectionBuilder.h"
#include "projection/core/ProjectionState.h"
#include "projection/runtime/ProjectionWorldEvents.h"
#include "projection/runtime/ProjectionSession.h"

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

void clearProjectionState() {
    std::lock_guard lock(detail::projectionStateMutex());
    detail::clearProjectionStateLocked();
}

} // namespace

bool installHook() {
    if (!detail::installProjectionGameHooks()) return false;
    if (!detail::installProjectionRenderHooks()) {
        detail::uninstallProjectionGameHooks();
        return false;
    }
    return true;
}

void uninstallHook() {
    detail::uninstallProjectionRenderHooks();
    detail::uninstallProjectionGameHooks();
    std::lock_guard lock(detail::projectionStateMutex());
    detail::projectionCaptureBounds().clear();
}

void disable() {
    clearProjectionState();
}

float getOpacity() {
    return detail::projectionOpacity();
}

void setOpacity(float opacity) {
    detail::setProjectionOpacity(opacity);
}

float getCorrectionFillOpacity() {
    return detail::projectionCorrectionFillOpacity();
}

void setCorrectionFillOpacity(float opacity) {
    detail::setProjectionCorrectionFillOpacity(opacity);
}

float getCorrectionOutlineOpacity() {
    return detail::projectionCorrectionOutlineOpacity();
}

void setCorrectionOutlineOpacity(float opacity) {
    detail::setProjectionCorrectionOutlineOpacity(opacity);
}

bool getStructureBoundsEnabled() {
    return detail::projectionStructureBoundsEnabled();
}

void setStructureBoundsEnabled(bool enabled) {
    detail::setProjectionStructureBoundsEnabled(enabled);
}

void requestNextStructureAnchor(int x, int y, int z) {
    detail::requestProjectionAnchor(x, y, z);
}

BuildProgress getBuildProgress() {
    return detail::getPublishedBuildProgress();
}

ProjectionQuery queryProjection(LocalPlayer& player, BlockPos const& worldPos) {
    std::unique_lock lock(detail::projectionStateMutex());
    auto& state = detail::projectionState();
    if (!state.enabled || !state.structure) return {nullptr, false};
    if (state.level != &player.getLevel() || state.dimension != &player.getDimension()) {
        detail::clearProjectionStateLocked();
        lock.unlock();
        structure::clear();
        return {nullptr, false};
    }
    return detail::queryProjectionCell(state, worldPos);
}

std::vector<RangeCandidate> queryMissingCellsInRange(LocalPlayer& player, Vec3 const& center, float radius) {
    std::unique_lock lock(detail::projectionStateMutex());
    auto& state = detail::projectionState();
    if (!state.enabled || !state.structure) return {};
    if (state.level != &player.getLevel() || state.dimension != &player.getDimension()) {
        detail::clearProjectionStateLocked();
        lock.unlock();
        structure::clear();
        return {};
    }
    return detail::queryMissingProjectionCells(state, center, radius);
}

} // namespace lholo::projection
