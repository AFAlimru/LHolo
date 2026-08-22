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
#include "projection/ProjectionController.h"
#include "projection/runtime/ProjectionProgress.h"
#include "projection/runtime/ProjectionSession.h"
#include "projection/core/ProjectionState.h"
#include "projection/world/ProjectionQueries.h"

#include "structure/StructureLoader.h"

#include <mutex>
#include <vector>

#include "mc/client/player/LocalPlayer.h"
#include "mc/world/level/BlockPos.h"
#include "mc/world/level/Level.h"

namespace lholo::projection {

bool installHook() {
    return detail::projectionController().installHooks();
}

void uninstallHook() {
    detail::projectionController().uninstallHooks();
}

void disable() {
    detail::projectionController().disableProjection();
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
