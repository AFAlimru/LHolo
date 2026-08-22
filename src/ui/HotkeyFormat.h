// LHolo - Client-side projection renderer for Minecraft Bedrock Windows
// Copyright (C) 2026  MarmieQi
//
// Pure hotkey formatting helpers. These functions do not read session state;
// callers pass the key and modifier mask explicitly.

#pragma once

#include <string>

namespace lholo::ui {

inline constexpr unsigned int kHotkeyModifierControl = 1u;
inline constexpr unsigned int kHotkeyModifierAlt     = 2u;
inline constexpr unsigned int kHotkeyModifierShift   = 4u;

bool isModifierKey(unsigned int key);

std::string hotkeyName(unsigned int key);

std::string hotkeyChordName(unsigned int modifiers, unsigned int key);

} // namespace lholo::ui
