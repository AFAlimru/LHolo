#include "ui/hud/NativeHudRenderer.h"
#include "ui/hud/NativeHudPainter.h"
#include "ui/hud/NativeHudIcons.h"
#include "ui/hud/editor/HudEditor.h"
#include "ll/api/memory/Hook.h"
#include "ll/api/mod/NativeMod.h"
#include "ll/api/service/Bedrock.h"
#include "plugin/LHolo.h"
#include "mc/client/game/ClientInstance.h"
#include "mc/client/game/ClientInstanceState.h"
#include "mc/client/gui/screens/ScreenView.h"
#include <atomic>
#include <exception>

namespace lholo::ui::hud {
namespace {
std::atomic_bool active{}, drawFailed{};
bool installed{};
LL_TYPE_INSTANCE_HOOK(NativeHudScreenRender, ll::memory::HookPriority::Normal,
    ScreenView, &ScreenView::render, void, UIRenderContext& context) {
    origin(context);
    if (!active.load() || drawFailed.load() || hudLayoutEditorActive() || getScreenName() != "hud_screen") return;
    auto client = ll::service::getClientInstance();
    if (!client || client->$getClientInstanceState() != ClientInstanceState::Playing || !client->getLocalPlayer()) {
        clearNativeHudIcons(); return;
    }
    try { paintNativeHud(context, client.get(), readHudContent(), effectiveHudPlacements(), effectiveHudBackgroundOpacities(), effectiveHudScales()); }
    catch (std::exception const& error) {
        if (!drawFailed.exchange(true)) LHolo::getInstance().getSelf().getLogger().error("[native-hud] drawing stopped after error: {}", error.what());
    }
}
}
bool installNativeHudRenderer() {
    if (active.load()) return true;
    drawFailed = false;
    installed = NativeHudScreenRender::hook() == 0;
    active = installed;
    if (installed && !installNativeHudIcons()) LHolo::getInstance().getSelf().getLogger().warn("[native-hud.icons] hooks unavailable; keeping text HUD");
    return installed;
}
void uninstallNativeHudRenderer() {
    active = false;
    if (installed) NativeHudScreenRender::unhook();
    uninstallNativeHudIcons();
    installed = false;
}
}
