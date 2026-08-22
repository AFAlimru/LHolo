// LHolo - Client-side projection renderer for Minecraft Bedrock Windows
// Copyright (C) 2026  MarmieQi
//
// Session-wide mutable state access contract. The storage is still owned by
// the projection facade; these accessors let frame orchestration run from
// runtime/ without touching Projection.cpp globals directly.

#pragma once

#include <mutex>

namespace lholo::overlay {
class BoundsWireframe;
}

namespace lholo::projection::detail {

struct ProjectionState;

std::mutex&               projectionStateMutex();
ProjectionState&          projectionState();
void                      clearProjectionStateLocked();
overlay::BoundsWireframe& projectionCaptureBounds();

float projectionOpacity();
float projectionCorrectionFillOpacity();
float projectionCorrectionOutlineOpacity();
bool  projectionStructureBoundsEnabled();

bool consumeProjectionAnchor(int& x, int& y, int& z);

} // namespace lholo::projection::detail
