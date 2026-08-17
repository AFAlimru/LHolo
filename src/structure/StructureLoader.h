// LHolo - Client-side projection renderer for Minecraft Bedrock Windows
// Copyright (C) 2026  MarmieQi
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

class Block;

namespace lholo::structure {

struct LoadedStructure {
    std::filesystem::path                 sourcePath;
    int                                  sizeX{};
    int                                  sizeY{};
    int                                  sizeZ{};
    std::uint64_t                        volume{};
    std::uint64_t                        primaryBlocks{};
    std::uint64_t                        secondaryBlocks{};
    std::uint64_t                        paletteEntries{};
    std::uint64_t                        generation{};
    struct RenderBlock {
        int          x{};
        int          y{};
        int          z{};
        Block const* block{};
        Block const* liquid{};
    };
    std::vector<RenderBlock>              renderBlocks;
};

void requestOpenGui();
bool isGuiVisible();
bool isInputTransitionBlocked();
bool handleGuiHotkeyKeyDown(unsigned int virtualKey);
bool handleGuiHotkeyKeyUp(unsigned int virtualKey);
void resetHotkeyState();
void processPendingHotkeyActions();
bool hasHudInfo();
void renderHud();
void renderGui();
void requestMaterialList();
void processPendingMaterialList();
void loadSettings();
void saveSettings();
std::shared_ptr<LoadedStructure const> getLoaded();
int getRotationQuarterTurns();
int getMirrorMode();
int getOffsetX();
int getOffsetY();
int getOffsetZ();
int getLayerDisplayMode();
int getDisplayLayer();
int getLayerAxis();
void recordProjectionAnchor(int x, int y, int z);
void clear();

} // namespace lholo::structure
