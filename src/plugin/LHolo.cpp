// LHolo - Client-side projection renderer for Minecraft Bedrock Windows
// Copyright (C) 2026  MarmieQi
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#include "plugin/LHolo.h"

#include "place/PlaceHelper.h"
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

    if (!place::installHook()) {
        mSelf.getLogger().warn("Failed to install easy-place hooks");
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
    place::uninstallHook();
    overlay::shutdown();
    structure::clear();
    projection::uninstallHook();
    mSelf.getLogger().info("LHolo disabled");
    return true;
}

} // namespace lholo

LL_REGISTER_MOD(lholo::LHolo, lholo::LHolo::getInstance());
