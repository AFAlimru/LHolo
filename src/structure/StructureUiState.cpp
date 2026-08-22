// LHolo - Client-side projection renderer for Minecraft Bedrock Windows
// Copyright (C) 2026  MarmieQi

#include "structure/StructureUiState.h"

#include "ui/HotkeyFormat.h"

#include <Windows.h>

namespace lholo::structure::detail {
namespace {

std::atomic_bool                 gGuiVisible{false};
std::atomic_int                  gOpeningInputBlockFrames{0};
std::atomic_uint64_t             gBlockGameInputUntil{};
std::atomic_bool                 gHudEnabled{true};
std::atomic_bool                 gHudShowFileName{true};
std::atomic_bool                 gHudShowLayer{true};
std::atomic_bool                 gHudShowOverallProgress{false};
std::atomic_bool                 gHudShowProgress{true};
std::atomic_bool                 gHudShowWrongState{true};
std::atomic_bool                 gHudShowWrongType{true};
std::atomic_bool                 gHudShowBlockEntity{true};
std::atomic_int                  gHudPosition{1};
std::atomic<float>              gUiScale{2.0f};
std::atomic_uint                 gGuiHotkey{'M'};
std::atomic_uint                 gGuiHotkeyModifiers{lholo::ui::kHotkeyModifierAlt};
std::atomic_bool                 gCapturingGuiHotkey{false};
std::atomic_bool                 gGuiHotkeyHeld{false};
std::atomic_uint                 gLayerIncreaseHotkey{VK_UP};
std::atomic_uint                 gLayerDecreaseHotkey{VK_DOWN};
std::atomic_uint                 gLayerIncreaseHotkeyModifiers{lholo::ui::kHotkeyModifierAlt};
std::atomic_uint                 gLayerDecreaseHotkeyModifiers{lholo::ui::kHotkeyModifierAlt};
std::atomic_bool                 gCapturingLayerIncreaseHotkey{false};
std::atomic_bool                 gCapturingLayerDecreaseHotkey{false};
std::atomic_bool                 gLayerIncreaseHotkeyHeld{false};
std::atomic_bool                 gLayerDecreaseHotkeyHeld{false};
std::array<std::atomic_uint, 6>  gMoveHotkeys{VK_LEFT, VK_RIGHT, VK_UP, VK_DOWN, VK_UP, VK_DOWN};
std::array<std::atomic_uint, 6>  gMoveHotkeyModifiers{
    lholo::ui::kHotkeyModifierControl,
    lholo::ui::kHotkeyModifierControl,
    lholo::ui::kHotkeyModifierControl,
    lholo::ui::kHotkeyModifierControl,
    lholo::ui::kHotkeyModifierShift,
    lholo::ui::kHotkeyModifierShift
};
std::array<std::atomic_bool, 6>  gCapturingMoveHotkey{};
std::array<std::atomic_bool, 6>  gMoveHotkeyHeld{};
std::atomic_bool                 gControlHeld{false};
std::atomic_bool                 gAltHeld{false};
std::atomic_bool                 gShiftHeld{false};
std::array<std::atomic_uint64_t, 256> gConsumeKeyReleaseUntil{};
std::atomic_int                  gPendingOffsetX{0};
std::atomic_int                  gPendingOffsetY{0};
std::atomic_int                  gPendingOffsetZ{0};
std::atomic_int                  gPendingLayerDelta{0};
std::atomic_bool                 gPendingSettingsSave{false};
std::atomic_uint64_t             gIgnoreHotkeyUntil{0};
std::mutex                       gMaterialMutex;
std::atomic_bool                 gMaterialListRequested{false};
std::vector<MaterialRequirement> gMaterialRequirements;
std::array<char, 2048>           gPathBuffer{};
bool                             gPathInitialized{};
lholo::ui::MenuPage              gActivePage{lholo::ui::MenuPage::Projection};

} // namespace

std::atomic_bool& uiGuiVisible() { return gGuiVisible; }
std::atomic_int& uiOpeningInputBlockFrames() { return gOpeningInputBlockFrames; }
std::atomic_uint64_t& uiBlockGameInputUntil() { return gBlockGameInputUntil; }

std::atomic_bool& uiHudEnabled() { return gHudEnabled; }
std::atomic_bool& uiHudShowFileName() { return gHudShowFileName; }
std::atomic_bool& uiHudShowLayer() { return gHudShowLayer; }
std::atomic_bool& uiHudShowOverallProgress() { return gHudShowOverallProgress; }
std::atomic_bool& uiHudShowProgress() { return gHudShowProgress; }
std::atomic_bool& uiHudShowWrongState() { return gHudShowWrongState; }
std::atomic_bool& uiHudShowWrongType() { return gHudShowWrongType; }
std::atomic_bool& uiHudShowBlockEntity() { return gHudShowBlockEntity; }
std::atomic_int& uiHudPosition() { return gHudPosition; }
std::atomic<float>& uiUiScale() { return gUiScale; }

std::atomic_uint& uiGuiHotkey() { return gGuiHotkey; }
std::atomic_uint& uiGuiHotkeyModifiers() { return gGuiHotkeyModifiers; }
std::atomic_bool& uiCapturingGuiHotkey() { return gCapturingGuiHotkey; }
std::atomic_bool& uiGuiHotkeyHeld() { return gGuiHotkeyHeld; }
std::atomic_uint& uiLayerIncreaseHotkey() { return gLayerIncreaseHotkey; }
std::atomic_uint& uiLayerDecreaseHotkey() { return gLayerDecreaseHotkey; }
std::atomic_uint& uiLayerIncreaseHotkeyModifiers() { return gLayerIncreaseHotkeyModifiers; }
std::atomic_uint& uiLayerDecreaseHotkeyModifiers() { return gLayerDecreaseHotkeyModifiers; }
std::atomic_bool& uiCapturingLayerIncreaseHotkey() { return gCapturingLayerIncreaseHotkey; }
std::atomic_bool& uiCapturingLayerDecreaseHotkey() { return gCapturingLayerDecreaseHotkey; }
std::atomic_bool& uiLayerIncreaseHotkeyHeld() { return gLayerIncreaseHotkeyHeld; }
std::atomic_bool& uiLayerDecreaseHotkeyHeld() { return gLayerDecreaseHotkeyHeld; }

std::array<std::atomic_uint, 6>& uiMoveHotkeys() { return gMoveHotkeys; }
std::array<std::atomic_uint, 6>& uiMoveHotkeyModifiers() { return gMoveHotkeyModifiers; }
std::array<std::atomic_bool, 6>& uiCapturingMoveHotkey() { return gCapturingMoveHotkey; }
std::array<std::atomic_bool, 6>& uiMoveHotkeyHeld() { return gMoveHotkeyHeld; }
std::atomic_bool& uiControlHeld() { return gControlHeld; }
std::atomic_bool& uiAltHeld() { return gAltHeld; }
std::atomic_bool& uiShiftHeld() { return gShiftHeld; }
std::array<std::atomic_uint64_t, 256>& uiConsumeKeyReleaseUntil() {
    return gConsumeKeyReleaseUntil;
}

std::atomic_int& uiPendingOffsetX() { return gPendingOffsetX; }
std::atomic_int& uiPendingOffsetY() { return gPendingOffsetY; }
std::atomic_int& uiPendingOffsetZ() { return gPendingOffsetZ; }
std::atomic_int& uiPendingLayerDelta() { return gPendingLayerDelta; }
std::atomic_bool& uiPendingSettingsSave() { return gPendingSettingsSave; }
std::atomic_uint64_t& uiIgnoreHotkeyUntil() { return gIgnoreHotkeyUntil; }

std::mutex& uiMaterialMutex() { return gMaterialMutex; }
std::atomic_bool& uiMaterialListRequested() { return gMaterialListRequested; }
std::vector<MaterialRequirement>& uiMaterialRequirements() { return gMaterialRequirements; }

std::array<char, 2048>& uiPathBuffer() { return gPathBuffer; }
bool& uiPathInitialized() { return gPathInitialized; }
lholo::ui::MenuPage& uiActivePage() { return gActivePage; }

} // namespace lholo::structure::detail
