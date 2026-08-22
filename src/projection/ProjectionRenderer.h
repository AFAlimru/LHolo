// LHolo - Client-side projection renderer for Minecraft Bedrock Windows
// Copyright (C) 2026  MarmieQi
//
// Submission of already-built projection meshes. CPU construction, upload and
// lifecycle recovery remain outside this module.

#pragma once

class BaseActorRenderContext;
class BlockPos;
class IClientInstance;
class Vec3;

namespace lholo::projection::detail {

class ProjectionState;

void submitProjectionMeshPass(
    ProjectionState&        state,
    BaseActorRenderContext& renderContext,
    IClientInstance&        client,
    BlockPos const&         renderOrigin,
    Vec3 const&             camera,
    float                   structureOpacity,
    bool                    renderAlphaLayer,
    bool                    structureBoundsEnabled
);

} // namespace lholo::projection::detail
