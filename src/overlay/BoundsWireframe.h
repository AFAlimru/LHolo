#pragma once

#include <cstdint>
#include <memory>

#include "mc/world/level/BlockPos.h"

class BaseActorRenderContext;
namespace mce { class Mesh; }

namespace lholo::overlay {

class BoundsWireframe {
public:
    BoundsWireframe() = default;
    ~BoundsWireframe();

    BoundsWireframe(BoundsWireframe const&) = delete;
    BoundsWireframe& operator=(BoundsWireframe const&) = delete;

    void setBounds(BlockPos const& min, BlockPos const& max, std::uint32_t color);
    void clear();
    void render(BaseActorRenderContext& renderContext, bool renderAlphaLayer);

private:
    BlockPos                   mMin{};
    BlockPos                   mMax{};
    std::uint32_t              mColor{};
    bool                       mHasBounds{};
    std::unique_ptr<mce::Mesh> mMesh;
};

} // namespace lholo::overlay
