// LHolo - Client-side projection renderer for Minecraft Bedrock Windows
// Copyright (C) 2026  MarmieQi
//
// Placement planning and execution. The executor owns the easy/range tick
// logic; game hooks stay in PlaceHelper and only call the public entry points.

#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "mc/deps/core/math/Vec3.h"

class LocalPlayer;
class Block;

namespace lholo::place::detail {

struct PlacementContext {
    Vec3  eye;
    float reachSquared;
    int   eyeX;
    int   eyeY;
    int   eyeZ;
    int   viewX;
    int   viewY;
    int   viewZ;
};

void tickEasyPlace();

// Fresh, synchronous check of whether the crosshair is over a placeable missing
// projection cell (raytrace at call time). The manual-mode build hooks use it to
// decide whether to place, or to let vanilla interact / show the blocked hint.
bool manualTargetUnderCrosshair();

// How many of each block the player currently holds, matched item-for-item to
// what LHolo would place. Same order/size as `blockNames`.
std::vector<int> availableCounts(std::vector<std::string> const& blockNames);

// Max stack size of the item `block` resolves to (64 normally, 16 for signs
// etc., 1 for filled buckets); 64 when it maps to no real item. Touches the
// item registry, so call it on the game tick thread only.
int maxStackForBlock(Block const& block);

// Map a runtime lit/powered/heat variant name to the base block that is actually
// placed and held as an item (e.g. lit_redstone_lamp -> redstone_lamp,
// powered_repeater -> unpowered_repeater). Names with no variant are unchanged.
// Used so the material list counts the two states as one material.
std::string_view baseMaterialName(std::string_view name);

// Localized display name of the item the block is gathered/placed as (e.g. an
// unpowered_repeater block -> "红石中继器", not "未启动的中继器"). Empty when the
// block maps to no real item. Touches the item registry — tick thread only.
std::string materialDisplayName(Block const& block);

// The actual item a block resolves to for the material list, via the game's own
// block->item mapping. This collapses wall/standing/hanging signs, coral fans,
// powered/unpowered redstone parts, lit/unlit variants etc. onto one real item —
// `aux` groups and matches against the inventory, `name` is the localized item
// name (formatting codes stripped), `stackSize` its max stack. Touches the item
// registry, so call on the tick thread only.
struct MaterialItem {
    std::string name;    // localized item name, formatting codes stripped
    std::string itemId;  // item type name — groups the list and matches inventory
    int         stackSize{64};
    bool        valid{false};
};
MaterialItem resolveMaterialItem(Block const& block);

void tickRangePlace(LocalPlayer& player, PlacementContext const& context);

} // namespace lholo::place::detail
