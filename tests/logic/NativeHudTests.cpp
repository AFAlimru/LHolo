#include "ui/hud/HudContent.h"
#include "structure/StructureUiState.h"
#include "ui/hud/HudLayout.h"
#include "ui/hud/HudText.h"
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
    i18n::setLanguage(oldLanguage);
    std::printf("NativeHudTests: %d checks, %d failures\n", checks, failures);
    return failures;
}
