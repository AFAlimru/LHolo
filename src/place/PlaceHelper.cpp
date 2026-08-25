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

#include "place/PlaceHelper.h"

#include "place/PlacementExecutor.h"
#include "place/PlacementState.h"

#include "plugin/LHolo.h"

#include "ll/api/memory/Hook.h"
#include "ll/api/mod/NativeMod.h"
#include "ll/api/service/Bedrock.h"

#include "mc/client/game/ClientInstance.h"
#include "mc/client/game/IClientInstance.h"
#include "mc/client/player/LocalPlayer.h"
#include "mc/world/gamemode/GameMode.h"
#include "mc/world/actor/player/Player.h"
#include "mc/world/item/ItemStack.h"
#include "mc/world/level/BlockPos.h"
#include "mc/world/level/Tick.h"

#include <Windows.h>

#include <algorithm>

namespace lholo::place {
namespace {

auto& placementState() {
    return detail::PlacementState::getInstance();
}

auto& logger() {
    return LHolo::getInstance().getSelf().getLogger();
}

LL_TYPE_INSTANCE_HOOK(
    LocalPlayerEasyPlaceHook,
    ll::memory::HookPriority::Normal,
    LocalPlayer,
    &LocalPlayer::$tickWorld,
    void,
    ::Tick const& currentTick
) {
    detail::tickEasyPlace();
    origin(currentTick);
}

// Returns true when manual mode is on and `gm` belongs to the local player, i.e.
// this is the client-side right-click we should take over. The local-player
// check is essential: the server processes LHolo's own placement through these
// same functions on the ServerPlayer, and that must not be suppressed.
bool isLocalManualBuild(GameMode& gm) {
    if (!placementState().manualMode()) return false;
    auto client = ll::service::getClientInstance();
    auto* localPlayer = client ? client->getLocalPlayer() : nullptr;
    return localPlayer && &gm.mPlayer == static_cast<Player*>(localPlayer);
}

void requestManualPlacement(bool held) {
    placementState().setManualPressAt(GetTickCount64());
    placementState().setManualPlaceRequested(true);
    placementState().setManualHeld(held);
}

// Manual-mode press edge: the initial right-click. Begin a held sequence (first
// block placed immediately by tickEasyPlace, then typematic repeat) and cancel
// the vanilla build start so nothing is placed twice.
LL_TYPE_INSTANCE_HOOK(
    GameModeStartBuildHook,
    ll::memory::HookPriority::Normal,
    GameMode,
    &GameMode::$startBuildBlock,
    void,
    ::BlockPos const& pos,
    uchar             face
) {
    if (isLocalManualBuild(*this)) {
        requestManualPlacement(true);
        return;  // LHolo handles the placement from tickEasyPlace.
    }
    origin(pos, face);
}

// Right-clicking a floating projection targets air, so Bedrock calls useItem
// instead of startBuildBlock. Capture it as a one-shot request: unlike the
// build path, air use has no matching stopBuildBlock edge we can rely on.
LL_TYPE_INSTANCE_HOOK(
    GameModeUseItemHook,
    ll::memory::HookPriority::Normal,
    GameMode,
    &GameMode::$useItem,
    bool,
    ::ItemStack& item
) {
    if (isLocalManualBuild(*this)) {
        requestManualPlacement(false);
        return false;
    }
    return origin(item);
}

// Manual-mode release edge: stop the typematic repeat when the button is let go.
LL_TYPE_INSTANCE_HOOK(
    GameModeStopBuildHook,
    ll::memory::HookPriority::Normal,
    GameMode,
    &GameMode::$stopBuildBlock,
    void
) {
    if (isLocalManualBuild(*this)) {
        placementState().setManualHeld(false);
        return;
    }
    origin();
}

// Suppress the vanilla continuous build while the button is held in manual mode;
// LHolo drives placement from the press/hold state above.
LL_TYPE_INSTANCE_HOOK(
    GameModeBuildBlockHook,
    ll::memory::HookPriority::Normal,
    GameMode,
    &GameMode::$buildBlock,
    bool,
    ::BlockPos const& pos,
    uchar             face,
    bool const        isSimTick
) {
    if (isLocalManualBuild(*this)) return false;
    return origin(pos, face, isSimTick);
}

} // namespace

void setEnabled(bool enabled) {
    if (enabled) {
        logger().info("Easy-place enabled");
    } else {
        logger().info("Easy-place disabled");
    }
    placementState().setEnabled(enabled);
}

bool isEnabled() {
    return placementState().enabled();
}

void setRangeEnabled(bool enabled) {
    if (enabled) {
        logger().info("Range placement enabled (radius {})", placementState().radius());
    } else {
        logger().info("Range placement disabled");
    }
    placementState().setRangeEnabled(enabled);
}

bool isRangeEnabled() {
    return placementState().rangeEnabled();
}

void setPlacementRadius(int radius) {
    placementState().setRadius(std::clamp(radius, 1, 4));
}

int getPlacementRadius() {
    return placementState().radius();
}

void setManualMode(bool manual) {
    if (!manual) {
        // A release hook can be missed while menus or mode switches are active.
        // Never carry a stale press/hold request into the next manual session.
        placementState().setManualHeld(false);
        placementState().setManualPlaceRequested(false);
        placementState().setManualPressAt(0);
        placementState().setLastManualPlaceAt(0);
    }
    placementState().setManualMode(manual);
}

bool isManualMode() {
    return placementState().manualMode();
}

std::string getAimedProjectedBlockName() {
    return placementState().aimedProjectedBlockName();
}

bool installHook() {
    if (LocalPlayerEasyPlaceHook::hook() < 0) {
        logger().error("Failed to install easy-place tick hook");
        return false;
    }
    if (GameModeStartBuildHook::hook() < 0) {
        logger().warn("Failed to install manual-place start hook; manual mode will be unavailable");
    }
    if (GameModeUseItemHook::hook() < 0) {
        logger().warn("Failed to install manual-place air-use hook; floating manual placement will be unavailable");
    }
    if (GameModeStopBuildHook::hook() < 0) {
        logger().warn("Failed to install manual-place stop hook; manual mode may keep repeating");
    }
    if (GameModeBuildBlockHook::hook() < 0) {
        logger().warn("Failed to install manual-place build hook; manual mode may double-place");
    }
    return true;
}

void uninstallHook() {
    GameModeBuildBlockHook::unhook();
    GameModeStopBuildHook::unhook();
    GameModeUseItemHook::unhook();
    GameModeStartBuildHook::unhook();
    LocalPlayerEasyPlaceHook::unhook();
}

} // namespace lholo::place
