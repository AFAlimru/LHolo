// LHolo - Client-side projection renderer for Minecraft Bedrock Windows
// Copyright (C) 2026  MarmieQi
//
// Frame callbacks consumed by the stateful projection render hooks. The
// implementations still live in the projection facade until state ownership
// moves into runtime/.

#pragma once

class BaseActorRenderContext;
class BlockPos;

namespace lholo::projection::detail {

bool shouldSuppressProjectionHitSelect(BlockPos const& pos);

void renderProjectionFrame(BaseActorRenderContext& renderContext, bool renderAlphaLayer);

} // namespace lholo::projection::detail
