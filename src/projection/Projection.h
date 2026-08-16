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

class Block;
class BlockPos;

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

// Easy-place support: query the currently projected virtual world in a single
// locked lookup.
struct ProjectionQuery {
    Block const* block;   // transformed expected block, or nullptr when the
                          // position is not a placeable projection target
                          // (liquid-only cells and hidden layers excluded)
    bool missing;         // correction state is Missing, i.e. worth placing
};
ProjectionQuery queryProjection(BlockPos const& worldPos);

} // namespace lholo::projection
