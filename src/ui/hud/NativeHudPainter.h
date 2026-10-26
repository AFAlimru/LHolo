#pragma once
#include "ui/hud/HudContent.h"
#include "ui/hud/HudLayout.h"
#include <array>
class ClientInstance;
class UIRenderContext;
namespace lholo::ui::hud {
void paintHudBackground(UIRenderContext& context, PanelPlacement const& bounds, float rounding,
    float opacity);
struct NativeHudFrame {
    float width{}, height{}, lineHeight{};
    std::array<PanelPlacement, 2> panels{};
};
NativeHudFrame paintNativeHud(UIRenderContext& context, ClientInstance& client,
    HudContent content, settings::HudPlacements const& placements, std::array<int, 2> const& opacities, std::array<int, 2> const& scales);
}
