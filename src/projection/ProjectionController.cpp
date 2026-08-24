// LHolo - Client-side projection renderer for Minecraft Bedrock Windows
// Copyright (C) 2026  MarmieQi

#include "projection/ProjectionController.h"

#include "projection/hooks/ProjectionGameHooks.h"
#include "projection/hooks/ProjectionRenderHooks.h"
#include "projection/runtime/ProjectionSession.h"
#include "projection/runtime/ProjectionLifecycle.h"

#include "overlay/BoundsWireframe.h"

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
    ProjectionSession::getInstance().withLockedState(
        [](ProjectionState&, overlay::BoundsWireframe& captureBounds) {
            captureBounds.clear();
        }
    );
}

void ProjectionController::disableProjection() {
    auto& session = ProjectionSession::getInstance();
    session.withLockedState(
        [](ProjectionState& state, overlay::BoundsWireframe&) {
            resetProjectionState(state);
        }
    );
    // A requested restore anchor belongs only to the projection being
    // activated. Explicit disable/close must not leak it into a later load.
    session.cancelAnchorRequest();
}

} // namespace lholo::projection::detail
