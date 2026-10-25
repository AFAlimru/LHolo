#pragma once
#include "app/RenderSettingsTypes.h"
#include <optional>
#include <string_view>
namespace lholo::oreui {
std::optional<app::RenderSettingsEdit> parseRenderSettingsIntent(std::string_view payload);
}
