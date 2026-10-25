#pragma once
#include <optional>
#include <string>
#include <string_view>
namespace lholo::oreui {
bool queueRenderSettingsIntent(std::string_view payload);
std::optional<std::string> takeRenderSettingsUpdate(bool force = false);
void resetRenderSettingsBindings();
}
