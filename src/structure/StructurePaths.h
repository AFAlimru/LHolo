// LHolo - Client-side projection renderer for Minecraft Bedrock Windows
// Copyright (C) 2026  MarmieQi
//
// UTF-8 path conversion helpers shared by the structure module and UI.

#pragma once

#include <filesystem>
#include <string>
#include <string_view>

namespace lholo::structure::detail {

std::filesystem::path pathFromUtf8(std::string_view value);

std::string pathToUtf8(std::filesystem::path const& path);

} // namespace lholo::structure::detail
