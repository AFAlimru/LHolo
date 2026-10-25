#pragma once
#include "structure/capture/CaptureActions.h"
#include <optional>
#include <string_view>

namespace lholo::oreui {
std::optional<structure::capture::Request> parseCaptureIntent(std::string_view payload);
}
