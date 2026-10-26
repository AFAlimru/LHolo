#include "ui/hud/HudContent.h"
#include "structure/StructureUiState.h"
#include "ui/hud/HudLayout.h"
#include "ui/hud/HudText.h"
#include "ui/hud/editor/HudEditor.h"
#include "ui/MaterialFormat.h"
#include <algorithm>
#include "i18n/LanguageStore.h"
#include <cstdio>
#include <cmath>
#include <limits>

int runNativeHudTests() {
    using namespace lholo;
    using namespace ui::hud;
    int checks{}, failures{};
    auto check = [&](bool ok, char const* message) {
        ++checks;
        if (!ok) { ++failures; std::printf("NativeHudTests: %s\n", message); }
    };
    auto const oldLanguage = i18n::language();
    i18n::setLanguageByCode("en_US");
    structure::detail::HudStateSnapshot settings;
    ProjectionHudData data;
    data.fileName = "name%中文.mcstructure";
    data.layer = 3; data.maxLayer = 7;
    data.progress = {11, 100, 3, 20, 2, 4, 5};
    data.aimedBlock = "Stone"; data.placementMode = "Manual";
    settings.showOverallProgress = true;
    auto panel = makeProjectionHud(settings, data);
    check(panel.lines.size() == 9 && panel.lines[0].text == "File: name%中文.mcstructure", "all ordinary fields, literal filename");
    check(panel.lines[2].text == "Overall: 11 / 100" && panel.lines[3].text == "Progress: 3 / 20", "overall and visible progress remain distinct");
    check(panel.lines[4].tone == TextTone::WrongState && panel.lines[5].tone == TextTone::WrongType
        && panel.lines[6].tone == TextTone::Extra, "error semantics retain their colors");
    check(panel.lines[7].text == "Projected block: Stone" && panel.lines[8].text == "Assist: Manual", "aimed block and assist mode");
    check(std::none_of(panel.lines.begin(), panel.lines.end(), [](auto const& line) { return line.iconId.has_value(); }),
        "ordinary HUD never reserves material icon columns");
    settings.enabled = false;
    check(makeProjectionHud(settings, data).lines.empty(), "ordinary HUD disabled");
    settings.enabled = true;
    settings.showFileName = settings.showOverallProgress = settings.showProgress = settings.showWrongState
        = settings.showWrongType = settings.showExtraBlocks = settings.showProjectedBlockName = false;
    data.placementMode.clear();
    struct Case { structure::LayerAxis axis; structure::LayerDisplayMode mode; char const* expected; };
    using structure::LayerAxis; using structure::LayerDisplayMode;
    for (auto const& value : {
        Case{LayerAxis::Y, LayerDisplayMode::All, "Range: whole structure"},
        Case{LayerAxis::X, LayerDisplayMode::Single, "Current layer: 3 / 7 (X axis)"},
        Case{LayerAxis::Y, LayerDisplayMode::UpToCurrent, "Range: layers 0-3 (Y axis)"},
        Case{LayerAxis::Y, LayerDisplayMode::FromCurrent, "Range: layers 3-7 (Y axis)"},
        Case{LayerAxis::Material, LayerDisplayMode::All, "Material filter: all"},
        Case{LayerAxis::Material, LayerDisplayMode::Single, "Material filter: #4 of 8"},
        Case{LayerAxis::Material, LayerDisplayMode::UpToCurrent, "Material filter: #1–4"},
        Case{LayerAxis::Material, LayerDisplayMode::FromCurrent, "Material filter: #4-8"}}) {
        data.axis = value.axis; data.mode = value.mode;
        panel = makeProjectionHud(settings, data);
        check(panel.lines.size() == 1 && panel.lines.front().text == value.expected, "axis and display-range mapping");
    }
    data.layer = 99; data.mode = LayerDisplayMode::Single;
    check(makeProjectionHud(settings, data).lines[0].text == "Material filter: #8 of 8", "stale current layer clamps to maximum");
    settings.showLayer = false; data.placementMode = "Manual";
    check(makeProjectionHud(settings, data).lines.empty(), "all ordinary fields disabled hides assist-only panel");

    structure::detail::MaterialHudSnapshot material;
    check(makeMaterialHud(material, 3).lines[1].tone == TextTone::Muted, "material scan pending");
    material.ready = true;
    check(makeMaterialHud(material, 3).lines[1].tone == TextTone::Success, "empty ready requirements complete");
    structure::detail::MaterialRequirement stone;
    stone.displayName = "Stone"; stone.count = 111;
    stone.itemId = "minecraft:stone"; stone.typeName = "minecraft:stone";
    auto signs = stone; signs.displayName = "Signs"; signs.count = 18; signs.stackSize = 16;
    signs.itemId = "minecraft:oak_sign"; signs.typeName = "minecraft:standing_sign";
    auto dirt = stone; dirt.displayName = "Dirt"; dirt.count = 3;
    dirt.itemId = "minecraft:dirt"; dirt.typeName = "minecraft:dirt";
    material.requirements = {signs, stone, dirt}; material.available = {1, 0, 4};
    panel = makeMaterialHud(material, 3);
    check(panel.position == 3 && panel.lines.size() == 3 && panel.lines[1].text == "Stone  111 (1 x 64 + 47)"
        && panel.lines[2].text == "Signs  17 (1 x 16 + 1)", "inventory deficit sorting, per-item stack sizes and no satisfied rows");
    check(!panel.lines.front().iconId && panel.lines[1].iconId == "minecraft:stone"
        && panel.lines[2].iconId == "minecraft:oak_sign", "sorted icons follow their material, preferring placement item identity");
    check(panel.lines[1].tone == TextTone::Normal && panel.lines[2].tone == TextTone::Normal,
        "material icons retain default white text");
    auto liquid = dirt; liquid.itemId.clear(); liquid.typeName = "minecraft:water";
    structure::detail::MaterialHudSnapshot liquidSnapshot{{liquid}, {0}, true};
    check(makeMaterialHud(liquidSnapshot, 0).lines[1].iconId == "minecraft:water" && liquid.itemId.empty(),
        "liquid fallback is display-only and does not invent an inventory item");
    check(ui::materialIconId(std::string{"addon:custom_item"}, std::string{"addon:custom_block"}) == "addon:custom_item",
        "shared icon identity retains custom namespaces");
    check(!makeMaterialHud({}, 0).lines[1].iconId
        && !makeMaterialHud({{}, {}, true}, 0).lines[1].iconId, "pending/complete messages have no item icon");
    material.available = {-1};
    check(makeMaterialHud(material, 0).lines.size() == 4, "short availability vector and negative counts stay safe");
    material.requirements.assign(16, dirt); material.available.clear();
    for (std::size_t index = 0; index < material.requirements.size(); ++index)
        material.requirements[index].displayName = "Dirt" + std::to_string(index);
    material.requirements.back().count = 42;
    panel = makeMaterialHud(material, 2);
    check(panel.lines.size() == 16 && panel.lines.back().text == "…additional material types: 2", "14 material rows plus summary");
    check(!panel.lines.back().iconId, "remaining-types summary has no item icon");
    bool stableTopRows = panel.lines[1].text == "Dirt15  42";
    for (std::size_t index = 2; index <= 14; ++index)
        stableTopRows = stableTopRows && panel.lines[index].text == "Dirt" + std::to_string(index - 2) + "  3";
    check(stableTopRows, "top-14 selection includes largest shortage and preserves equal-count tracker order");
    check(!makeMaterialHud(material, 2).lines.empty() && makeProjectionHud(settings, data).lines.empty(), "material HUD independent of ordinary HUD");
    check(makeMaterialHud(material, 2).compactRows && !makeProjectionHud(settings, data).compactRows,
        "only material HUD selects compact rows");

    auto const material100 = placeHudPanel(1000, 1000, 100, 10, 4, 0, nullptr, true);
    auto const material200 = placeHudPanel(1000, 1000, 200, 20, 4, 0, nullptr, true, 2.0f);
    check(std::abs(material200.width - material100.width * 2) < 0.001f
        && std::abs(material200.height - material100.height * 2) < 0.001f,
        "material scale doubles text padding and compact row spacing together");
    auto const largeAtEdge = applyHudPlacement(material200, 1000, 1000, {true, 0.99f, 0.99f});
    check(largeAtEdge.x + largeAtEdge.width <= 1000 && largeAtEdge.y + largeAtEdge.height <= 1000,
        "scaled HUD remains inside viewport at a saved edge position");
    for (int corner = 0; corner < 4; ++corner) {
        auto const primary = placeHudPanel(320, 240, 90, 10, 4, corner);
        auto const secondary = placeHudPanel(320, 240, 120, 10, 16, corner, &primary, true);
        check(primary.rows == 4 && secondary.rows > 0, "both panels fit in every corner");
        check(std::abs(primary.height - 560.0f / 9.0f) < 0.001f, "legacy HUD padding and spacing proportions");
        check(primary.x >= 12 && primary.y >= 12 && secondary.x >= 12 && secondary.y >= 12
            && primary.x + primary.width <= 308 && secondary.y + secondary.height <= 228, "panel viewport bounds");
        check((corner & 1) ? secondary.y + secondary.height + hudStyle(10).lineGap <= primary.y + 0.001f
            : primary.y + primary.height + hudStyle(10).lineGap <= secondary.y + 0.001f, "same-corner panels stack inward without overlap");
        check(corner < 2 ? primary.x == 12 : primary.x + primary.width == 308, "left/right anchoring");
    }
    auto const compact = placeHudPanel(320, 240, 90, 10, 4, 0, nullptr, true);
    auto const moved = applyHudPlacement(compact, 320, 240, {true, 0.5f, 0.25f});
    check(moved.x == 160 && moved.y == 60 && moved.width == compact.width && moved.height == compact.height,
        "custom placement moves without changing HUD size");
    auto const edge = applyHudPlacement(compact, 320, 240, {true, 1, 1});
    check(std::abs(edge.x + edge.width - 320) < 0.001f && std::abs(edge.y + edge.height - 240) < 0.001f, "custom positions reach right and bottom screen edges");
    auto const origin = applyHudPlacement(compact, 320, 240, {true, 0, 0});
    check(origin.x == 0 && origin.y == 0, "custom positions reach top and left screen edges");
    auto const resized = applyHudPlacement(compact, 640, 480, {true, 0.5f, 0.25f});
    check(resized.x == 320 && resized.y == 120, "saved positions follow viewport resize");
    check(std::abs(compact.height - (40.0f + 6.0f + 80.0f / 9.0f)) < 0.001f,
        "compact material rows use two-unit gaps and unchanged outer padding");
    check(placeHudPanel(20, 20, 100, 10, 2, 0).rows == 0, "insufficient viewport skips drawing");
    check(placeHudPanel(320, 240, 10000, 10, 200, 3).width == 296, "long text width bounded");
    check(placeHudPanel(320, 240, 100, std::numeric_limits<float>::quiet_NaN(), 2, 0).rows == 0, "non-finite line height rejected");
    auto const full = placeHudPanel(320, 100, 100, 10, 20, 0);
    check(placeHudPanel(320, 100, 100, 10, 20, 0, &full).rows == 0, "full first panel leaves no overlapping second panel");
    int measurements{};
    auto measure = [&](std::string const& value) {
        ++measurements;
        return std::count_if(value.begin(), value.end(), [](unsigned char c) { return (c & 0xc0) != 0x80; });
    };
    check(fitHudText("石英石英石英石英", 7, measure) == "石英石英...", "text fitting preserves UTF-8 character boundaries");
    check(fitHudText("Stone", 5, measure) == "Stone" && fitHudText("Stone", 2, measure).empty(), "exact fit and too-narrow text areas");
    measurements = 0;
    check(fitHudText(std::string(10000, 'x'), 20, measure) == std::string(17, 'x') + "..."
        && measurements <= 18, "long labels use logarithmic font measurement calls");
    auto& uiState = structure::detail::StructureUiState::getInstance();
    auto const originalPositions = uiState.hudPlacements();
    auto const originalScales = uiState.hudScales();
    uiState.setHudScales({100, 100});
    auto const originalOpacities = uiState.hudBackgroundOpacities();
    uiState.setHudBackgroundOpacities({60, 60});
    uiState.setHudPlacements({});
    shutdownHudLayoutEditor();
    auto publishButtons = [] {
        auto const view = hudEditorSnapshot();
        publishHudEditorButtons({HudEditorBounds{0.2f, 0.92f, 0.15f, 0.06f},
            {0.4f, 0.92f, 0.15f, 0.06f}, {0.6f, 0.92f, 0.15f, 0.06f},
            view.selected >= 0 ? HudEditorBounds{0.1f, 0.05f, 0.25f, 0.06f} : HudEditorBounds{},
            view.selected >= 0 ? HudEditorBounds{0.4f, 0.05f, 0.25f, 0.06f} : HudEditorBounds{},
            view.selected >= 0 ? HudEditorBounds{0.75f, 0.05f, 0.2f, 0.06f} : HudEditorBounds{}},
            view.slider != HudEditorSlider::None ? HudEditorBounds{0.36f, 0.12f, 0.36f, 0.07f} : HudEditorBounds{},
            view.slider != HudEditorSlider::None ? HudEditorBounds{0.4f, 0.14f, 0.2f, 0.04f} : HudEditorBounds{});
    };
    auto beginEditor = [&] {
        processHudLayoutEditor(true, true, true);
        publishHudEditorBounds(0, {0.1f, 0.2f, 0.2f, 0.1f});
        publishHudEditorBounds(1, {0.7f, 0.6f, 0.15f, 0.2f});
        publishButtons();
    };
    auto clickButton = [&](int index) {
        auto const box = hudEditorSnapshot().buttons[index];
        moveHudEditorPointer(box.x + box.width / 2, box.y + box.height / 2);
        pressHudEditorPointer(true); pressHudEditorPointer(false);
    };
    auto dragTo = [](int index, float x, float y) {
        auto const box = hudEditorSnapshot().bounds[index];
        moveHudEditorPointer(box.x + 0.01f, box.y + 0.01f);
        pressHudEditorPointer(true);
        moveHudEditorPointer(x + 0.01f, y + 0.01f);
        pressHudEditorPointer(false);
    };
    check(!processHudLayoutEditor(true, false, true) && !hudLayoutEditorActive(), "native editor requires a world");
    beginEditor();
    check(hudEditorSnapshot().selected == -1 && hudEditorSnapshot().buttons[3].width == 0,
        "single reset is absent until a HUD is selected");
    dragTo(0, -1, -1);
    check(effectiveHudPlacements()[0].x == 0 && effectiveHudPlacements()[0].y == 0
        && effectiveHudPlacements()[1].x == 0.7f, "native pointer drag reaches edge and freezes the other HUD");
    clickButton(1);
    check(!processHudLayoutEditor(false, true, true) && uiState.hudPlacements() == lholo::settings::HudPlacements{},
        "native cancel button discards the draft");
    beginEditor();
    dragTo(0, 2, 2);
    clickButton(2);
    check(processHudLayoutEditor(false, true, true) && uiState.hudPlacements()[0].custom
        && uiState.hudPlacements()[0].x == 0.8f && uiState.hudPlacements()[0].y == 0.9f,
        "native done button commits an in-bounds placement");
    beginEditor();
    auto const preservedMaterial = effectiveHudPlacements()[1];
    moveHudEditorPointer(0.11f, 0.21f); pressHudEditorPointer(true); pressHudEditorPointer(false);
    publishButtons(); clickButton(3); clickButton(2); processHudLayoutEditor(false, true, true);
    check(!uiState.hudPlacements()[0].custom && uiState.hudPlacements()[1] == preservedMaterial,
        "context button resets only the selected HUD");
    lholo::settings::HudPlacements const savedCustom{{{true, 0.3f, 0.4f}, {true, 0.6f, 0.2f}}};
    uiState.setHudPlacements(savedCustom);
    beginEditor(); clickButton(0);
    check(effectiveHudPlacements() == lholo::settings::HudPlacements{} && uiState.hudPlacements() == savedCustom,
        "reset all restores both presets without writing saved preferences");
    clickButton(1); processHudLayoutEditor(false, true, true);
    check(uiState.hudPlacements() == savedCustom, "cancel after reset all preserves saved positions");
    beginEditor();
    moveHudEditorPointer(0.11f, 0.21f); pressHudEditorPointer(true);
    moveHudEditorPointer(0.21f, 0.31f);
    auto const dragged = effectiveHudPlacements()[0];
    pressHudEditorPointer(true); // Native Down may repeat while held.
    releaseHudEditorPointer();
    moveHudEditorPointer(0.5f, 0.5f);
    check(effectiveHudPlacements()[0] == dragged, "focus loss ends dragging without saving or moving again");
    publishButtons();
    moveHudEditorPointer(0.95f, 0.1f); pressHudEditorPointer(true); pressHudEditorPointer(false);
    check(hudEditorSnapshot().selected == -1 && hudEditorSnapshot().buttons[3].width == 0,
        "blank click clears selection and its contextual button");
    clickButton(2); moveHudEditorPointer(0.5f, 0.5f); // finished request remains explicit
    processHudLayoutEditor(false, true, true);
    beginEditor(); clickButton(0);
    auto const resetBox = hudEditorSnapshot().buttons[2];
    moveHudEditorPointer(resetBox.x + 0.01f, resetBox.y + 0.01f); pressHudEditorPointer(true);
    moveHudEditorPointer(0.95f, 0.1f); pressHudEditorPointer(false);
    check(!processHudLayoutEditor(false, true, true) && hudLayoutEditorActive(), "releasing outside done does not commit");
    cancelHudLayoutEditor(); processHudLayoutEditor(false, true, true);
    auto const example = makeHudEditorContent(1, 2);
    check(example.projection.position == 1 && example.materials.position == 2
        && example.projection.lines.size() == 6 && example.materials.lines.size() == 4,
        "editor examples provide both panels without a loaded projection");
    check(example.materials.lines[1].iconId == "minecraft:stone"
        && example.projection.lines[2].text == "Progress: 128 / 337", "examples reuse localized HUD formatters and icon identities");
    beginEditor();
    check(!processHudLayoutEditor(false, true, false) && !hudLayoutEditorActive(), "removing the native scene cancels editing");
    beginEditor();
    moveHudEditorPointer(0.11f, 0.21f); pressHudEditorPointer(true); pressHudEditorPointer(false);
    publishButtons(); clickButton(4); publishButtons();
    check((hudEditorSnapshot().slider != HudEditorSlider::None), "opacity button opens slider popup");
    auto dragOpacity = [](float start, float end) {
        moveHudEditorPointer(start, 0.16f); pressHudEditorPointer(true);
        moveHudEditorPointer(end, 0.16f); pressHudEditorPointer(false);
    };
    auto const beforeSliderPositions = effectiveHudPlacements();
    dragOpacity(0.5f, 0.53f);
    check(effectiveHudBackgroundOpacities() == std::array<int, 2>{65, 60}
        && uiState.hudBackgroundOpacities() == std::array<int, 2>{60, 60}, "slider changes only selected draft at one-percent precision");
    check(effectiveHudPlacements() == beforeSliderPositions, "slider drag leaves HUD positions unchanged");
    moveHudEditorPointer(0.5f, 0.13f); pressHudEditorPointer(true); pressHudEditorPointer(false);
    check(hudEditorSnapshot().selected == 0 && (hudEditorSnapshot().slider != HudEditorSlider::None), "popup percentage preserves selection");
    clickButton(4); publishButtons();
    check(!(hudEditorSnapshot().slider != HudEditorSlider::None) && hudEditorSnapshot().sliderTrack.width == 0, "opacity button toggles slider closed");
    clickButton(2); processHudLayoutEditor(false, true, true);
    check(uiState.hudBackgroundOpacities() == std::array<int, 2>{65, 60}, "done persists selected opacity");
    beginEditor();
    moveHudEditorPointer(0.71f, 0.61f); pressHudEditorPointer(true); pressHudEditorPointer(false);
    publishButtons(); clickButton(4); publishButtons();
    dragOpacity(0.5f, -0.1f);
    check(effectiveHudBackgroundOpacities() == std::array<int, 2>{65, 0}, "slider drag stops at zero outside track");
    dragOpacity(0.5f, 1.1f);
    check(effectiveHudBackgroundOpacities() == std::array<int, 2>{65, 100}, "slider drag stops at one hundred outside track");
    moveHudEditorPointer(0.5f, 0.16f); pressHudEditorPointer(true);
    releaseHudEditorPointer();
    auto const stoppedOpacity = effectiveHudBackgroundOpacities();
    moveHudEditorPointer(0.55f, 0.16f);
    check(effectiveHudBackgroundOpacities() == stoppedOpacity && !hudEditorSnapshot().sliderDragging,
        "focus loss stops slider drag");
    clickButton(3);
    check(effectiveHudBackgroundOpacities() == std::array<int, 2>{65, 60}, "selected reset restores opacity while preserving other HUD");
    clickButton(0);
    check(effectiveHudBackgroundOpacities() == std::array<int, 2>{60, 60}, "reset all restores both default opacities");
    clickButton(1); processHudLayoutEditor(false, true, true);
    check(uiState.hudBackgroundOpacities() == std::array<int, 2>{65, 60}, "cancel discards opacity and reset changes");
    beginEditor();
    moveHudEditorPointer(0.11f, 0.21f); pressHudEditorPointer(true); pressHudEditorPointer(false);
    publishButtons(); clickButton(4); publishButtons();
    moveHudEditorPointer(0.71f, 0.61f); pressHudEditorPointer(true); pressHudEditorPointer(false);
    check(hudEditorSnapshot().selected == 1 && !(hudEditorSnapshot().slider != HudEditorSlider::None), "selecting another HUD closes old slider popup");
    cancelHudLayoutEditor(); processHudLayoutEditor(false, true, true);
    beginEditor();
    moveHudEditorPointer(0.11f, 0.21f); pressHudEditorPointer(true); pressHudEditorPointer(false);
    publishButtons(); clickButton(5); publishButtons();
    check(hudEditorSnapshot().slider == HudEditorSlider::Scale, "scale button opens the shared slider for scale");
    dragOpacity(0.5f, 0.55f);
    check(effectiveHudScales() == std::array<int, 2>{163, 100}
        && uiState.hudScales() == std::array<int, 2>{100, 100}, "scale preview changes only selected draft");
    clickButton(4); publishButtons();
    check(hudEditorSnapshot().slider == HudEditorSlider::Opacity, "opacity button switches the shared popup from scale");
    check(effectiveHudBackgroundOpacities() == std::array<int, 2>{65, 60}, "scale leaves opacity unchanged");
    clickButton(5); publishButtons();
    dragOpacity(0.5f, -1);
    check(effectiveHudScales()[0] == 50, "scale slider clamps at fifty percent");
    dragOpacity(0.5f, 2);
    check(effectiveHudScales()[0] == 200, "scale slider clamps at two hundred percent");
    clickButton(2); processHudLayoutEditor(false, true, true);
    check(uiState.hudScales() == std::array<int, 2>{200, 100}, "done saves independent scale");
    beginEditor();
    moveHudEditorPointer(0.11f, 0.21f); pressHudEditorPointer(true); pressHudEditorPointer(false);
    publishButtons(); clickButton(3);
    check(effectiveHudScales()[0] == 100 && effectiveHudBackgroundOpacities()[0] == 60
        && !effectiveHudPlacements()[0].custom, "selected reset restores position opacity and scale");
    clickButton(1); processHudLayoutEditor(false, true, true);
    check(uiState.hudScales()[0] == 200, "cancel discards scale reset");
    beginEditor(); clickButton(0);
    check(effectiveHudScales() == std::array<int, 2>{100, 100}, "reset all restores both default scales");
    cancelHudLayoutEditor(); processHudLayoutEditor(false, true, true);
    shutdownHudLayoutEditor();
    uiState.setHudScales(originalScales);
    uiState.setHudBackgroundOpacities(originalOpacities);
    uiState.setHudPlacements(originalPositions);
    i18n::setLanguage(oldLanguage);
    std::printf("NativeHudTests: %d checks, %d failures\n", checks, failures);
    return failures;
}
