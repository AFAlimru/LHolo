// LHolo - Client-side projection renderer for Minecraft Bedrock Windows
// Copyright (C) 2026  MarmieQi
//
// Explicit immutable inputs shared by asynchronous and synchronous section
// geometry construction. The builder implementation is migrated separately.

#pragma once

#include "mc/util/Mirror.h"
#include "mc/util/Rotation.h"

namespace lholo::projection::detail {

struct ProjectionSectionBuildSettings {
    Mirror   mirror{Mirror::None};
    Rotation rotation{Rotation::None};
    int      mirrorMode{};
    int      rotationTurns{};
    int      offsetX{};
    int      offsetY{};
    int      offsetZ{};
    float    structureOpacity{};
    float    correctionFillOpacity{};
    float    correctionOutlineOpacity{};
    bool     identityTransform{};
};

} // namespace lholo::projection::detail
