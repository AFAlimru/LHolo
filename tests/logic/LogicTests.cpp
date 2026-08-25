// LHolo logic tests: pure projection rules and progress publication.
// Run with: xmake r LHoloLogicTests

#include <cstdio>
#include <fstream>

#include "place/PlacementState.h"
#include "projection/core/ProjectionRules.h"
#include "projection/runtime/ProjectionProgress.h"
#include "settings/SettingsStore.h"
#include "structure/StructureSession.h"
#include "structure/StructureUiState.h"
#include "structure/java_to_bedrock/JavaBlockEntityToBedrock.h"
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
    settings.hudShowProjectedBlockName = false;
    settings.autoPlacementBreakCooldownSeconds = 27;
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
    LHOLO_CHECK(!loaded.hudShowProjectedBlockName);
    LHOLO_CHECK(loaded.autoPlacementBreakCooldownSeconds == 27);
    LHOLO_CHECK(loaded.moveHotkeys[4] == 0x57);
    LHOLO_CHECK(loaded.hasSavedProjection);
    LHOLO_CHECK(loaded.savedAnchorX == 12);
    LHOLO_CHECK(loaded.savedAnchorZ == -34);

    // Existing configs keep their preference when the old, narrower
    // block-entity label migrates to the projected-block label.
    {
        std::ofstream legacy(path, std::ios::trunc);
        legacy << R"({"hudShowBlockEntity":false})";
    }
    lholo::settings::Settings migrated;
    LHOLO_CHECK(lholo::settings::loadSettingsFile(path, migrated));
    LHOLO_CHECK(!migrated.hudShowProjectedBlockName);
    LHOLO_CHECK(migrated.autoPlacementBreakCooldownSeconds == 10);

    lholo::settings::Settings missing;
    std::filesystem::remove(path, error);
    LHOLO_CHECK(!lholo::settings::loadSettingsFile(path, missing));
    std::filesystem::remove(path, error);
}

void testStructureSession() {
    using lholo::structure::detail::SavedProjectionSnapshot;
    using lholo::structure::detail::StructureSession;

    auto& session = StructureSession::getInstance();
    session.clearLoaded("尚未加载结构文件");
    session.resetTransform();
    session.setLastPath("initial.mcstructure");
    session.setSavedProjection(SavedProjectionSnapshot{});

    auto snapshot = session.snapshot();
    LHOLO_CHECK(!snapshot.loaded);
    LHOLO_CHECK(snapshot.status == "尚未加载结构文件");
    LHOLO_CHECK(snapshot.lastPath == "initial.mcstructure");
    LHOLO_CHECK(snapshot.transform.rotation == 0);
    LHOLO_CHECK(!snapshot.saved.available);

    auto loaded = std::make_shared<LoadedStructure>();
    loaded->sizeX = 7;
    loaded->sizeY = 5;
    loaded->sizeZ = 3;
    session.replaceLoaded(loaded, "active.mcstructure", "loaded");
    LHOLO_CHECK(session.setRotation(2));
    LHOLO_CHECK(!session.setRotation(2));
    LHOLO_CHECK(session.setMirror(1));
    LHOLO_CHECK(session.setOffsetX(12));
    LHOLO_CHECK(session.setOffsetY(-4));
    LHOLO_CHECK(session.setLayerDisplayMode(1));
    LHOLO_CHECK(session.setDisplayLayer(4));
    LHOLO_CHECK(session.setLayerAxis(1));

    snapshot = session.snapshot();
    LHOLO_CHECK(snapshot.loaded == loaded);
    LHOLO_CHECK(snapshot.maxLayerY == 4);
    LHOLO_CHECK(snapshot.maxLayerX == 6);
    LHOLO_CHECK(snapshot.transform.offsetX == 12);
    LHOLO_CHECK(snapshot.transform.offsetY == -4);

    session.recordProjectionAnchor(10, 20, 30);
    auto const saved = session.savedProjection();
    LHOLO_CHECK(saved.available);
    LHOLO_CHECK(saved.anchorX == 10);
    LHOLO_CHECK(saved.anchorY == 20);
    LHOLO_CHECK(saved.anchorZ == 30);
    LHOLO_CHECK(saved.transform.rotation == 2);
    LHOLO_CHECK(saved.transform.mirror == 1);
    LHOLO_CHECK(saved.structurePath == "active.mcstructure");

    auto layered = std::make_shared<LoadedStructure>();
    layered->sizeX = 3;
    layered->sizeY = 8;
    layered->sizeZ = 4;
    session.replaceLoaded(layered, "layered.mcstructure", "layered");
    session.setLayerDisplayMode(2);
    session.setDisplayLayer(7);
    session.setLayerAxis(0);
    session.recordProjectionAnchor(40, 50, 60);
    session.clearLoaded("closed");
    session.setDisplayLayer(0); // Empty-menu clamping must not alter the saved layer.
    auto const layeredSaved = session.savedProjection();
    LHOLO_CHECK(layeredSaved.transform.layerDisplayMode == 2);
    LHOLO_CHECK(layeredSaved.transform.displayLayer == 7);
    LHOLO_CHECK(layeredSaved.transform.layerAxis == 0);

    session.setLayerDisplayMode(layeredSaved.transform.layerDisplayMode);
    session.setDisplayLayer(layeredSaved.transform.displayLayer);
    session.setLayerAxis(layeredSaved.transform.layerAxis);
    session.replaceLoaded(layered, layeredSaved.structurePath, "restored");
    snapshot = session.snapshot();
    LHOLO_CHECK(snapshot.loaded == layered);
    LHOLO_CHECK(snapshot.maxLayerY == 7);
    LHOLO_CHECK(snapshot.transform.displayLayer == 7);

    session.clearLoaded("closed");
}

