// LHolo - Client-side projection renderer for Minecraft Bedrock Windows
// Copyright (C) 2026  MarmieQi

#include "input/MenuInputGuard.h"

#include "structure/StructureLoader.h"

#include "ll/api/memory/Hook.h"
#include "ll/api/service/Bedrock.h"

#include "mc/client/game/ClientInstance.h"
#include "mc/client/player/LocalPlayer.h"
#include "mc/deps/core/math/Vec3.h"
#include "mc/world/actor/player/Player.h"
#include "mc/world/gamemode/GameMode.h"
#include "mc/world/level/BlockPos.h"

namespace lholo::input {
namespace {

bool shouldBlockDestroy(GameMode& gameMode) {
    if (!structure::isGuiVisible() && !structure::isInputTransitionBlocked()) return false;

    auto client = ll::service::getClientInstance();
    auto* player = client ? client->getLocalPlayer() : nullptr;
    return player && &gameMode.mPlayer == static_cast<Player*>(player);
}

LL_TYPE_INSTANCE_HOOK(
    MenuStartDestroyBlockHook,
    ll::memory::HookPriority::Normal,
    GameMode,
    &GameMode::$startDestroyBlock,
    bool,
    ::BlockPos const& pos,
    uchar face,
    bool& hasDestroyedBlock
) {
    if (shouldBlockDestroy(*this)) {
        hasDestroyedBlock = false;
        return false;
    }
    return origin(pos, face, hasDestroyedBlock);
}

LL_TYPE_INSTANCE_HOOK(
    MenuContinueDestroyBlockHook,
    ll::memory::HookPriority::Normal,
    GameMode,
    &GameMode::$continueDestroyBlock,
    bool,
    ::BlockPos const& pos,
    uchar face,
    ::Vec3 const& playerPos,
    bool& hasDestroyedBlock
) {
    if (shouldBlockDestroy(*this)) {
        hasDestroyedBlock = false;
        return false;
    }
    return origin(pos, face, playerPos, hasDestroyedBlock);
}

} // namespace

bool installMenuInputGuard() {
    if (MenuStartDestroyBlockHook::hook() < 0) return false;
    if (MenuContinueDestroyBlockHook::hook() < 0) {
        MenuStartDestroyBlockHook::unhook();
        return false;
    }
    return true;
}

void uninstallMenuInputGuard() {
    MenuContinueDestroyBlockHook::unhook();
    MenuStartDestroyBlockHook::unhook();
}

} // namespace lholo::input
