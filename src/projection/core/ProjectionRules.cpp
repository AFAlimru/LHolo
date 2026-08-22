// LHolo - Client-side projection renderer for Minecraft Bedrock Windows
// Copyright (C) 2026  MarmieQi
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#include "projection/core/ProjectionRules.h"

#include <string>
#include <type_traits>

#include "mc/deps/nbt/CompoundTag.h"
#include "mc/deps/nbt/IntTag.h"
#include "mc/deps/nbt/StringTag.h"
#include "mc/deps/nbt/Tag.h"
#include "mc/world/Facing.h"
#include "mc/world/level/block/Block.h"
#include "mc/world/level/block/VanillaStates.h"
#include "mc/world/level/levelgen/structure/LegacyStructureSettings.h"
#include "mc/world/level/levelgen/structure/LegacyStructureTemplate.h"

namespace lholo::projection::detail {

RenderBucket renderBucketFor(BlockRenderLayer layer) {
    switch (layer) {
    case BlockRenderLayer::RenderlayerBlend:
    case BlockRenderLayer::RenderlayerBlendToOpaque:
        return RenderBucket::Blend;
    case BlockRenderLayer::RenderlayerOpaque:
    case BlockRenderLayer::RenderlayerSeasonsOpaque:
    case BlockRenderLayer::RenderlayerShiftOpaqueInternalOnly:
        return RenderBucket::Opaque;
    case BlockRenderLayer::RenderlayerAlphatestSingleSide:
    case BlockRenderLayer::RenderlayerAlphatestSingleSideToOpaque:
    case BlockRenderLayer::RenderlayerShiftAlphatestSingleSideInternalOnly:
    case BlockRenderLayer::RenderlayerShiftAlphatestSingleSideToOpaqueInternalOnly:
        return RenderBucket::AlphaOneSided;
    default:
        return RenderBucket::Alpha;
    }
}

Mirror getProjectionMirror(int mirrorMode) {
    switch (mirrorMode) {
    // LHolo's UI names the coordinate being flipped. Bedrock names Mirror by
    // the axis kept fixed: Mirror::Z flips X, while Mirror::X flips Z.
    case 1: return Mirror::Z;
    case 2: return Mirror::X;
    default: return Mirror::None;
    }
}

Rotation getProjectionRotation(int quarterTurns) {
    switch (quarterTurns & 3) {
    case 1: return Rotation::Clockwise90;
    case 2: return Rotation::Clockwise180;
    case 3: return Rotation::CounterClockwise90;
    default: return Rotation::None;
    }
}

Block const* transformExpectedBlock(
    Block const*                   block,
    LegacyStructureSettings const& settings,
    bool                           identityTransform
) {
    if (!block) return nullptr;
    if (identityTransform) return block;
    // Use the same generic permutation mapping as vanilla structure placement.
    // The engine owns the complete set of transformable states, so new or
    // uncommon directional blocks require no LHolo-side block/state table.
    return &LegacyStructureTemplate::_mapToData(*block, settings);
}

bool projectionStatesMatch(Block const& expected, Block const& actual) {
    if (expected == actual) return true;
    if (expected.getTypeName() != actual.getTypeName()) return false;

    auto stateMatches = [&](auto const& state) {
        using StateValue = typename std::remove_cvref_t<decltype(state)>::Type;
        auto const expectedValue = expected.getState<StateValue>(state);
        auto const actualValue = actual.getState<StateValue>(state);
        return expectedValue && actualValue && *expectedValue == *actualValue;
    };

    // A real door stores its placement state across two blocks: the lower half
    // owns direction/open, the upper half owns hinge. Bedrock may normalize the
    // duplicated fields differently after a structure load, so the complete
    // serialization hash can differ even for a correctly placed door. Only real
    // doors carry upper_block_bit; trapdoor names also end with "door", so the
    // presence of that state is the reliable discriminator.
    auto const expectedUpper = expected.getState<bool>(VanillaStates::UpperBlockBit());
    if (expectedUpper) {
        auto const actualUpper = actual.getState<bool>(VanillaStates::UpperBlockBit());
        if (!actualUpper || *actualUpper != *expectedUpper) return false;
        return *expectedUpper
            ? stateMatches(VanillaStates::DoorHingeBit())
            : stateMatches(VanillaStates::Direction())
                && stateMatches(VanillaStates::OpenBit());
    }

    // Trapdoors are single blocks: compare their own placement states instead
    // of treating them like a two-block door.
    auto const expectedOpen = expected.getState<bool>(VanillaStates::OpenBit());
    if (expectedOpen) {
        auto const actualOpen = actual.getState<bool>(VanillaStates::OpenBit());
        return actualOpen && *actualOpen == *expectedOpen
            && stateMatches(VanillaStates::Direction())
            && stateMatches(VanillaStates::UpsideDownBit());
    }

    return false;
}

// Front face (Facing 0-5) for a block-entity placeholder, read from whichever
// facing state the block actually carries. Chests and similar block entities
// moved from the integer facing_direction to the string
// minecraft:cardinal_direction, so both are handled. Returns -1 when the block
// has no horizontal facing.
int blockFrontFace(Block const& block) {
    for (auto const& [key, value] : block.getSerializationId()) {
        if (key != "states") continue;
        if (!value.hold<CompoundTag>()) break;
        for (auto const& [stateKey, stateValue] : value.get<CompoundTag>()) {
            if (stateKey == "facing_direction" && stateValue.getId() == Tag::Type::Int) {
                return stateValue.get<IntTag>().data;
            }
            if (stateKey == "minecraft:cardinal_direction" && stateValue.getId() == Tag::Type::String) {
                std::string const& facing = static_cast<std::string const&>(stateValue.get<StringTag>());
                if (facing == "north") return static_cast<int>(Facing::Name::North);
                if (facing == "south") return static_cast<int>(Facing::Name::South);
                if (facing == "west")  return static_cast<int>(Facing::Name::West);
                if (facing == "east")  return static_cast<int>(Facing::Name::East);
            }
        }
        break;
    }
    return -1;
}

BlockPos transformStructurePosition(
    structure::LoadedStructure::RenderBlock const& entry,
    structure::LoadedStructure const&              loaded,
    int                                             mirrorMode,
    int                                             rotation
) {
    int x = entry.x;
    int z = entry.z;
    if (mirrorMode == 1) x = loaded.sizeX - 1 - x;
    if (mirrorMode == 2) z = loaded.sizeZ - 1 - z;
    switch (rotation) {
    case 1: return BlockPos{loaded.sizeZ - 1 - z, entry.y, x};
    case 2: return BlockPos{loaded.sizeX - 1 - x, entry.y, loaded.sizeZ - 1 - z};
    case 3: return BlockPos{z, entry.y, loaded.sizeX - 1 - x};
    default: return BlockPos{x, entry.y, z};
    }
}

bool isLayerVisible(int layer, int layerDisplayMode, int displayLayer) {
    switch (layerDisplayMode) {
    case 1: return layer == displayLayer;
    case 2: return layer <= displayLayer;
    case 3: return layer >= displayLayer;
    default: return true;
    }
}

} // namespace lholo::projection::detail
