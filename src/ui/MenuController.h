// LHolo - Client-side projection renderer for Minecraft Bedrock Windows
// Copyright (C) 2026  MarmieQi
//
// Menu model build/apply. The controller maps session/UI state into the pure
// menu model and writes applied values back through the same accessors.

#pragma once

#include <atomic>

#include "ui/LHoloMenu.h"

namespace lholo::ui {

struct MenuModel;

struct HotkeyBinding {
    std::atomic_uint* key{};
    std::atomic_uint* modifiers{};
    std::atomic_bool* capturing{};
};

HotkeyBinding hotkeyBinding(HotkeyId id);

MenuModel buildStructureMenuModel(float effectiveUiScale);

void applyStructureMenuModel(MenuModel const& model, float effectiveUiScale);

} // namespace lholo::ui
