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

#include "plugin/LHolo.h"
#include "projection/Projection.h"
#include "structure/StructureLoader.h"

#include "ll/api/memory/Hook.h"
#include "ll/api/mod/NativeMod.h"
#include "ll/api/service/Bedrock.h"

#include "mc/client/game/ClientInstance.h"
#include "mc/client/game/IClientInstance.h"
#include "mc/client/player/LocalPlayer.h"
#include "mc/deps/core/math/Vec3.h"
#include "mc/network/PacketSender.h"
#include "mc/network/packet/InventoryTransactionPacket.h"
#include "mc/world/Facing.h"
#include "mc/world/ContainerID.h"
#include "mc/world/actor/player/Inventory.h"
#include "mc/world/actor/player/Player.h"
#include "mc/world/inventory/transaction/ComplexInventoryTransaction.h"
#include "mc/world/inventory/transaction/InventoryAction.h"
#include "mc/world/inventory/transaction/InventorySource.h"
#include "mc/world/inventory/transaction/InventorySourceType.h"
#include "mc/world/inventory/transaction/InventoryTransaction.h"
#include "mc/world/inventory/transaction/ItemUseInventoryTransaction.h"
#include "mc/world/item/ItemStack.h"
#include "mc/world/level/BlockPos.h"
#include "mc/world/level/BlockSource.h"
#include "mc/world/level/Tick.h"
#include "mc/world/level/block/Block.h"
#include "mc/world/level/block/actor/BlockActorType.h"

#include <Windows.h>

#include <atomic>
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>

