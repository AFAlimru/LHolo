// LHolo - Client-side projection renderer for Minecraft Bedrock Windows
// Copyright (C) 2026  MarmieQi
//
// Render-thread consumption and GPU upload of completed worker MeshData.

#pragma once

class Tessellator;

namespace lholo::projection::detail {

struct ProjectionState;

void uploadCompletedProjectionMeshes(ProjectionState& state, Tessellator& tessellator);

} // namespace lholo::projection::detail
