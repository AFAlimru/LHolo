// LHolo - Client-side projection renderer for Minecraft Bedrock Windows
// Copyright (C) 2026  MarmieQi
//
// Thread-local virtual block view used only while LHolo tessellates or renders
// projected block entities. Outside an explicit scope every query is empty.

#pragma once

#include "projection/ProjectionInternalTypes.h"

class Block;
class BlockActor;
class BlockPos;

namespace lholo::projection::detail {

class ScopedTessellationBlocks {
public:
    explicit ScopedTessellationBlocks(
        ExpectedBlockMap const&      blocks,
        ExpectedBlockActorMap const& blockActors
    );
    ~ScopedTessellationBlocks();

    ScopedTessellationBlocks(ScopedTessellationBlocks const&) = delete;
    ScopedTessellationBlocks& operator=(ScopedTessellationBlocks const&) = delete;

private:
    ExpectedBlockMap const*      mPreviousBlocks{};
    ExpectedBlockActorMap const* mPreviousBlockActors{};
};

Block const*      findTessellationBlock(BlockPos const& position);
BlockActor const* findTessellationBlockActor(BlockPos const& position);

} // namespace lholo::projection::detail