namespace lholo::place {
namespace {

// Easy-place searches the full inventory (hotbar 0-8, backpack 9-35) for the
// matching block item and references the found slot directly in the placement
// transaction, so no cross-container inventory request is needed.
constexpr int kHotbarSlots = 9;
constexpr int kInventorySlots = 36;
// A cell that was just placed must not be re-targeted until the server applies
// it and the correction scan catches up. This window prevents hammering one
// cell; new cells along the ray are placed immediately (bounded by the tick).
constexpr std::uint64_t kCellLockMs = 200;
// Safety floor between any two placements. The tick hook already runs at 20 Hz,
// so this just guards against double-sends on unusual tick rates.
constexpr std::uint64_t kMinSendIntervalMs = 40;
// Backoff for a rejected inventory swap. Without it a failed swap retries every
// tick and spams the server.
constexpr std::uint64_t kSwapRetryMs = 200;

std::atomic_bool gEnabled{false};
std::atomic_uint64_t gNextPlaceAt{0};
std::optional<BlockPos> gLastPlaceCell;
std::atomic_uint64_t gLastPlaceAt{0};
std::atomic_uint64_t gNextSwapAt{0};
// Name of the block-entity block the crosshair currently points at, shown in
// the HUD so projected chests/signs/hoppers/... can be identified.
std::string gAimedBlockEntityName;
std::mutex  gAimedNameMutex;

void updateAimedBlockEntityName(Block const* block) {
    std::string name;
    if (block && block->getBlockEntityType() != BlockActorType::Undefined) {
        ItemStack const item(*block, 1, nullptr);
        name = item.getName();
    }
    std::lock_guard lock(gAimedNameMutex);
    gAimedBlockEntityName = std::move(name);
}

auto& logger() {
    return LHolo::getInstance().getSelf().getLogger();
}

struct ItemFind {
    int             slot;
    ItemStack const* item;
};

ItemFind findItemSlot(Player& player, ItemStack const& want) {
    auto& inventory = player.getInventory();
    for (int slot = 0; slot < kInventorySlots; ++slot) {
        auto const& item = inventory.getItem(slot);
        if (item.sameItemAndAuxAndBlockData(want)) return {slot, &item};
    }
    return {-1, nullptr};
}

// Server-synced slot exchange expressed as a legacy NormalTransaction: both
// slots swap their items, so it stays valid whether the target slot is empty
// or occupied, and the server keeps its item-stack-net bookkeeping consistent
// (unlike a direct container mutation, which the net manager reverts).
void sendInventorySwap(LocalPlayer& player, int fromSlot, int toSlot, ItemStack const& fromItem, ItemStack const& toItem) {
    auto transaction = ComplexInventoryTransaction::fromType(ComplexInventoryTransaction::Type::NormalTransaction);
    if (!transaction) return;
    auto& invTx = transaction->mTransaction.get();
    InventorySource const source{
        InventorySourceType::ContainerInventory,
        ContainerID::Inventory,
        InventorySource::InventorySourceFlags::NoFlag
    };
    invTx.addAction(InventoryAction{source, static_cast<uint>(fromSlot), fromItem, toItem});
    invTx.addAction(InventoryAction{source, static_cast<uint>(toSlot), toItem, fromItem});
    InventoryTransactionPacket packet(std::move(transaction), true);
    player.getClientInstance().getPacketSender().sendToServer(packet);
}

// A projected ghost cell the player aims at: the cell itself, the real block
// to place against, the face such that at.neighbor(face) == cell, and the
// expected block that fills the cell.
struct ProjectionTarget {
    BlockPos     cell;
    BlockPos     at;
    uchar        face;
    Block const* block;
};

// Voxel raycast (Amanatides & Woo) against the real world plus the projection's
// virtual grid. The vanilla crosshair hit result never sees LHolo's drawn ghost
// blocks, so the ray is traced manually: real blocks block it, projected
// missing cells are the placement targets.
std::optional<ProjectionTarget> findProjectionTarget(
    LocalPlayer& player,
    Vec3 const&  origin,
    Vec3 const&  dir,
    float        maxDist
) {
    auto& region = player.getDimensionBlockSource();

    int const stepX = dir.x > 0.0f ? 1 : -1;
    int const stepY = dir.y > 0.0f ? 1 : -1;
    int const stepZ = dir.z > 0.0f ? 1 : -1;
    float const tDeltaX = dir.x != 0.0f ? std::abs(1.0f / dir.x) : std::numeric_limits<float>::infinity();
    float const tDeltaY = dir.y != 0.0f ? std::abs(1.0f / dir.y) : std::numeric_limits<float>::infinity();
    float const tDeltaZ = dir.z != 0.0f ? std::abs(1.0f / dir.z) : std::numeric_limits<float>::infinity();

    int x = static_cast<int>(std::floor(origin.x));
    int y = static_cast<int>(std::floor(origin.y));
    int z = static_cast<int>(std::floor(origin.z));
    BlockPos const originCell{x, y, z};
    float tMaxX = (stepX > 0 ? (static_cast<float>(x) + 1.0f - origin.x) : (origin.x - static_cast<float>(x))) * tDeltaX;
    float tMaxY = (stepY > 0 ? (static_cast<float>(y) + 1.0f - origin.y) : (origin.y - static_cast<float>(y))) * tDeltaY;
    float tMaxZ = (stepZ > 0 ? (static_cast<float>(z) + 1.0f - origin.z) : (origin.z - static_cast<float>(z))) * tDeltaZ;

    for (int step = 0; step < 512; ++step) {
        float tEnter;
        uchar entryFace;
        if (tMaxX < tMaxY && tMaxX < tMaxZ) {
            x += stepX;
            tEnter = tMaxX;
            tMaxX += tDeltaX;
            entryFace = stepX > 0 ? static_cast<uchar>(Facing::Name::West) : static_cast<uchar>(Facing::Name::East);
        } else if (tMaxY < tMaxZ) {
            y += stepY;
            tEnter = tMaxY;
            tMaxY += tDeltaY;
            entryFace = stepY > 0 ? static_cast<uchar>(Facing::Name::Down) : static_cast<uchar>(Facing::Name::Up);
        } else {
            z += stepZ;
            tEnter = tMaxZ;
            tMaxZ += tDeltaZ;
            entryFace = stepZ > 0 ? static_cast<uchar>(Facing::Name::North) : static_cast<uchar>(Facing::Name::South);
        }
        if (tEnter > maxDist) break;

        BlockPos const cell{x, y, z};
        if (!region.getBlock(cell).isAir()) {
            // A real block blocks the ray. Placing into the camera-side cell
            // (the vanilla placement position) fills an adjacent ghost. Never
            // target the cell the camera itself is standing in.
            BlockPos const neighbor = cell.neighbor(entryFace);
            auto const query = projection::queryProjection(neighbor);
            if (neighbor != originCell && query.block && query.missing) {
                return ProjectionTarget{neighbor, cell, entryFace, query.block};
            }
            break;
        }
        auto const query = projection::queryProjection(cell);
        if (!query.block || !query.missing) continue;

        // The ghost itself is the target. Choose a real, non-air neighbor as
        // the placement support, preferring the one most facing the camera.
        uchar bestFace = std::numeric_limits<uchar>::max();
        float bestScore = std::numeric_limits<float>::lowest();
        for (uchar face = 0; face < 6; ++face) {
            BlockPos const at = cell.neighbor(face);
            if (region.getBlock(at).isAir()) continue;
            float const offX = static_cast<float>(at.x - cell.x);
            float const offY = static_cast<float>(at.y - cell.y);
            float const offZ = static_cast<float>(at.z - cell.z);
            float const score = -(offX * dir.x + offY * dir.y + offZ * dir.z);
            if (score > bestScore) {
                bestScore = score;
                bestFace = face;
            }
        }
        if (bestFace == std::numeric_limits<uchar>::max()) {
            // No real support nearby. The server places the block AT an air
            // mPos (instead of mPos.neighbor(mFace) for a solid mPos), so point
            // mPos at the ghost cell itself to make the block land there.
            return ProjectionTarget{
                cell,
                cell,
                Facing::getOpposite(entryFace),
                query.block
            };
        }

        return ProjectionTarget{cell, cell.neighbor(bestFace), Facing::getOpposite(bestFace), query.block};
    }
    return std::nullopt;
}

void placeBlock(LocalPlayer& player, ProjectionTarget const& target, int slot, ItemStack const& item) {
    auto& region = player.getDimensionBlockSource();

    // Server-authoritative placement: build an ItemUseInventoryTransaction and
    // deliver it through the client's packet sender (LoopbackPacketSender in
    // single-player, the network sender on a real server). GameMode::useItemOn
    // only predicts locally and Player::sendNetworkPacket does not reach the
    // integrated server, so neither persists.
    ItemUseInventoryTransaction transaction;
    transaction.mType = ComplexInventoryTransaction::Type::ItemUseTransaction;
    transaction.mActionType = ItemUseInventoryTransaction::ActionType::Place;
    transaction.mTriggerType = ItemUseInventoryTransaction::TriggerType::PlayerInput;
    // mPos is the block that was clicked on; the server places the new block
    // at mPos.neighbor(mFace). Setting it to the support block makes the
    // placement land exactly on the target ghost cell.
    transaction.mPos = target.at;
    transaction.mFace = target.face;
    // The item is always in the selected hotbar slot by the time we place:
    // hotbar items are selected directly, backpack items were swapped in.
    transaction.mSlot = slot;
    transaction.mFromPos = player.getPosition();
    // Click point: the center of the target cell. A point on the shared face
    // between the support and the cell sits exactly on the cell boundary, and
    // the server's cell rounding can then land one cell toward the player.
    transaction.mClickPos = Vec3{
        static_cast<float>(target.cell.x) + 0.5f,
        static_cast<float>(target.cell.y) + 0.5f,
        static_cast<float>(target.cell.z) + 0.5f
    };
    transaction.mClientPredictedResult = ItemUseInventoryTransaction::PredictedResult::Success;
    transaction.mClientCooldownState = ItemUseInventoryTransaction::ClientCooldownState::Off;
    transaction.setTargetBlock(region.getBlock(target.at));
    transaction.setSelectedItem(item);
    // The server's stack-net-id system expects the item descriptor to carry
    // the stack net id. With the flag off the writer omits the net id bytes,
    // the server misaligns the stream while reading and silently drops the
    // packet before the transaction is ever validated.
    transaction.mItem.get().setIncludeNetIds(true);

    InventoryTransactionPacket packet(
        std::make_unique<ItemUseInventoryTransaction>(transaction),
        true
    );
    player.getClientInstance().getPacketSender().sendToServer(packet);

    // logger().info(
    //     "[place] target=({},{},{}) mPos=({},{},{}) mFace={}",
    //     target.cell.x, target.cell.y, target.cell.z,
    //     transaction.mPos.get().x, transaction.mPos.get().y, transaction.mPos.get().z,
    //     static_cast<int>(transaction.mFace)
    // );
    gNextPlaceAt.store(GetTickCount64() + kMinSendIntervalMs, std::memory_order_release);
}

void tickEasyPlace() {
    auto client = ll::service::getClientInstance();
    if (!client) {
        updateAimedBlockEntityName(nullptr);
        return;
    }
    auto* player = client->getLocalPlayer();
    if (!player) {
        updateAimedBlockEntityName(nullptr);
        return;
    }
    // Only act during gameplay: menus, pause screens and the LHolo GUI itself
    // disable in-game input.
    if (!client->isInGameInputEnabled() || structure::isGuiVisible()) {
        updateAimedBlockEntityName(nullptr);
        return;
    }

    // Ray from the camera eye along the view direction against the projection.
    Vec3 const origin = player->getEyePos();
    Vec3 const rawDir = player->getViewVector(1.0f);
    float const length = std::sqrt(rawDir.x * rawDir.x + rawDir.y * rawDir.y + rawDir.z * rawDir.z);
    if (length <= 0.0f) {
        updateAimedBlockEntityName(nullptr);
        return;
    }
    Vec3 const dir{rawDir.x / length, rawDir.y / length, rawDir.z / length};

    auto target = findProjectionTarget(*player, origin, dir, player->getPickRange());
    // Keep the HUD informed even when easy-place is disabled.
    updateAimedBlockEntityName(target ? target->block : nullptr);

    if (!gEnabled.load(std::memory_order_acquire)) return;
    if (GetTickCount64() < gNextPlaceAt.load(std::memory_order_acquire)) return;
    if (!target) return;

    // Per-cell lock: the server needs a moment to apply the placement and the
    // correction scan needs a moment to mark the cell Correct. Skip re-sending
    // the same cell within that window; new cells place immediately.
    auto const now = GetTickCount64();
    if (gLastPlaceCell && *gLastPlaceCell == target->cell
        && now - gLastPlaceAt.load(std::memory_order_acquire) < kCellLockMs) {
        return;
    }

    ItemStack const want(*target->block, 1, nullptr);
    auto const found = findItemSlot(*player, want);
    if (found.slot < 0) return;

    if (found.slot >= kHotbarSlots) {
        // Back off a rejected swap so it never retries more often than
        // kSwapRetryMs.
        auto const now = GetTickCount64();
        if (now < gNextSwapAt.load(std::memory_order_acquire)) return;
        // The server only accepts placements from the selected hotbar slot.
        // Swap the backpack item into the currently selected slot through a
        // server-synced NormalTransaction. Do not place in the same tick: the
        // server's item-stack-net bookkeeping lags the legacy swap, so an
        // immediate placement can be rejected and then re-throttled by the cell
        // lock. The next tick finds the item in the hotbar and places via the
        // single-packet fast path.
        auto& inventory = player->getInventory();
        int const hotbarSlot = player->getSelectedItemSlot();
        auto const& toItem = inventory.getItem(hotbarSlot);
        sendInventorySwap(*player, found.slot, hotbarSlot, *found.item, toItem);
        gNextSwapAt.store(now + kSwapRetryMs, std::memory_order_release);
        return;
    }
    player->setSelectedSlot(found.slot);
    gLastPlaceCell = target->cell;
    gLastPlaceAt.store(now, std::memory_order_release);
    placeBlock(*player, *target, found.slot, *found.item);
}

LL_TYPE_INSTANCE_HOOK(
    LocalPlayerEasyPlaceHook,
    ll::memory::HookPriority::Normal,
    LocalPlayer,
    &LocalPlayer::$tickWorld,
    void,
    ::Tick const& currentTick
) {
    tickEasyPlace();
    origin(currentTick);
}

} // namespace

void setEnabled(bool enabled) {
    if (enabled) {
        logger().info("Easy-place enabled");
    } else {
        logger().info("Easy-place disabled");
    }
    gEnabled.store(enabled, std::memory_order_release);
}

bool isEnabled() {
    return gEnabled.load(std::memory_order_acquire);
}

std::string getAimedBlockEntityName() {
    std::lock_guard lock(gAimedNameMutex);
    return gAimedBlockEntityName;
}

bool installHook() {
    if (LocalPlayerEasyPlaceHook::hook() < 0) {
        logger().error("Failed to install easy-place tick hook");
        return false;
    }
    return true;
}

void uninstallHook() {
    LocalPlayerEasyPlaceHook::unhook();
}

} // namespace lholo::place
