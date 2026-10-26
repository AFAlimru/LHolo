#include "ui/hud/NativeHudRenderer.h"
#include "ui/hud/HudContent.h"
#include "ui/hud/HudLayout.h"
#include "ui/hud/HudText.h"
#include "ui/hud/NativeHudIcons.h"
#include "ll/api/memory/Hook.h"
#include "ll/api/mod/NativeMod.h"
#include "ll/api/service/Bedrock.h"
#include "plugin/LHolo.h"
#include "mc/client/game/ClientInstance.h"
#include "mc/client/game/ClientInstanceState.h"
#include "mc/client/gui/CaretMeasureData.h"
#include "mc/client/gui/Font.h"
#include "mc/client/gui/FontHandle.h"
#include "mc/client/gui/TextAlignment.h"
#include "mc/client/gui/TextMeasureData.h"
#include "mc/client/gui/controls/UIRenderContext.h"
#include "mc/client/gui/screens/ScreenView.h"
#include "mc/deps/core/math/Color.h"
#include "mc/deps/input/RectangleArea.h"
#include <algorithm>
#include <atomic>
#include <cmath>
#include <exception>
#include <string>
#include <utility>
#include <vector>

namespace lholo::ui::hud {
namespace {
// User-selected opacity, shared by both native HUD panels.
constexpr float backgroundAlpha = 0.6f;
constexpr float fontScale = 1.0f;
std::atomic_bool active{}, drawFailed{};
bool installed{};
thread_local bool drawing{};
auto& logger() { return LHolo::getInstance().getSelf().getLogger(); }

mce::Color color(TextTone tone) {
    switch (tone) {
    case TextTone::Muted: return {0.65f, 0.65f, 0.65f, 1.0f};
    case TextTone::Success: return {0.55f, 0.85f, 0.40f, 1.0f};
    case TextTone::WrongState: return {1.0f, 0.62f, 0.18f, 1.0f};
    case TextTone::WrongType: return {1.0f, 0.28f, 0.24f, 1.0f};
    case TextTone::Extra: return {1.0f, 0.30f, 0.90f, 1.0f};
    case TextTone::Placement: return {0.45f, 0.85f, 1.0f, 1.0f};
    default: return {1.0f, 1.0f, 1.0f, 1.0f};
    }
}

// Truncate at UTF-8 boundaries before submission, so a long file/material name
// cannot wrap over the following native row or escape the panel background.
std::string fitLine(UIRenderContext& context, Font& font, std::string text, float width) {
    auto measure = [&](std::string const& value) { return context.getLineLength(font, value, fontScale, false); };
    return fitHudText(std::move(text), width, measure);
}

void fillPanelBackground(UIRenderContext& context, float x, float y, float width, float height, float rounding) {
    auto const radius = std::min({rounding, width * 0.5f, height * 0.5f});
    mce::Color const background{0.075f, 0.075f, 0.075f, 1.0f};
    // UIRenderContext exposes fillRectangle, not a rounded-rectangle primitive.
    // Disjoint horizontal bands form the corners without alpha accumulating
    // from overlapping rectangles, and need no texture or extra render backend.
    auto const steps = std::max(1, static_cast<int>(std::ceil(radius * 2.0f)));
    auto const stepHeight = radius / static_cast<float>(steps);
    for (int index = 0; index < steps; ++index) {
        auto const offset = static_cast<float>(index) * stepHeight;
        auto const circleY = radius - offset - stepHeight * 0.5f;
        auto const inset = radius - std::sqrt(std::max(0.0f, radius * radius - circleY * circleY));
        context.fillRectangle({x + inset, x + width - inset, y + offset, y + offset + stepHeight}, background, backgroundAlpha);
        context.fillRectangle({x + inset, x + width - inset, y + height - offset - stepHeight, y + height - offset}, background, backgroundAlpha);
    }
    if (height > 2 * radius)
        context.fillRectangle({x, x + width, y + radius, y + height - radius}, background, backgroundAlpha);
}

PanelPlacement drawPanel(UIRenderContext& context, Font& font, RectangleArea const& viewport,
    float lineHeight, HudPanel panel, std::vector<HudIconDraw>& icons, PanelPlacement const* preceding = nullptr) {
    float textWidth{};
    for (auto const& line : panel.lines)
        textWidth = std::max(textWidth, static_cast<float>(context.getLineLength(font, line.text, fontScale, false))
            + (line.iconId ? hudIconColumn(lineHeight) : 0.0f));
    auto const placement = placeHudPanel(viewport._x1 - viewport._x0, viewport._y1 - viewport._y0,
        textWidth, lineHeight, panel.lines.size(), panel.position, preceding, panel.compactRows);
    if (!placement.rows) return placement;
    auto const x = viewport._x0 + placement.x;
    auto const y = viewport._y0 + placement.y;
    auto const style = hudStyle(lineHeight, panel.compactRows);
    fillPanelBackground(context, x, y, placement.width, placement.height, style.rounding);
    TextMeasureData const textData{fontScale, 0.0f, true, false, false, ::ui::TextAlignment::Left};
    CaretMeasureData const caret{-1, false};
    for (std::size_t index = 0; index < placement.rows; ++index) {
        auto& line = panel.lines[index];
        if (index + 1 == placement.rows && placement.rows < panel.lines.size()) line = {"...", TextTone::Muted};
        auto const iconColumn = line.iconId ? hudIconColumn(lineHeight) : 0.0f;
        auto const availableWidth = placement.width - 2 * style.horizontalPadding;
        auto const showIcon = line.iconId && availableWidth > iconColumn;
        auto const textOffset = showIcon ? iconColumn : 0.0f;
        auto text = fitLine(context, font, std::move(line.text), availableWidth - textOffset);
        auto const rowHeight = lineHeight + style.lineGap;
        auto const top = y + style.verticalPadding + static_cast<float>(index) * rowHeight;
        if (showIcon) icons.push_back({*line.iconId, x + style.horizontalPadding, top, lineHeight});
        context.drawText(font, {x + style.horizontalPadding + textOffset, x + placement.width - style.horizontalPadding, top, top + lineHeight},
            std::move(text), color(line.tone), 1.0f, ::ui::TextAlignment::Left, textData, caret);
    }
    return placement;
}

void drawHud(UIRenderContext& context, ClientInstance& client) {
    auto content = readHudContent();
    if (content.projection.lines.empty() && content.materials.lines.empty()) {
        clearNativeHudIcons();
        return;
    }
    // Borrow font/context only within this render call. No native pointers or
    // geometry from an earlier world, frame or dimension are retained.
    auto fontHandle = client.$getFontHandle();
    auto& font = fontHandle.getFont();
    auto const viewport = context.getFullClippingRectangle();
    auto const lineHeight = std::max(10.0f, font.getWrapHeight()) * fontScale;
    bool const sameCorner = content.projection.position == content.materials.position;
    std::vector<HudIconDraw> icons;
    auto const projection = drawPanel(context, font, viewport, lineHeight, std::move(content.projection), icons);
    auto const materials = drawPanel(context, font, viewport, lineHeight, std::move(content.materials), icons, sameCorner ? &projection : nullptr);
    if (!projection.rows && !materials.rows) return;
    // origin() has already flushed vanilla drawing. Submit our appended text
    // now, without advancing the obfuscated-font clock a second time.
    flushHudTextAndIcons(context, client, content.context, icons);
}

LL_TYPE_INSTANCE_HOOK(NativeHudScreenRender, ll::memory::HookPriority::Normal,
    ScreenView, &ScreenView::render, void, UIRenderContext& context) {
    origin(context);
    if (!active.load() || drawing || drawFailed.load()) return;
    auto client = ll::service::getClientInstance();
    if (!client || client->$getClientInstanceState() != ClientInstanceState::Playing || !client->getLocalPlayer()) {
        clearNativeHudIcons();
        return;
    }
    try {
        if (getScreenName() != "hud_screen") return;
        drawing = true;
        struct DrawingScope { ~DrawingScope() { drawing = false; } } scope;
        drawHud(context, client.get());
    } catch (std::exception const& error) {
        if (!drawFailed.exchange(true)) logger().error("[native-hud] drawing stopped after error: {}", error.what());
    }
}
}

bool installNativeHudRenderer() {
    if (active.load()) return true;
    drawFailed = false;
    installed = NativeHudScreenRender::hook() == 0;
    active = installed;
    if (installed && !installNativeHudIcons()) logger().warn("[native-hud.icons] hooks unavailable; keeping text HUD");
    if (installed) logger().info("[native-hud] installed for projection and material HUD");
    return installed;
}
void uninstallNativeHudRenderer() {
    active = false;
    if (installed) NativeHudScreenRender::unhook();
    uninstallNativeHudIcons();
    installed = false;
}
}
