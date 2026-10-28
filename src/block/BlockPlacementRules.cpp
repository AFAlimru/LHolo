// LHolo - Client-side projection renderer for Minecraft Bedrock Windows
// Copyright (C) 2026  MarmieQi

#include "block/BlockPlacementRules.h"

#include "mc/world/item/ItemInstance.h"
#include "mc/world/item/SaveContext.h"
#include "mc/deps/nbt/CompoundTag.h"
#include "mc/world/item/ItemStack.h"
#include "mc/world/level/block/Block.h"
#include "mc/world/level/block/BlockType.h"

#include <cstddef>

namespace lholo::block {

ItemStack makePlacementItem(Block const& block) {
    std::string const blockName{placeableBaseName(block.getTypeName())};
    auto const itemName = placementItemName(blockName);
    std::string_view const name = itemName.empty() ? std::string_view{blockName} : itemName;
    // 26.40 removed the ItemStack(name, count, aux, tag) constructor; the
    // remaining route is default construction followed by reinit().
    ItemStack item;
    item.reinit(name, 1, 0);
    return item;
}

std::string stripMinecraftFormatting(std::string_view text) {
    std::string out;
    out.reserve(text.size());
    for (std::size_t index = 0; index < text.size();) {
        if (index + 1 < text.size()
            && static_cast<unsigned char>(text[index]) == 0xC2
            && static_cast<unsigned char>(text[index + 1]) == 0xA7) {
            index += 2;
            if (index < text.size()) ++index;
            continue;
        }
        out.push_back(text[index++]);
    }
    return out;
}

PlacementItem resolvePlacementItem(Block const& block, BlockActor const* blockActor) {
    // 1. Bedrock native polymorphic resolution: BlockType::asItemInstance(block, blockActor)
    // Mojang implements this virtual method on all BlockType subclasses (crops, signs, redstone, etc.)
    try {
        ItemInstance const pickItem = block.getBlockType().asItemInstance(block, blockActor);
        if (!pickItem.isNull()) {
            int const size = static_cast<int>(pickItem.getMaxStackSize());
            return {
                stripMinecraftFormatting(pickItem.getHoverName()),
                pickItem.getTypeName(),
                size > 0 ? size : 64,
                true,
                std::make_shared<ItemStack>(ItemStack::fromTag(*pickItem.save(SaveContext{}))),
            };
        }
    } catch (...) {
    }

    // 2. Try makePlacementItem
    ItemStack const placeItem = makePlacementItem(block);
    if (!placeItem.isNull()) {
        int const size = static_cast<int>(placeItem.getMaxStackSize());
        return {
            stripMinecraftFormatting(placeItem.getHoverName()),
            placeItem.getTypeName(),
            size > 0 ? size : 64,
            true,
            std::make_shared<ItemStack>(placeItem),
        };
    }

    // 3. Native Block -> ItemInstance conversion
    ItemInstance const gameItem{block};
    if (!gameItem.isNull()) {
        int const size = static_cast<int>(gameItem.getMaxStackSize());
        return {
            stripMinecraftFormatting(gameItem.getHoverName()),
            gameItem.getTypeName(),
            size > 0 ? size : 64,
            true,
            std::make_shared<ItemStack>(ItemStack::fromTag(*gameItem.save(SaveContext{}))),
        };
    }

    return {};
}

} // namespace lholo::block
