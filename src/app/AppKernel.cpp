// LHolo - Client-side projection renderer for Minecraft Bedrock Windows
// Copyright (C) 2026  MarmieQi

#include "app/AppKernel.h"

#include "input/MenuInputGuard.h"
#include "overlay/ImGuiOverlay.h"
#include "place/PlaceHelper.h"
#include "plugin/LHolo.h"
#include "projection/ProjectionController.h"
#include "structure/capture/StructureCapture.h"
#include "structure/StructureLoader.h"

#include "ll/api/mod/NativeMod.h"

namespace lholo::app {

AppKernel& AppKernel::getInstance() {
    static AppKernel instance;
    return instance;
}

bool AppKernel::load() {
    structure::loadSettings();
    return true;
}

bool AppKernel::enable() {
    auto& logger = LHolo::getInstance().getSelf().getLogger();

    if (!projection::detail::projectionController().installHooks()) {
        logger.error("Failed to install projection hooks");
        return false;
    }

    if (!place::installHook()) {
        logger.warn("Failed to install easy-place hooks");
    }

    if (!input::installMenuInputGuard()) {
        logger.warn("Failed to install menu block-destroy guard");
    }

    if (!overlay::ensureInstalled()) {
        logger.warn("GUI overlay hotkey hooks are not ready; lholo will retry initialization");
    }

    logger.info("LHolo enabled. Type lholo to open the projection menu.");
    return true;
}

bool AppKernel::disable() {
    auto& logger = LHolo::getInstance().getSelf().getLogger();

    structure::saveSettings();
    projection::detail::projectionController().disableProjection();
    input::uninstallMenuInputGuard();
    place::uninstallHook();
    overlay::shutdown();
    structure::capture::clear();
    structure::clear();
    projection::detail::projectionController().uninstallHooks();

    logger.info("LHolo disabled");
    return true;
}

} // namespace lholo::app
