// LHolo - Client-side projection renderer for Minecraft Bedrock Windows
// Copyright (C) 2026  MarmieQi

#pragma once

#include "render/OverlayMaterials.h"

class IClientInstance;

namespace lholo::projection::detail {

// The water and lava tiles are animated flipbooks. RenderDragon advances them
// in a texture the projection cannot bind (the matching cell of the terrain
// atlas holds no frame), so the projection keeps its own small texture: a 2x2
// grid of the current still and flowing frames of water and lava, refilled
// from the active resource packs' flipbook images as time passes.
enum class LiquidFlipbookCell : int {
    WaterStill = 0,
    WaterFlow  = 1,
    LavaStill  = 2,
    LavaFlow   = 3,
};

struct LiquidFlipbookUv {
    float u0, v0, u1, v1;
};

// Normalized UV rectangle of a cell in the flipbook texture. Constant, so mesh
// workers may call it.
LiquidFlipbookUv liquidFlipbookCellUv(LiquidFlipbookCell cell);

// True once the flipbook texture has been built for this session. It never
// turns false again until resetLiquidFlipbook(), so a mesh built against the
// flipbook cells always has the texture to draw with. Safe from any thread.
bool liquidFlipbookReady();

// Client thread only. Loads the flipbook images and uploads the texture.
// Returns liquidFlipbookReady().
bool prepareLiquidFlipbook(IClientInstance& client);

// Client (render) thread only. Advances the animation and returns the texture
// to bind for the liquid hulls, or nullptr when the flipbook is not ready.
render::TextureVariant const* acquireLiquidFlipbookTexture(IClientInstance& client);

} // namespace lholo::projection::detail
