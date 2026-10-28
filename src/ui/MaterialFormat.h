#pragma once

#include "i18n/Translator.h"
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace lholo::ui {
// Shared by menu and native HUD; no dependency on either rendering backend.
// HUD conversion shows stacks only (43x64+34), or the raw count when disabled.
std::string formatMaterialHudCount(std::uint64_t count, int stackSize, bool convertStacks);
// Liquids use the interface language key; other names come from the game.
char const* materialDisplayName(std::string const& displayName, std::optional<i18n::TextKey> const& nameKey);
// Display identity only: never changes inventory availability or placement.
std::string_view materialIconId(std::string const& itemId, std::string const& typeName);
}
