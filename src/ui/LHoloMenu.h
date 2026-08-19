// LHolo - Fluent-style menu
#pragma once

#include "ui/FluentTheme.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace lholo::ui {

enum class MenuPage : std::uint8_t { Projection, Transform, Render, Hotkeys, Hud, UiScale, Experimental };

enum class HotkeyId : std::uint8_t {
    Gui, MoveXMinus, MoveXPlus, MoveZMinus, MoveZPlus,
    MoveYPlus, MoveYMinus, LayerIncrease, LayerDecrease
};

struct HotkeyRow {
    HotkeyId    id{};
    std::string label;
    std::string display;
    bool        capturing{};
};

struct MaterialRow {
    std::string displayName;
    std::string typeName;
    std::uint64_t count{};
};

struct MenuModel {
    MenuPage page{MenuPage::Projection};
    char* pathBuffer{};
    std::size_t pathBufferSize{};
    bool blockOpeningInput{};
    std::string status;
    bool hasLoadedStructure{};
    bool hasSavedProjection{};
    int savedAnchorX{};
    int savedAnchorY{};
    int savedAnchorZ{};
    float uiScale{1.0f};

    bool structureBoundsEnabled{};
    bool easyPlaceEnabled{};
    bool manualPlace{};
    bool rangeEnabled{};
    int placementRadius{4};
    int offsetX{};
    int offsetY{};
    int offsetZ{};
    int rotation{};
    int mirror{};

    float opacity{1.0f};
    float correctionFillOpacity{0.15f};
    float correctionOutlineOpacity{1.0f};
    int layerAxis{};
    int layerDisplayMode{};
    int displayLayer{};
    int maxLayerY{};
    int maxLayerX{};

    std::array<HotkeyRow, 9> hotkeys{};
    bool hudEnabled{true};
    int hudPosition{1};
    bool hudShowFileName{true};
    bool hudShowLayer{true};
    bool hudShowOverallProgress{false};
    bool hudShowProgress{true};
    bool hudShowWrongState{true};
    bool hudShowWrongType{true};
    bool hudShowBlockEntity{true};

    std::vector<MaterialRow> materials;
    bool materialPopupRequested{};
    bool closeRequested{};
};

struct MenuActions {
    std::function<std::optional<std::string>(std::string_view)> browseStructure;
    std::function<void(std::string_view)> loadStructure;
    std::function<void()> restoreProjection;
    std::function<void()> closeProjection;
    std::function<void()> requestMaterials;
    std::function<void(HotkeyId)> beginHotkeyCapture;
    std::function<void(HotkeyId)> clearHotkey;
    std::function<void()> resetHotkeys;
    std::function<void()> resetCorrectionStyle;
};

void renderMenu(MenuModel& model, MenuActions const& actions, UiMetrics const& metrics);

} // namespace lholo::ui
