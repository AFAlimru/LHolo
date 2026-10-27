#pragma once
#include "input/HotkeyTypes.h"
#include "i18n/TextKeys.h"
#include <array>
namespace lholo::ui {
using input::HotkeyId;
struct HotkeyDefinition { HotkeyId id; i18n::TextKey label; };
inline constexpr std::array<HotkeyDefinition, input::kHotkeyCount> kHotkeyDefinitions{{
    {HotkeyId::Gui, i18n::TextKey::HotkeyOpenMenu},
    {HotkeyId::MoveLeft, i18n::TextKey::HotkeyMoveLeft},
    {HotkeyId::MoveRight, i18n::TextKey::HotkeyMoveRight},
    {HotkeyId::MoveForward, i18n::TextKey::HotkeyMoveForward},
    {HotkeyId::MoveBackward, i18n::TextKey::HotkeyMoveBackward},
    {HotkeyId::MoveUp, i18n::TextKey::HotkeyMoveUp},
    {HotkeyId::MoveDown, i18n::TextKey::HotkeyMoveDown},
    {HotkeyId::LayerIncrease, i18n::TextKey::HotkeyLayerIncrease},
    {HotkeyId::LayerDecrease, i18n::TextKey::HotkeyLayerDecrease}
}};
}
