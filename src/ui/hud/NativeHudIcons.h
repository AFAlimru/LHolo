#pragma once

#include <cstdint>
#include <span>
#include <string>

class UIRenderContext;
class ClientInstance;

namespace lholo::ui::hud {
struct HudIconDraw {
    std::string id;
    float x{}, y{}, size{};
};
bool installNativeHudIcons();
void uninstallNativeHudIcons();
void clearNativeHudIcons();
// Flushes the HUD's text once. Icons use the concrete game's matching text
// flush callback, avoiding an unchecked downcast from UIRenderContext.
void flushHudTextAndIcons(UIRenderContext& context, ClientInstance& client,
    std::uint64_t generation, std::span<HudIconDraw const> icons);
}
