// LHolo - Adapts active mesh storage to the resource-free CPU draw cache.
#pragma once
#include <cstddef>
#include <cstdint>
class Vec3;
class BlockPos;
namespace lholo::projection::detail {
struct ProjectionState;
std::uint64_t prepareProjectionDrawLists(ProjectionState&, Vec3 const& camera, BlockPos const& origin,
                                         float opacity);
void recordProjectionDrawPass(ProjectionState&, bool alpha, std::uint64_t totalMicros,
                              std::uint64_t prepareMicros, std::size_t draws);
} // namespace lholo::projection::detail
