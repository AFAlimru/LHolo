#pragma once

#include <cstdint>
#include <span>
#include <memory>
#include <string>

class UIRenderContext;
class ItemStack;
class ClientInstance;

namespace lholo::ui::hud {
struct HudIconDraw {
    std::string id;
    float x{}, y{}, size{};
    std::shared_ptr<ItemStack const> item;
};
bool installNativeHudIcons();
void uninstallNativeHudIcons();
void clearNativeHudIcons();
// Called on the client update thread while the native editor may pause world ticks.
void prepareNativeHudIconItems();
// Flushes the HUD's text once. Icons use the concrete game's matching text
// flush callback, avoiding an unchecked downcast from UIRenderContext.
void flushHudTextAndIcons(UIRenderContext& context, ClientInstance& client,
    std::uint64_t generation, std::span<HudIconDraw const> icons);
}
