#include "projection/runtime/ProjectionVisibility.h"
#include "mc/client/gui/screens/ScreenContext.h"
#include "mc/client/renderer/BaseActorRenderContext.h"
#include "mc/deps/renderer/Camera.h"
#include "projection/core/ProjectionState.h"

namespace lholo::projection::detail {
void updateProjectionVisibility(ProjectionState &state, BaseActorRenderContext &context, Vec3 const &camera) {
    ProjectionFrustum frustum;
    auto const &cameraObject = context.mScreenContext.camera;
    if (!cameraObject.viewMatrixStack->stack->empty() &&
        !cameraObject.projectionMatrixStack->stack->empty()) {
        frustum = ProjectionFrustum::fromViewProjection(cameraObject.getProjectionMatrix() *
                                                        *cameraObject.viewMatrixStack->top()._m);
    }
    Vec3 const origin{static_cast<float>(state.anchor.x + state.cachedOffsetX) - camera.x,
                      static_cast<float>(state.anchor.y + state.cachedOffsetY) - camera.y,
                      static_cast<float>(state.anchor.z + state.cachedOffsetZ) - camera.z};
    state.visibleSectionCount = 0;
    for (auto &section : state.sections) {
        // Actual vertex bounds include liquid and correction geometry. Before
        // geometry exists, build priority uses transformed source-cell bounds.
        section.renderVisible = projectionBoundsVisible(frustum, section.meshBounds, origin);
        section.buildVisible = projectionBoundsVisible(frustum, section.buildBounds, origin);
        section.prefetchVisible = projectionBoundsVisible(frustum, section.buildBounds, origin, 16.0f);
        if (section.renderVisible)
            ++state.visibleSectionCount;
    }
}
} // namespace lholo::projection::detail
