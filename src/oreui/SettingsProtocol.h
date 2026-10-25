#pragma once
#include "app/SettingsActions.h"
#include <optional>
#include <string_view>
#include <array>
namespace lholo::oreui {
inline constexpr std::array<std::string_view, app::kSettingsFieldCount> kSettingsNames{
    "hudEnabled", "hudPosition", "hudShowFileName", "hudShowLayer", "hudShowOverallProgress", "hudShowProgress",
    "hudShowProjectedBlockName", "hudShowWrongState", "hudShowWrongType", "hudShowExtraBlocks", "materialHudEnabled",
    "materialHudPosition", "altWheelOffsetEnabled", "placementRadius", "autoPlacementCooldown", "activeMode", "language"};
std::optional<app::SettingsIntent> parseSettingsIntent(std::string_view payload);
}
