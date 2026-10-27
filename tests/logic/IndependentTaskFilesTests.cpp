#include "structure/IndependentTaskFiles.h"
#include <cstdio>
#include <limits>

int runIndependentTaskFilesTests() {
    using namespace lholo::structure;
    int failures{};
    auto check = [&](bool ok, char const* message) {
        if (!ok) {
            ++failures;
            std::printf("IndependentTaskFilesTests FAILED: %s\n", message);
        }
    };
    auto raw = std::make_shared<LoadedStructure>();
    raw->sizeX = 3;
    raw->sizeY = 2;
    raw->sizeZ = 6;
    raw->regions.push_back({0, 0, 0, 3, 2, 6});
    auto stone = reinterpret_cast<Block const*>(0x1000);
    raw->renderBlocks = {{0, 0, 0, stone}, {2, 1, 5, stone}};
    raw->primaryBlocks = 2;
    int transformed{};
    auto transform = [&](Block const* b, int, int) {
        if (b)
            ++transformed;
        return b;
    };
    std::vector<CompositionFile> files{{"A", raw, true}, {"B", raw, true, 30}};
    auto independent = composeTaskProjection(files, transform);
    check(independent.loaded->independentFiles != nullptr, "disjoint AABBs select original per-file data");
    check(transformed == 0 && independent.loaded->renderBlocks.empty(),
          "disjoint branch never merges or transforms blocks");
    check(independentTaskFile(*independent.loaded, 0)->loaded == raw &&
              independentTaskFile(*independent.loaded, 1)->loaded == raw,
          "duplicate path entries share raw data but retain file identity");
    auto moving = files[0];
    moving.x = 5;
    check(independentTaskPlacement(moving, files), "live movement in free space needs no composition");
    moving.x = 29;
    check(!independentTaskPlacement(moving, files),
          "live movement into another file leaves independent branch");
    files[0].x = -400;
    auto moved = composeTaskProjection(files, transform);
    check(moved.origin == std::array<int, 3>{0, 0, 0}, "moving A preserves task anchor");
    check(independentTaskFile(*moved.loaded, 1)->x == 30 &&
              independentTaskFile(*moved.loaded, 1)->loaded ==
                  independentTaskFile(*independent.loaded, 1)->loaded,
          "moving A preserves B data and world offset");
    check(transformed == 0, "XYZ edits of disjoint files have zero block transformation work");
    files[0].x = 0;
    files[1].x = 4;
    check(composeTaskProjection(files, transform).loaded->independentFiles != nullptr,
          "one empty block between boxes stays independent");
    files[1].x = 3;
    check(!composeTaskProjection(files, transform).loaded->independentFiles,
          "touching faces retain shared neighbor models");
    files[1].x = 2;
    auto overlap = composeTaskProjection(files, transform);
    auto oracle = composeTask(files, transform);
    check(!overlap.loaded->independentFiles &&
              overlap.loaded->renderBlocks.size() == oracle.loaded->renderBlocks.size(),
          "overlap uses established composition");
    files[1].x = 0;
    files[1].y = 3;
    check(composeTaskProjection(files, transform).loaded->independentFiles != nullptr,
          "separation on Y is sufficient");
    files[1].y = 0;
    files[1].z = 7;
    check(composeTaskProjection(files, transform).loaded->independentFiles != nullptr,
          "separation on Z is sufficient");
    files[1].z = 0;
    files[1].x = 5;
    check(composeTaskProjection(files, transform).loaded->independentFiles != nullptr,
          "unrotated width is three");
    files[0].rotation = 1;
    check(!composeTaskProjection(files, transform).loaded->independentFiles,
          "90-degree rotation swaps width/depth before AABB test");
    files[0].rotation = 2;
    files[0].mirror = 2;
    check(composeTaskProjection(files, transform).loaded->independentFiles != nullptr,
          "180-degree rotation and mirror preserve extents");
    files[1].x = 0;
    files[1].visible = false;
    auto hidden = composeTaskProjection(files, transform);
    check(hidden.loaded->independentFiles != nullptr && hidden.loaded->sourceFiles.size() == 2,
          "hidden file retains identity without participating in overlap");
    files[0].visible = false;
    check(composeTaskProjection(files, transform).loaded->independentFiles != nullptr,
          "all hidden remains an empty independent task");
    check(!independentTaskFile(*hidden.loaded, -1) && !independentTaskFile(*hidden.loaded, 2),
          "source bounds validated");
    files[0].visible = true;
    files[0].x = std::numeric_limits<int>::max();
    bool rejected{};
    try {
        composeTaskProjection(files, transform);
    } catch (...) {
        rejected = true;
    }
    check(rejected, "overflowing transformed extent rejected");
    std::printf("IndependentTaskFilesTests: %d failures\n", failures);
    return failures;
}
