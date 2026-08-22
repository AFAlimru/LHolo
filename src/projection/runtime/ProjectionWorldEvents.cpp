// LHolo - Client-side projection renderer for Minecraft Bedrock Windows
// Copyright (C) 2026  MarmieQi
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#include "projection/runtime/ProjectionWorldEvents.h"

#include <algorithm>
#include <atomic>
#include <deque>
#include <mutex>
#include <tuple>

#include "mc/world/level/BlockSource.h"
#include "mc/world/level/BlockSourceListener.h"
#include "mc/world/level/Level.h"
#include "mc/world/level/LevelListener.h"
#include "mc/world/level/chunk/LevelChunk.h"

namespace lholo::projection::detail {
namespace {

std::mutex                gPendingEventsMutex;
std::deque<BlockPos>      gIncomingBlockChanges;
std::deque<SubChunkKey>   gIncomingLoadedSubChunks;
std::atomic<BlockSource*> gAttachedBlockSource{};
std::atomic<ChunkSource*> gAttachedChunkSource{};
std::atomic<Level*>       gAttachedLevel{};

class ProjectionBlockSourceListener final : public BlockSourceListener {
public:
    void onSourceDestroyed(BlockSource& source) override {
        auto* expected = &source;
        if (gAttachedBlockSource.compare_exchange_strong(
                expected,
                nullptr,
                std::memory_order_acq_rel
            )) {
            gAttachedChunkSource.store(nullptr, std::memory_order_release);
        }
    }

    void onBlockChanged(
        BlockSource&,
        BlockPos const&              pos,
        uint,
        Block const&,
        Block const&,
        int,
        ActorBlockSyncMessage const*,
        BlockChangedEventTarget,
        Actor*
    ) override {
        std::lock_guard lock(gPendingEventsMutex);
        gIncomingBlockChanges.push_back(pos);
    }
};

ProjectionBlockSourceListener gProjectionBlockSourceListener;

class ProjectionLevelListener final : public LevelListener {
public:
    void onSubChunkLoaded(
        ChunkSource& source,
        LevelChunk&  chunk,
        short        absoluteSubChunkIndex,
        bool
    ) override {
        if (&source != gAttachedChunkSource.load(std::memory_order_acquire)) return;
        auto const& chunkPosition = chunk.getPosition();
        std::lock_guard lock(gPendingEventsMutex);
        gIncomingLoadedSubChunks.emplace_back(
            chunkPosition.x,
            static_cast<int>(absoluteSubChunkIndex),
            chunkPosition.z
        );
    }

    void onLevelDestruction(std::string const&) override {
        gAttachedLevel.store(nullptr, std::memory_order_release);
        gAttachedChunkSource.store(nullptr, std::memory_order_release);
    }
};

ProjectionLevelListener gProjectionLevelListener;

} // namespace

void attachProjectionWorldEvents(Level& level, BlockSource& blockSource) {
    if (auto* attached = gAttachedBlockSource.load(std::memory_order_acquire);
        attached != &blockSource) {
        if (attached) {
            attached->removeListener(gProjectionBlockSourceListener);
        }
        blockSource.addListener(gProjectionBlockSourceListener);
        gAttachedBlockSource.store(&blockSource, std::memory_order_release);
    }
    gAttachedChunkSource.store(&blockSource.getChunkSource(), std::memory_order_release);
    if (auto* attached = gAttachedLevel.load(std::memory_order_acquire); attached != &level) {
        if (attached) {
            attached->removeListener(gProjectionLevelListener);
        }
        level.addListener(gProjectionLevelListener);
        gAttachedLevel.store(&level, std::memory_order_release);
    }
}

void detachProjectionWorldEvents() {
    if (auto* level = gAttachedLevel.exchange(nullptr, std::memory_order_acq_rel)) {
        level->removeListener(gProjectionLevelListener);
    }
    gAttachedChunkSource.store(nullptr, std::memory_order_release);
    if (auto* blockSource = gAttachedBlockSource.exchange(nullptr, std::memory_order_acq_rel)) {
        blockSource->removeListener(gProjectionBlockSourceListener);
    }
    std::lock_guard lock(gPendingEventsMutex);
    gIncomingBlockChanges.clear();
    gIncomingLoadedSubChunks.clear();
}

std::vector<BlockPos> takePendingBlockChanges(std::size_t limit) {
    std::vector<BlockPos> changes;
    {
        std::lock_guard lock(gPendingEventsMutex);
        auto const count = std::min(limit, gIncomingBlockChanges.size());
        changes.reserve(count);
        for (std::size_t index = 0; index < count; ++index) {
            changes.push_back(gIncomingBlockChanges.front());
            gIncomingBlockChanges.pop_front();
        }
    }
    std::sort(changes.begin(), changes.end(), [](BlockPos const& lhs, BlockPos const& rhs) {
        return std::tie(lhs.x, lhs.y, lhs.z) < std::tie(rhs.x, rhs.y, rhs.z);
    });
    changes.erase(
        std::unique(changes.begin(), changes.end(), [](BlockPos const& lhs, BlockPos const& rhs) {
            return lhs.x == rhs.x && lhs.y == rhs.y && lhs.z == rhs.z;
        }),
        changes.end()
    );
    return changes;
}

std::vector<SubChunkKey> takePendingLoadedSubChunks(std::size_t limit) {
    std::vector<SubChunkKey> loaded;
    {
        std::lock_guard lock(gPendingEventsMutex);
        auto const count = std::min(limit, gIncomingLoadedSubChunks.size());
        loaded.reserve(count);
        for (std::size_t index = 0; index < count; ++index) {
            loaded.push_back(gIncomingLoadedSubChunks.front());
            gIncomingLoadedSubChunks.pop_front();
        }
    }
    return loaded;
}

} // namespace lholo::projection::detail
