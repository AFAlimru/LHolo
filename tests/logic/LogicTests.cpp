// LHolo logic tests: pure projection rules and progress publication.
// Run with: xmake r LHoloLogicTests

#include <cstdio>

#include "projection/core/ProjectionRules.h"
#include "projection/runtime/ProjectionProgress.h"
#include "settings/SettingsStore.h"
#include "ui/HotkeyFormat.h"

#include <Windows.h>

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

void testSettingsStore() {
    auto const path = std::filesystem::temp_directory_path() / "lholo_settings_test.json";
    std::error_code error;
    std::filesystem::remove(path, error);

    lholo::settings::Settings settings;
    settings.uiScale = 1.25f;
    settings.guiHotkey = 'L';
    settings.guiHotkeyModifiers = 1;
    settings.moveHotkeys[4] = 0x57; // W
    settings.hasSavedProjection = true;
    settings.savedAnchorX = 12;
    settings.savedAnchorZ = -34;
    lholo::settings::saveSettingsFile(path, settings);

    lholo::settings::Settings loaded;
    LHOLO_CHECK(lholo::settings::loadSettingsFile(path, loaded));
    LHOLO_CHECK(loaded.uiScale == 1.25f);
    LHOLO_CHECK(loaded.guiHotkey == 'L');
    LHOLO_CHECK(loaded.guiHotkeyModifiers == 1);
    LHOLO_CHECK(loaded.moveHotkeys[4] == 0x57);
    LHOLO_CHECK(loaded.hasSavedProjection);
    LHOLO_CHECK(loaded.savedAnchorX == 12);
    LHOLO_CHECK(loaded.savedAnchorZ == -34);

    lholo::settings::Settings missing;
    std::filesystem::remove(path, error);
    LHOLO_CHECK(!lholo::settings::loadSettingsFile(path, missing));
    std::filesystem::remove(path, error);
}

void testHotkeyFormat() {
    LHOLO_CHECK(lholo::ui::isModifierKey(VK_CONTROL));
    LHOLO_CHECK(lholo::ui::isModifierKey(VK_MENU));
    LHOLO_CHECK(lholo::ui::isModifierKey(VK_LWIN));
    LHOLO_CHECK(!lholo::ui::isModifierKey('A'));
    LHOLO_CHECK(lholo::ui::hotkeyName(0) == "未设置");
    LHOLO_CHECK(lholo::ui::hotkeyChordName(0, 0) == "未设置");
    auto const chord = lholo::ui::hotkeyChordName(lholo::ui::kHotkeyModifierControl, 'M');
    LHOLO_CHECK(chord.rfind("Ctrl + ", 0) == 0);
    LHOLO_CHECK(chord.size() > 7);
}

} // namespace

int main() {
    testLayoutRules();
    testProgress();
    testSettingsStore();
    testHotkeyFormat();
    std::printf("LHoloLogicTests: %d checks, %d failures\n", gChecks, gFailures);
    return gFailures == 0 ? 0 : 1;
}
