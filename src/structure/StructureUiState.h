// LHolo - Client-side projection renderer for Minecraft Bedrock Windows
// Copyright (C) 2026  MarmieQi
//
// UI/menu session state owned by the structure module. Accessors return the
// underlying atomic storage so existing hotkey/menu logic keeps its exact
// read-modify-write shapes; only the ownership location changes.

#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

#include "ui/LHoloMenu.h"

namespace lholo::structure::detail {

struct MaterialRequirement {
    std::string displayName;
    std::string typeName;
    std::uint64_t count{};
};

std::atomic_bool&       uiGuiVisible();
std::atomic_int&        uiOpeningInputBlockFrames();
std::atomic_uint64_t&   uiBlockGameInputUntil();

std::atomic_bool&       uiHudEnabled();
std::atomic_bool&       uiHudShowFileName();
std::atomic_bool&       uiHudShowLayer();
std::atomic_bool&       uiHudShowOverallProgress();
std::atomic_bool&       uiHudShowProgress();
std::atomic_bool&       uiHudShowWrongState();
std::atomic_bool&       uiHudShowWrongType();
std::atomic_bool&       uiHudShowBlockEntity();
std::atomic_int&        uiHudPosition();
std::atomic<float>&     uiUiScale();

std::atomic_uint&       uiGuiHotkey();
std::atomic_uint&       uiGuiHotkeyModifiers();
std::atomic_bool&       uiCapturingGuiHotkey();
std::atomic_bool&       uiGuiHotkeyHeld();
std::atomic_uint&       uiLayerIncreaseHotkey();
std::atomic_uint&       uiLayerDecreaseHotkey();
std::atomic_uint&       uiLayerIncreaseHotkeyModifiers();
std::atomic_uint&       uiLayerDecreaseHotkeyModifiers();
std::atomic_bool&       uiCapturingLayerIncreaseHotkey();
std::atomic_bool&       uiCapturingLayerDecreaseHotkey();
std::atomic_bool&       uiLayerIncreaseHotkeyHeld();
std::atomic_bool&       uiLayerDecreaseHotkeyHeld();

std::array<std::atomic_uint, 6>&   uiMoveHotkeys();
std::array<std::atomic_uint, 6>&   uiMoveHotkeyModifiers();
std::array<std::atomic_bool, 6>&   uiCapturingMoveHotkey();
std::array<std::atomic_bool, 6>&   uiMoveHotkeyHeld();
std::atomic_bool&                  uiControlHeld();
std::atomic_bool&                  uiAltHeld();
std::atomic_bool&                  uiShiftHeld();
std::array<std::atomic_uint64_t, 256>& uiConsumeKeyReleaseUntil();

std::atomic_int&        uiPendingOffsetX();
std::atomic_int&        uiPendingOffsetY();
std::atomic_int&        uiPendingOffsetZ();
std::atomic_int&        uiPendingLayerDelta();
std::atomic_bool&       uiPendingSettingsSave();
std::atomic_uint64_t&   uiIgnoreHotkeyUntil();

std::mutex&                        uiMaterialMutex();
std::atomic_bool&                  uiMaterialListRequested();
std::vector<MaterialRequirement>&  uiMaterialRequirements();

std::array<char, 2048>& uiPathBuffer();
bool&                   uiPathInitialized();

lholo::ui::MenuPage&    uiActivePage();

} // namespace lholo::structure::detail
