// LHolo - Client-side projection renderer for Minecraft Bedrock Windows
// Copyright (C) 2026  MarmieQi
//
// Section state store. Resizes the per-section state vector and restores the
// initial build flags; scheduling and upload semantics stay unchanged.

#pragma once

#include "projection/core/ProjectionState.h"

#include <vector>

namespace lholo::projection::detail {

void initializeSectionStates(
    std::vector<SectionState>& sections,
    std::vector<Vec3> const&   centers
);

} // namespace lholo::projection::detail
