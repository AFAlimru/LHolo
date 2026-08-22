// LHolo - Client-side projection renderer for Minecraft Bedrock Windows
// Copyright (C) 2026  MarmieQi

#include "projection/ProjectionController.h"

#include "projection/hooks/ProjectionGameHooks.h"
#include "projection/hooks/ProjectionRenderHooks.h"
#include "projection/runtime/ProjectionSession.h"

#include "overlay/BoundsWireframe.h"

#include <mutex>

namespace lholo::projection::detail {

ProjectionController& projectionController() {
    static ProjectionController instance;
    return instance;
}

bool ProjectionController::installHooks() {
    if (!installProjectionGameHooks()) return false;
    if (!installProjectionRenderHooks()) {
        uninstallProjectionGameHooks();
        return false;
    }
    return true;
}

void ProjectionController::uninstallHooks() {
    uninstallProjectionRenderHooks();
    uninstallProjectionGameHooks();
    std::lock_guard lock(projectionStateMutex());
    projectionCaptureBounds().clear();
}

void ProjectionController::disableProjection() {
    std::lock_guard lock(projectionStateMutex());
    clearProjectionStateLocked();
}

} // namespace lholo::projection::detail
