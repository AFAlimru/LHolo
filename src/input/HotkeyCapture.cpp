#include "input/HotkeyCapture.h"
#include "structure/StructureUiState.h"
#include "ui/HotkeyFormat.h"
#include <Windows.h>
namespace lholo::input {
bool hotkeyCaptureActive() { return structure::detail::StructureUiState::getInstance().capturingHotkey().has_value(); }
bool captureHotkeyKey(unsigned key, unsigned modifiers) {
    auto& state = structure::detail::StructureUiState::getInstance();
    auto slot = state.capturingHotkey();
    if (!slot || key == VK_F11) return false;
    if (key == VK_ESCAPE) state.stopHotkeyCapture();
    else if (key == VK_DELETE || key == VK_BACK) {
        state.clearHotkey(*slot); state.stopHotkeyCapture(); state.requestSettingsSave();
    } else if (!ui::isModifierKey(key)) {
        state.bindCapturedHotkey(*slot, key, modifiers & 7);
        state.setIgnoreHotkeyUntil(GetTickCount64() + 250); state.requestSettingsSave();
    }
    return true;
}
}
