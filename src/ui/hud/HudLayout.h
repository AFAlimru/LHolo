#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace lholo::ui::hud {
inline constexpr float kHudMargin = 12.0f;
struct HudStyle {
    float horizontalPadding, verticalPadding, lineGap, rounding;
};
inline HudStyle hudStyle(float lineHeight, bool compactRows = false) {
    // Legacy Fluent HUD: logical font 18, padding 12/8, item gap 8 and
    // rounding 4 * 0.7. Express those proportions in native font/GUI units;
    // do not import ImGui's viewport scale or global theme into native HUD.
    auto const unit = lineHeight / 18.0f;
    // Material HUD uses vanilla's +2 GUI units per row; retain all other
    // legacy panel metrics, including the gap between separate HUD panels.
    return {12.0f * unit, 8.0f * unit, compactRows ? 2.0f : 8.0f * unit, 2.8f * unit};
}
// Reserve icon + gap only on material rows; never enlarge the row height.
inline float hudIconColumn(float lineHeight) {
    return lineHeight + hudStyle(lineHeight).lineGap * 0.7f;
}
struct PanelPlacement {
    float x{}, y{}, width{}, height{};
    std::size_t rows{};
};
// In the game's GUI units. When two panels share a corner the material panel
// follows the projection panel inward; never let either escape the viewport.
inline PanelPlacement placeHudPanel(float viewportWidth, float viewportHeight, float textWidth,
    float lineHeight, std::size_t rows, int position, PanelPlacement const* preceding = nullptr, bool compactRows = false) {
    PanelPlacement result;
    if (!rows || !std::isfinite(viewportWidth) || !std::isfinite(viewportHeight)
        || !std::isfinite(textWidth) || !std::isfinite(lineHeight) || lineHeight <= 0) return result;
    auto const bottom = (position & 1) != 0;
    auto const style = hudStyle(lineHeight, compactRows);
    auto const panelGap = hudStyle(lineHeight).lineGap;
    auto top = kHudMargin;
    auto end = viewportHeight - kHudMargin;
    if (preceding && preceding->rows) {
        if (bottom) end = preceding->y - panelGap;
        else top = preceding->y + preceding->height + panelGap;
    }
    auto const availableHeight = end - top - 2 * style.verticalPadding;
    auto const rowHeight = lineHeight + style.lineGap;
    auto const availableWidth = viewportWidth - 2 * kHudMargin;
    if (availableHeight < lineHeight || availableWidth <= 2 * style.horizontalPadding) return result;
    result.rows = std::min(rows, static_cast<std::size_t>((availableHeight + style.lineGap) / rowHeight));
    result.width = std::clamp(textWidth + 2 * style.horizontalPadding, 2 * style.horizontalPadding, availableWidth);
    result.height = 2 * style.verticalPadding + static_cast<float>(result.rows) * rowHeight - style.lineGap;
    result.x = position >= 2 ? viewportWidth - kHudMargin - result.width : kHudMargin;
    result.y = bottom ? end - result.height : top;
    return result;
}
}
