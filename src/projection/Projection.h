#pragma once

#include <cstdint>

namespace lholo::projection {

struct BuildProgress {
    std::uint64_t placed{};
    std::uint64_t total{};
    std::uint64_t wrongType{};
    std::uint64_t wrongState{};
};

bool installHook();
void uninstallHook();

void disable();
float getOpacity();
void setOpacity(float opacity);
float getCorrectionFillOpacity();
void setCorrectionFillOpacity(float opacity);
float getCorrectionOutlineOpacity();
void setCorrectionOutlineOpacity(float opacity);
bool getStructureBoundsEnabled();
void setStructureBoundsEnabled(bool enabled);
void requestNextStructureAnchor(int x, int y, int z);
BuildProgress getBuildProgress();

} // namespace lholo::projection
