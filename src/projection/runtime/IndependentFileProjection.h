#pragma once
#include <memory>
class BaseActorRenderContext;
namespace lholo::structure {
struct LoadedStructure;
}
namespace lholo::projection::detail {
struct ProjectionState;
bool updateIndependentFiles(ProjectionState&, BaseActorRenderContext&,
                            std::shared_ptr<structure::LoadedStructure const> const&);
void distributeIndependentFileEvents(ProjectionState&);
ProjectionState* selectedFileProjection(ProjectionState&);
} // namespace lholo::projection::detail
