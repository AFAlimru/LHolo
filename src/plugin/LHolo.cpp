#include "plugin/LHolo.h"

#include "projection/Projection.h"
#include "overlay/ImGuiOverlay.h"
#include "structure/StructureLoader.h"

#include "ll/api/mod/NativeMod.h"
#include "ll/api/mod/RegisterHelper.h"

namespace lholo {

LHolo& LHolo::getInstance() {
    static LHolo instance;
    return instance;
}

LHolo::LHolo() : mSelf(*ll::mod::NativeMod::current()) {}

ll::mod::NativeMod& LHolo::getSelf() const { return mSelf; }

bool LHolo::load() {
    structure::loadSettings();
    return true;
}

bool LHolo::enable() {
    if (!projection::installHook()) {
        mSelf.getLogger().error("Failed to install projection hooks");
        return false;
    }

    if (!overlay::ensureInstalled()) {
        mSelf.getLogger().warn("GUI overlay hotkey hooks are not ready; lholo will retry initialization");
    }

    mSelf.getLogger().info("LHolo enabled. Type lholo to open the projection menu.");
    return true;
}

bool LHolo::disable() {
    structure::saveSettings();
    projection::disable();
    overlay::shutdown();
    structure::clear();
    projection::uninstallHook();
    mSelf.getLogger().info("LHolo disabled");
    return true;
}

} // namespace lholo

LL_REGISTER_MOD(lholo::LHolo, lholo::LHolo::getInstance());
