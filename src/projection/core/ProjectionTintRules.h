// LHolo - Validate biome samples before they become persistent vertex colors.
#pragma once

#include <array>
#include <cmath>
#include <cstdint>

namespace lholo::projection::detail {

using ProjectionTintRgb = std::array<float, 3>;

inline bool validProjectionBiomeTint(ProjectionTintRgb const& color) {
    for (auto const channel : color) {
        if (!std::isfinite(channel) || channel < 0.0f || channel > 1.0f)
            return false;
    }
    // Projection display policy treats pure black as a suspicious tint.
    // This does not identify the cause of a black sample in the engine.
    return color[0] > 0.0f || color[1] > 0.0f || color[2] > 0.0f;
}

// Called only after the original sample has been rejected by the adapter.
inline ProjectionTintRgb projectionFallbackBiomeTint(ProjectionTintRgb const& engineDefault) {
    if (validProjectionBiomeTint(engineDefault))
        return engineDefault;
    return {1.0f, 1.0f, 1.0f};
}

class ProjectionTintQuadObserver {
  public:
    void observe(std::uint32_t color) {
        if (mBlackFace)
            return;
        mQuadRgb |= color & 0x00FFFFFFU;
        if (++mVertices == 4) {
            mBlackFace = mQuadRgb == 0;
            mVertices = 0;
            mQuadRgb = 0;
        }
    }
    bool hasBlackFace() const { return mBlackFace; }

  private:
    std::uint32_t mQuadRgb{};
    unsigned int mVertices{};
    bool mBlackFace{};
};

} // namespace lholo::projection::detail
