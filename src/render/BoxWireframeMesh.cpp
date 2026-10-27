#include "render/BoxWireframeMesh.h"

#include "mc/client/renderer/SupplementaryFieldAutoGenerationMode.h"
#include "mc/client/renderer/Tessellator.h"
#include "mc/deps/core/math/Vec2.h"
#include "mc/deps/minecraft_renderer/renderer/Mesh.h"
#include <limits>
#include <stdexcept>

namespace lholo::render {

std::unique_ptr<mce::Mesh> buildBoxWireframeMesh(
    Tessellator& tessellator, Vec3 const& minimum, Vec3 const& maximum,
    std::string_view debugName
) {
    BoxWireframeBounds const box{minimum, maximum};
    return buildBoxWireframesMesh(tessellator, {&box, 1}, debugName);
}

std::unique_ptr<mce::Mesh> buildBoxWireframesMesh(
    Tessellator& tessellator, std::span<BoxWireframeBounds const> boxes,
    std::string_view debugName
) {
    if (boxes.empty()) return nullptr;
    if (boxes.size() > static_cast<std::size_t>(std::numeric_limits<int>::max() / 24))
        throw std::length_error("Too many wireframe boxes");
    tessellator.begin(Tessellator::DebugContextCallback{}, mce::PrimitiveMode::LineList,
        static_cast<int>(boxes.size() * 24), false);
    tessellator.color(1.f, 1.f, 1.f, 1.f);
    for (auto const& box : boxes)
        for (auto const& vertex : boxWireframeVertices(box.minimum, box.maximum)) {
            tessellator.tex2({0.5f, 0.5f});
            tessellator.vertex(vertex.x, vertex.y, vertex.z);
        }
    return std::make_unique<mce::Mesh>(tessellator.end(
        Tessellator::UploadMode::Buffered, debugName, SupplementaryFieldAutoGenerationMode{0}));
}

} // namespace lholo::render
