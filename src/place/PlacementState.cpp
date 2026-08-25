// LHolo - Client-side projection renderer for Minecraft Bedrock Windows
// Copyright (C) 2026  MarmieQi

#include "place/PlacementState.h"

#include <iterator>
#include <utility>

namespace lholo::place::detail {

PlacementState& PlacementState::getInstance() {
    static PlacementState instance;
    return instance;
}

bool PlacementState::enabled() const { return mEnabled.load(std::memory_order_acquire); }
void PlacementState::setEnabled(bool enabled) { mEnabled.store(enabled, std::memory_order_release); }

bool PlacementState::rangeEnabled() const { return mRangeEnabled.load(std::memory_order_acquire); }
void PlacementState::setRangeEnabled(bool enabled) { mRangeEnabled.store(enabled, std::memory_order_release); }

bool PlacementState::manualMode() const { return mManualMode.load(std::memory_order_acquire); }
void PlacementState::setManualMode(bool manual) { mManualMode.store(manual, std::memory_order_release); }

int PlacementState::radius() const { return mRadius.load(std::memory_order_relaxed); }
void PlacementState::setRadius(int radius) { mRadius.store(radius, std::memory_order_release); }

int PlacementState::autoPlacementBreakCooldownSeconds() const {
    return mAutoPlacementBreakCooldownSeconds.load(std::memory_order_relaxed);
}
void PlacementState::setAutoPlacementBreakCooldownSeconds(int seconds) {
    mAutoPlacementBreakCooldownSeconds.store(seconds, std::memory_order_release);
}

bool PlacementState::manualHeld() const { return mManualHeld.load(std::memory_order_acquire); }
void PlacementState::setManualHeld(bool held) { mManualHeld.store(held, std::memory_order_release); }

bool PlacementState::manualPlaceRequested() const {
    return mManualPlaceRequested.load(std::memory_order_acquire);
}
void PlacementState::setManualPlaceRequested(bool requested) {
    mManualPlaceRequested.store(requested, std::memory_order_release);
}

std::uint64_t PlacementState::manualPressAt() const {
    return mManualPressAt.load(std::memory_order_acquire);
}
void PlacementState::setManualPressAt(std::uint64_t time) {
    mManualPressAt.store(time, std::memory_order_release);
}

std::uint64_t PlacementState::lastManualPlaceAt() const {
    return mLastManualPlaceAt.load(std::memory_order_acquire);
}
void PlacementState::setLastManualPlaceAt(std::uint64_t time) {
    mLastManualPlaceAt.store(time, std::memory_order_release);
}

std::uint64_t PlacementState::nextPlaceAt() const {
    return mNextPlaceAt.load(std::memory_order_acquire);
}
void PlacementState::setNextPlaceAt(std::uint64_t time) {
    mNextPlaceAt.store(time, std::memory_order_release);
}

std::uint64_t PlacementState::nextSwapAt() const {
    return mNextSwapAt.load(std::memory_order_acquire);
}
void PlacementState::setNextSwapAt(std::uint64_t time) {
    mNextSwapAt.store(time, std::memory_order_release);
}

bool PlacementState::recentPlacementActive(std::int64_t cell, std::uint64_t now) const {
    std::lock_guard lock(mRecentMutex);
    auto const found = mRecentPlacements.find(cell);
    return found != mRecentPlacements.end() && now < found->second;
}

void PlacementState::recordRecentPlacement(
    std::int64_t  cell,
    std::uint64_t now,
    std::uint64_t expiresAt
) {
    std::lock_guard lock(mRecentMutex);
    if (mRecentPlacements.size() > 256) {
        for (auto it = mRecentPlacements.begin(); it != mRecentPlacements.end();) {
            it = now >= it->second ? mRecentPlacements.erase(it) : std::next(it);
        }
    }
    mRecentPlacements[cell] = expiresAt;
}

bool PlacementState::autoPlacementSuppressionsActive(std::uint64_t now) {
    auto const nextExpiry = mNextAutoPlacementSuppressionExpiry.load(std::memory_order_acquire);
    if (nextExpiry == 0) return false;
    // The earliest active expiry provides the empty/common fast path and avoids
    // scanning the sparse map every tick while every entry is still active.
    if (now < nextExpiry) return true;

    std::lock_guard lock(mRecentMutex);
    std::uint64_t earliest{};
    for (auto it = mAutoPlacementSuppressions.begin(); it != mAutoPlacementSuppressions.end();) {
        if (now >= it->second) {
            it = mAutoPlacementSuppressions.erase(it);
            continue;
        }
        if (earliest == 0 || it->second < earliest) earliest = it->second;
        ++it;
    }
    mNextAutoPlacementSuppressionExpiry.store(earliest, std::memory_order_release);
    return earliest != 0;
}

bool PlacementState::autoPlacementSuppressed(std::int64_t cell, std::uint64_t now) const {
    std::lock_guard lock(mRecentMutex);
    auto const found = mAutoPlacementSuppressions.find(cell);
    return found != mAutoPlacementSuppressions.end() && now < found->second;
}

void PlacementState::suppressAutoPlacement(std::int64_t cell, std::uint64_t expiresAt) {
    std::lock_guard lock(mRecentMutex);
    mAutoPlacementSuppressions[cell] = expiresAt;
    auto const nextExpiry = mNextAutoPlacementSuppressionExpiry.load(std::memory_order_relaxed);
    if (nextExpiry == 0 || expiresAt < nextExpiry) {
        mNextAutoPlacementSuppressionExpiry.store(expiresAt, std::memory_order_release);
    }
}

bool PlacementState::failedPlanCached(FailedPlanKey const& key, std::uint64_t now) const {
    auto const found = mFailedRangePlans.find(key);
    return found != mFailedRangePlans.end() && now < found->second;
}

void PlacementState::cacheFailedPlan(
    FailedPlanKey const& key,
    std::uint64_t        now,
    std::uint64_t        expiresAt
) {
    if (mFailedRangePlans.size() > 256) {
        for (auto it = mFailedRangePlans.begin(); it != mFailedRangePlans.end();) {
            it = now >= it->second ? mFailedRangePlans.erase(it) : std::next(it);
        }
    }
    mFailedRangePlans[key] = expiresAt;
}

std::string PlacementState::aimedProjectedBlockName() const {
    std::lock_guard lock(mAimedProjectedBlockNameMutex);
    return mAimedProjectedBlockName;
}

void PlacementState::setAimedProjectedBlockName(std::string name) {
    std::lock_guard lock(mAimedProjectedBlockNameMutex);
    mAimedProjectedBlockName = std::move(name);
}

} // namespace lholo::place::detail
