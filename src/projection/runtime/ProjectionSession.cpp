// LHolo - Client-side projection renderer for Minecraft Bedrock Windows
// Copyright (C) 2026  MarmieQi
//
// Session-wide mutable state storage and accessors. This is the single owner
// of the projection globals; frame orchestration and the facade both consume
// the ProjectionSession contract instead of touching file-local globals.

#include "projection/runtime/ProjectionSession.h"

#include "projection/core/ProjectionState.h"
#include "projection/runtime/ProjectionLifecycle.h"

#include "overlay/BoundsWireframe.h"

#include <algorithm>
#include <atomic>
#include <mutex>

namespace lholo::projection::detail {
namespace {

std::atomic<float> gOpacity{1.0f};
std::atomic<float> gCorrectionFillOpacity{0.15f};
std::atomic<float> gCorrectionOutlineOpacity{1.0f};
std::atomic_bool   gStructureBoundsEnabled{true};
std::atomic_bool   gPendingStructureAnchor{false};
std::atomic_int    gPendingStructureAnchorX{0};
std::atomic_int    gPendingStructureAnchorY{0};
std::atomic_int    gPendingStructureAnchorZ{0};
std::mutex         gStateMutex;
ProjectionState    gState;
overlay::BoundsWireframe gCaptureBounds;

} // namespace

std::mutex& projectionStateMutex() {
    return gStateMutex;
}

ProjectionState& projectionState() {
    return gState;
}

void clearProjectionStateLocked() {
    resetProjectionState(gState);
}

overlay::BoundsWireframe& projectionCaptureBounds() {
    return gCaptureBounds;
}

float projectionOpacity() {
    return gOpacity.load(std::memory_order_relaxed);
}

void setProjectionOpacity(float opacity) {
    gOpacity.store(std::clamp(opacity, 0.0f, 1.0f), std::memory_order_relaxed);
}

float projectionCorrectionFillOpacity() {
    return gCorrectionFillOpacity.load(std::memory_order_relaxed);
}

void setProjectionCorrectionFillOpacity(float opacity) {
    gCorrectionFillOpacity.store(std::clamp(opacity, 0.0f, 1.0f), std::memory_order_relaxed);
}

float projectionCorrectionOutlineOpacity() {
    return gCorrectionOutlineOpacity.load(std::memory_order_relaxed);
}

void setProjectionCorrectionOutlineOpacity(float opacity) {
    gCorrectionOutlineOpacity.store(std::clamp(opacity, 0.0f, 1.0f), std::memory_order_relaxed);
}

bool projectionStructureBoundsEnabled() {
    return gStructureBoundsEnabled.load(std::memory_order_relaxed);
}

void setProjectionStructureBoundsEnabled(bool enabled) {
    gStructureBoundsEnabled.store(enabled, std::memory_order_relaxed);
}

bool consumeProjectionAnchor(int& x, int& y, int& z) {
    if (!gPendingStructureAnchor.exchange(false, std::memory_order_acq_rel)) return false;
    x = gPendingStructureAnchorX.load(std::memory_order_relaxed);
    y = gPendingStructureAnchorY.load(std::memory_order_relaxed);
    z = gPendingStructureAnchorZ.load(std::memory_order_relaxed);
    return true;
}

void requestProjectionAnchor(int x, int y, int z) {
    gPendingStructureAnchorX.store(x, std::memory_order_relaxed);
    gPendingStructureAnchorY.store(y, std::memory_order_relaxed);
    gPendingStructureAnchorZ.store(z, std::memory_order_relaxed);
    gPendingStructureAnchor.store(true, std::memory_order_release);
}

} // namespace lholo::projection::detail
