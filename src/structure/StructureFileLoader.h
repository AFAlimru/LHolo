// LHolo - Client-side projection renderer for Minecraft Bedrock Windows
// Copyright (C) 2026  MarmieQi

#pragma once

#include <string>

namespace lholo::structure::detail {

struct StructureLoadOrigin;

// One owned background task; normal close/world exit invalidates publication
// without waiting. Level destruction and mod shutdown wait for file parsing
// before the world's registry or this DLL can be released.
void enableStructureFileLoader();
void shutdownStructureFileLoader();
bool isStructureFileLoading();
bool startStructureFileLoad(std::string path, StructureLoadOrigin origin);
bool startSavedProjectionLoad();

} // namespace lholo::structure::detail
