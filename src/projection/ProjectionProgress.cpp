// LHolo - Client-side projection renderer for Minecraft Bedrock Windows
// Copyright (C) 2026  MarmieQi

#include "projection/ProjectionProgress.h"

#include <atomic>

namespace lholo::projection::detail {
namespace {

std::atomic_uint64_t sPlaced{0};
std::atomic_uint64_t sTotal{0};
std::atomic_uint64_t sVisiblePlaced{0};
std::atomic_uint64_t sVisibleTotal{0};
std::atomic_uint64_t sWrongType{0};
std::atomic_uint64_t sWrongState{0};

} // namespace

void resetPublishedBuildProgress() {
    sPlaced.store(0, std::memory_order_relaxed);
    sVisiblePlaced.store(0, std::memory_order_relaxed);
    sVisibleTotal.store(0, std::memory_order_relaxed);
    sWrongType.store(0, std::memory_order_relaxed);
    sWrongState.store(0, std::memory_order_relaxed);
    sTotal.store(0, std::memory_order_release);
}

void initializePublishedBuildProgress(std::uint64_t total) {
    sPlaced.store(0, std::memory_order_relaxed);
    sVisiblePlaced.store(0, std::memory_order_relaxed);
    sVisibleTotal.store(total, std::memory_order_relaxed);
    sWrongType.store(0, std::memory_order_relaxed);
    sWrongState.store(0, std::memory_order_relaxed);
    sTotal.store(total, std::memory_order_release);
}

void resetPublishedBuildProgressCounts() {
    sPlaced.store(0, std::memory_order_release);
    sVisiblePlaced.store(0, std::memory_order_release);
    sWrongType.store(0, std::memory_order_release);
    sWrongState.store(0, std::memory_order_release);
}

void publishPlacedProgress(std::uint64_t placed) {
    sPlaced.store(placed, std::memory_order_release);
}

void publishVisiblePlacedProgress(std::uint64_t placed) {
    sVisiblePlaced.store(placed, std::memory_order_release);
}

void publishVisibleProgress(std::uint64_t placed, std::uint64_t total) {
    sVisiblePlaced.store(placed, std::memory_order_release);
    sVisibleTotal.store(total, std::memory_order_release);
}

void publishErrorProgress(std::uint64_t wrongType, std::uint64_t wrongState) {
    sWrongType.store(wrongType, std::memory_order_release);
    sWrongState.store(wrongState, std::memory_order_release);
}

BuildProgress getPublishedBuildProgress() {
    BuildProgress result;
    result.total = sTotal.load(std::memory_order_acquire);
    result.placed = sPlaced.load(std::memory_order_acquire);
    result.visibleTotal = sVisibleTotal.load(std::memory_order_acquire);
    result.visiblePlaced = sVisiblePlaced.load(std::memory_order_acquire);
    result.wrongType = sWrongType.load(std::memory_order_acquire);
    result.wrongState = sWrongState.load(std::memory_order_acquire);
    if (result.placed > result.total) result.placed = result.total;
    if (result.visiblePlaced > result.visibleTotal) result.visiblePlaced = result.visibleTotal;
    if (result.wrongType > result.total) result.wrongType = result.total;
    if (result.wrongState > result.total) result.wrongState = result.total;
    return result;
}

} // namespace lholo::projection::detail
