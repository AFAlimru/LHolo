#pragma once
#include <optional>
#include <string>
#include <string_view>
namespace lholo::oreui {
bool queueSettingsIntent(std::string_view payload);
std::optional<std::string> takeSettingsUpdate(bool force = false);
void cancelSettingsHotkeyCapture();
void resetSettingsBindings();
}