void testPlacementState() {
    using lholo::place::detail::FailedPlanKey;
    using lholo::place::detail::PlacementState;

    auto& state = PlacementState::getInstance();
    state.setEnabled(true);
    state.setRangeEnabled(true);
    state.setManualMode(true);
    state.setRadius(3);
    state.setAutoPlacementBreakCooldownSeconds(12);
    state.setManualPressAt(100);
    state.setLastManualPlaceAt(80);
    state.setManualPlaceRequested(true);
    state.setManualHeld(true);
    state.setNextPlaceAt(140);
    state.setNextSwapAt(150);

    LHOLO_CHECK(state.enabled());
    LHOLO_CHECK(state.rangeEnabled());
    LHOLO_CHECK(state.manualMode());
    LHOLO_CHECK(state.radius() == 3);
    LHOLO_CHECK(state.autoPlacementBreakCooldownSeconds() == 12);
    LHOLO_CHECK(state.manualPressAt() == 100);
    LHOLO_CHECK(state.lastManualPlaceAt() == 80);
    LHOLO_CHECK(state.manualPlaceRequested());
    LHOLO_CHECK(state.manualHeld());
    LHOLO_CHECK(state.nextPlaceAt() == 140);
    LHOLO_CHECK(state.nextSwapAt() == 150);

    constexpr std::int64_t recentCell = 0x123456789LL;
    state.recordRecentPlacement(recentCell, 100, 150);
    LHOLO_CHECK(state.recentPlacementActive(recentCell, 149));
    LHOLO_CHECK(!state.recentPlacementActive(recentCell, 150));

    constexpr std::int64_t suppressedCell = 0x23456789ALL;
    LHOLO_CHECK(!state.autoPlacementSuppressionsActive(100));
    state.suppressAutoPlacement(suppressedCell, 200);
    LHOLO_CHECK(state.autoPlacementSuppressionsActive(100));
    LHOLO_CHECK(state.autoPlacementSuppressed(suppressedCell, 199));
    LHOLO_CHECK(!state.autoPlacementSuppressionsActive(200));
    LHOLO_CHECK(!state.autoPlacementSuppressed(suppressedCell, 200));

    // Extending the current earliest entry may leave the cheap expiry hint at
    // its old value, but the first boundary refresh must retain the live entry.
    state.suppressAutoPlacement(suppressedCell, 300);
    state.suppressAutoPlacement(suppressedCell, 350);
    LHOLO_CHECK(state.autoPlacementSuppressionsActive(300));
    LHOLO_CHECK(state.autoPlacementSuppressed(suppressedCell, 349));
    LHOLO_CHECK(!state.autoPlacementSuppressionsActive(350));

    FailedPlanKey const failedKey{recentCell, 42, 7, 1, 2, 3, 4, 5, 6};
    state.cacheFailedPlan(failedKey, 200, 250);
    LHOLO_CHECK(state.failedPlanCached(failedKey, 249));
    LHOLO_CHECK(!state.failedPlanCached(failedKey, 250));

    state.setAimedProjectedBlockName("Test projected block");
    LHOLO_CHECK(state.aimedProjectedBlockName() == "Test projected block");

    state.setEnabled(false);
    state.setRangeEnabled(false);
    state.setManualMode(false);
    state.setRadius(4);
    state.setAutoPlacementBreakCooldownSeconds(10);
    state.setManualPressAt(0);
    state.setLastManualPlaceAt(0);
    state.setManualPlaceRequested(false);
    state.setManualHeld(false);
    state.setNextPlaceAt(0);
    state.setNextSwapAt(0);
    state.setAimedProjectedBlockName({});
}

