// LHolo logic tests: pure projection rules and progress publication.
// Run with: xmake r LHoloLogicTests

#include <cstdio>

#include "projection/core/ProjectionRules.h"
#include "projection/runtime/ProjectionProgress.h"

namespace {

int gChecks = 0;
int gFailures = 0;

#define LHOLO_CHECK(cond)                                     \
    do {                                                      \
        ++gChecks;                                            \
        if (!(cond)) {                                        \
            ++gFailures;                                      \
            std::fprintf(stderr, "FAIL %s:%d: %s\n",          \
                __FILE__, __LINE__, #cond);                   \
        }                                                     \
    } while (false)

using namespace lholo::projection::detail;
using lholo::structure::LoadedStructure;

bool expectBlockPos(BlockPos const& pos, int x, int y, int z) {
    return pos.x == x && pos.y == y && pos.z == z;
}

void testLayoutRules() {
    LHOLO_CHECK(getProjectionMirror(0) == Mirror::None);
    LHOLO_CHECK(getProjectionMirror(1) == Mirror::Z);
    LHOLO_CHECK(getProjectionMirror(2) == Mirror::X);
    LHOLO_CHECK(getProjectionMirror(9) == Mirror::None);

    LHOLO_CHECK(getProjectionRotation(0) == Rotation::None);
    LHOLO_CHECK(getProjectionRotation(1) == Rotation::Clockwise90);
    LHOLO_CHECK(getProjectionRotation(2) == Rotation::Clockwise180);
    LHOLO_CHECK(getProjectionRotation(3) == Rotation::CounterClockwise90);
    LHOLO_CHECK(getProjectionRotation(4) == Rotation::None);
    LHOLO_CHECK(getProjectionRotation(5) == Rotation::Clockwise90);
    LHOLO_CHECK(getProjectionRotation(-1) == Rotation::CounterClockwise90);

    LoadedStructure loaded;
    loaded.sizeX = 4;
    loaded.sizeY = 3;
    loaded.sizeZ = 5;
    LoadedStructure::RenderBlock const entry{1, 2, 3, nullptr, nullptr, nullptr};

    LHOLO_CHECK(expectBlockPos(transformStructurePosition(entry, loaded, 0, 0), 1, 2, 3));
    LHOLO_CHECK(expectBlockPos(transformStructurePosition(entry, loaded, 1, 0), 2, 2, 3));
    LHOLO_CHECK(expectBlockPos(transformStructurePosition(entry, loaded, 2, 0), 1, 2, 1));
    LHOLO_CHECK(expectBlockPos(transformStructurePosition(entry, loaded, 0, 1), 1, 2, 1));
    LHOLO_CHECK(expectBlockPos(transformStructurePosition(entry, loaded, 0, 2), 2, 2, 1));
    LHOLO_CHECK(expectBlockPos(transformStructurePosition(entry, loaded, 0, 3), 3, 2, 2));
    LHOLO_CHECK(expectBlockPos(transformStructurePosition(entry, loaded, 1, 1), 1, 2, 2));

    LHOLO_CHECK(isLayerVisible(3, 0, 0));
    LHOLO_CHECK(!isLayerVisible(3, 1, 2));
    LHOLO_CHECK(isLayerVisible(2, 1, 2));
    LHOLO_CHECK(isLayerVisible(2, 2, 3));
    LHOLO_CHECK(!isLayerVisible(4, 2, 3));
    LHOLO_CHECK(isLayerVisible(4, 3, 3));
    LHOLO_CHECK(!isLayerVisible(2, 3, 3));

    LHOLO_CHECK(renderBucketFor(BlockRenderLayer::RenderlayerOpaque) == RenderBucket::Opaque);
    LHOLO_CHECK(renderBucketFor(BlockRenderLayer::RenderlayerSeasonsOpaque) == RenderBucket::Opaque);
    LHOLO_CHECK(renderBucketFor(BlockRenderLayer::RenderlayerBlend) == RenderBucket::Blend);
    LHOLO_CHECK(renderBucketFor(BlockRenderLayer::RenderlayerBlendToOpaque) == RenderBucket::Blend);
    LHOLO_CHECK(renderBucketFor(BlockRenderLayer::RenderlayerAlphatestSingleSide) == RenderBucket::AlphaOneSided);
    LHOLO_CHECK(renderBucketFor(BlockRenderLayer::RenderlayerAlphatest) == RenderBucket::Alpha);
    LHOLO_CHECK(renderBucketFor(BlockRenderLayer::RenderlayerDoubleSided) == RenderBucket::Alpha);
}

void testProgress() {
    initializePublishedBuildProgress(100);
    auto progress = getPublishedBuildProgress();
    LHOLO_CHECK(progress.total == 100);
    LHOLO_CHECK(progress.visibleTotal == 100);
    LHOLO_CHECK(progress.placed == 0);

    publishPlacedProgress(120);
    progress = getPublishedBuildProgress();
    LHOLO_CHECK(progress.placed == 100);

    publishVisibleProgress(60, 80);
    publishErrorProgress(130, 5);
    progress = getPublishedBuildProgress();
    LHOLO_CHECK(progress.visiblePlaced == 60);
    LHOLO_CHECK(progress.visibleTotal == 80);
    LHOLO_CHECK(progress.wrongType == 100);
    LHOLO_CHECK(progress.wrongState == 5);

    resetPublishedBuildProgress();
    progress = getPublishedBuildProgress();
    LHOLO_CHECK(progress.total == 0);
    LHOLO_CHECK(progress.placed == 0);
    LHOLO_CHECK(progress.visibleTotal == 0);

    initializePublishedBuildProgress(50);
    publishVisibleProgress(40, 30);
    progress = getPublishedBuildProgress();
    LHOLO_CHECK(progress.visiblePlaced == 30);
    LHOLO_CHECK(progress.total == 50);

    resetPublishedBuildProgressCounts();
    progress = getPublishedBuildProgress();
    LHOLO_CHECK(progress.total == 50);
    LHOLO_CHECK(progress.placed == 0);
    LHOLO_CHECK(progress.wrongType == 0);
    LHOLO_CHECK(progress.wrongState == 0);
}

} // namespace

int main() {
    testLayoutRules();
    testProgress();
    std::printf("LHoloLogicTests: %d checks, %d failures\n", gChecks, gFailures);
    return gFailures == 0 ? 0 : 1;
}
