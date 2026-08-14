#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

class CompoundTag;
class Block;

namespace lholo::structure {

struct LoadedStructure {
    std::filesystem::path                 sourcePath;
    std::unique_ptr<CompoundTag>           rootTag;
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