void testStructureUiState() {
    using lholo::structure::detail::HudStateSnapshot;
    using lholo::structure::detail::StructureUiState;

    auto& state = StructureUiState::getInstance();
    state.resetHotkeys();
    state.resetHotkeyState();
    state.stopHotkeyCapture();
    (void)state.consumePendingHotkeyActions();
    state.clearMaterials();

    auto hud = state.hud();
    hud.enabled = false;
    hud.showLayer = false;
    hud.showProjectedBlockName = false;
    hud.position = 3;
    hud.uiScale = 1.5f;
    LHOLO_CHECK(state.applyHud(hud));
    LHOLO_CHECK(!state.applyHud(hud));
    auto const appliedHud = state.hud();
    LHOLO_CHECK(!appliedHud.enabled);
    LHOLO_CHECK(!appliedHud.showLayer);
    LHOLO_CHECK(!appliedHud.showProjectedBlockName);
    LHOLO_CHECK(appliedHud.position == 3);
    LHOLO_CHECK(appliedHud.uiScale == 1.5f);

    state.resetHotkeys();
    LHOLO_CHECK(state.hotkey(0).key == 'M');
    LHOLO_CHECK(state.hotkey(0).modifiers == lholo::ui::kHotkeyModifierAlt);
    LHOLO_CHECK(state.hotkey(1).key == VK_LEFT);
    LHOLO_CHECK(state.hotkey(7).key == VK_UP);

    state.beginHotkeyCapture(1);
    LHOLO_CHECK(state.capturingHotkey() == 1);
    state.setHotkey(2, 'K', lholo::ui::kHotkeyModifierControl);
    state.bindCapturedHotkey(1, 'K', lholo::ui::kHotkeyModifierControl);
    LHOLO_CHECK(state.hotkey(1).key == 'K');
    LHOLO_CHECK(state.hotkey(2).key == 0);
    LHOLO_CHECK(!state.capturingHotkey());

    state.setControlHeld(true);
    state.setShiftHeld(true);
    LHOLO_CHECK(
        state.currentHotkeyModifiers()
        == (lholo::ui::kHotkeyModifierControl | lholo::ui::kHotkeyModifierShift)
    );
    state.setControlHeld(false);
    state.setShiftHeld(false);

    state.resetHotkeys();
    LHOLO_CHECK(state.tryPressHotkey(0));
    LHOLO_CHECK(!state.tryPressHotkey(0));
    LHOLO_CHECK(state.releaseHotkeysForKey('M', 100));
    LHOLO_CHECK(state.releaseHotkeysForKey('M', 150));
    LHOLO_CHECK(!state.releaseHotkeysForKey('M', 201));

    state.queueMove(0);
    state.queueMove(4);
    state.queueLayerDelta(-1);
    state.requestSettingsSave();
    auto const pending = state.consumePendingHotkeyActions();
    LHOLO_CHECK(pending.offsetX == -1);
    LHOLO_CHECK(pending.offsetY == 1);
    LHOLO_CHECK(pending.offsetZ == 0);
    LHOLO_CHECK(pending.layerDelta == -1);
    LHOLO_CHECK(pending.settingsSave);

    state.replaceMaterialRequirements({{"Stone", "minecraft:stone", 12}});
    auto const materials = state.materialRequirements();
    LHOLO_CHECK(materials.size() == 1);
    LHOLO_CHECK(materials[0].typeName == "minecraft:stone");
    LHOLO_CHECK(materials[0].count == 12);

    state.setGuiVisible(false);
    LHOLO_CHECK(state.toggleGuiVisible());
    LHOLO_CHECK(state.guiVisible());
    state.setOpeningInputBlockFrames(1);
    LHOLO_CHECK(state.openingInputBlocked());
    state.consumeOpeningInputBlockFrame();
    LHOLO_CHECK(!state.openingInputBlocked());

    state.setGuiVisible(false);
    state.resetHotkeys();
    state.resetHotkeyState();
    state.clearMaterials();
    state.applyHud(HudStateSnapshot{});
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

void testJavaTextComponents() {
    using lholo::structure::detail::javaTextComponentToPlainText;
    LHOLO_CHECK(javaTextComponentToPlainText(R"("Launch")") == "Launch");
    LHOLO_CHECK(javaTextComponentToPlainText(R"({"text":"X","extra":[{"text":" count"}]})") == "X count");
    LHOLO_CHECK(javaTextComponentToPlainText(R"(["A",{"text":"B"}])") == "AB");
    LHOLO_CHECK(javaTextComponentToPlainText(R"({"translate":"block.minecraft.oak_sign"})")
                == "block.minecraft.oak_sign");
    LHOLO_CHECK(javaTextComponentToPlainText("not json") == "not json");
}

} // namespace

int main() {
    testLayoutRules();
    testProgress();
    testSettingsStore();
    testStructureSession();
    testPlacementState();
    testStructureUiState();
    testHotkeyFormat();
    testJavaTextComponents();
    std::printf("LHoloLogicTests: %d checks, %d failures\n", gChecks, gFailures);
    return gFailures == 0 ? 0 : 1;
}
