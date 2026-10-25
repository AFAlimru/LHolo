#pragma once
#include <optional>
#include <string>
#include <string_view>

namespace lholo::oreui {
bool queueCaptureIntent(std::string_view payload);
std::optional<std::string> takeCaptureUpdate(bool force = false);
void resetCaptureBindings();
void shutdownCaptureBindings();
}
