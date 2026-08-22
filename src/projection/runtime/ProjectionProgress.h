// LHolo - Client-side projection renderer for Minecraft Bedrock Windows
// Copyright (C) 2026  MarmieQi
//
// Lock-free HUD progress publication. Projection state owns the counters;
// this module only publishes snapshots across the render/UI boundary.

#pragma once

#include "projection/ProjectionTypes.h"

#include <cstdint>

namespace lholo::projection::detail {

void resetPublishedBuildProgress();
void initializePublishedBuildProgress(std::uint64_t total);
void resetPublishedBuildProgressCounts();
void publishPlacedProgress(std::uint64_t placed);
void publishVisiblePlacedProgress(std::uint64_t placed);
void publishVisibleProgress(std::uint64_t placed, std::uint64_t total);
void publishErrorProgress(std::uint64_t wrongType, std::uint64_t wrongState);

BuildProgress getPublishedBuildProgress();

} // namespace lholo::projection::detail
