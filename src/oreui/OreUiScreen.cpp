#include "oreui/OreUiScreen.h"
#include "oreui/OreUiMenuResources.h"
#include "input/HotkeyCapture.h"

#include "oreui/OreUiBridge.h"
#include "plugin/LHolo.h"
#include "structure/StructureLoader.h"
#include "ll/api/memory/Hook.h"
#include "ll/api/mod/NativeMod.h"
#include "ll/api/service/Bedrock.h"
#include "mc/client/game/ClientInstance.h"
#include "mc/client/game/ClientInstanceState.h"
#include "mc/client/gui/oreui/interface/RouteMode.h"
#include "mc/client/gui/oreui/interface/RoutePrerequisite.h"
#include "mc/client/gui/oreui/interface/RouteType.h"
#include "mc/client/gui/oreui/routing/RouteAction.h"
#include "mc/client/gui/oreui/routing/RouteHistoryAction.h"
#include "mc/client/gui/oreui/routing/RouteMatcher.h"
#include "mc/client/gui/oreui/routing/Router.h"
#include "mc/client/gui/screens/AbstractScene.h"
#include "mc/client/gui/screens/SceneFactory.h"
#include "mc/client/gui/screens/interfaces/ISceneStack.h"
#include "mc/deps/input/KeyboardEventProcessor.h"
#include "mc/deps/input/win/HIDControllerGameCoreDesktop.h"
#include "mc/network/LoopbackPacketSender.h"
#include "mc/network/MinecraftPacketIds.h"
#include "mc/network/Packet.h"
#include "mc/network/packet/TextPacket.h"

#include <atomic>
#include <chrono>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>
#include <variant>

namespace lholo::oreui {
namespace {
std::atomic_bool active{}, openPending{}, closePending{}, menuOnTop{}, escapeHeld{};
bool sendInstalled{}, serverInstalled{}, downInstalled{}, upInstalled{};
auto& logger() { return LHolo::getInstance().getSelf().getLogger(); }
bool isMenuScene(std::string const& name) { return name.find("lholo-menu.html") != std::string::npos; }

bool isMenuCommand(std::string_view message) {
    constexpr std::string_view command{"lholo"};
    if (message.size() != command.size()) return false;
    for (std::size_t index = 0; index < command.size(); ++index) {
        auto character = message[index];
        if (character >= 'A' && character <= 'Z') character += 'a' - 'A';
        if (character != command[index]) return false;
    }
    return true;
}

bool filterMenuCommand(Packet& packet) {
    if (!active.load() || packet.getId() != MinecraftPacketIds::Text) return false;
    auto const& text = static_cast<TextPacket&>(packet);
    auto const* chat = std::get_if<TextPacketPayload::AuthorAndMessage>(&text.mBody.get());
    if (!chat || chat->mType != TextPacketType::Chat || !isMenuCommand(chat->mMessage.get())) return false;
    auto client = ll::service::getClientInstance();
    if (!client || client->$getClientInstanceState() != ClientInstanceState::Playing) return true;
    ::Bedrock::NonOwnerPointer<ISceneStack> stack = client->$getClientSceneStack();
    auto* top = stack ? stack->getTopScene() : nullptr;
    if (top && top->getScreenName() == "chat_screen") stack->schedulePopScreen(1);
    openPending = true;
    return true;
}

bool resourcesAvailable() {
    // Prefer the mod-owned pack. Never silently fall back when a pack is installed:
    // that could hide a failed resource-pack mount behind old development files.
    auto const pack = LHolo::getInstance().getSelf().getResourceDir() / "LHoloUI";
    auto const packaged = std::filesystem::exists(pack / "manifest.json");
    auto const root = packaged ? pack / "hbui"
                              : std::filesystem::current_path() / "data/gui/dist/hbui";
    for (auto const* file : {"lholo-menu.html", "lholo-menu.js"}) {
        std::ifstream input(root / file, std::ios::binary);
        std::string const contents{std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
        if (contents.find("lholo-menu-v7-settings") == std::string::npos) {
            logger().error("[oreui] Missing or incompatible {}. Build and deploy the LHolo OreUI assets first.", (root / file).string());
            return false;
        }
    }
    return true;
}

bool ensureRoute(OreUI::Router& router) {
    auto const* base = router.mRouteMatcher.get();
    if (!base) return false;
    auto& matcher = *const_cast<OreUI::RouteMatcher*>(static_cast<OreUI::RouteMatcher const*>(base));
    for (auto const& route : matcher.mRoutes.get()) {
        if (route.pattern.get() == kMenuRoute) return true;
    }
    matcher.addRoute(kMenuAsset, kMenuRoute, OreUI::RouteType::OreUI,
        OreUI::RouteMode::NoLeaveDelay, OreUI::RoutePrerequisite::None,
        [] { return active.load(); },
        [](std::string const& route, OreUI::RouteHistoryAction action) {
            auto client = ll::service::getClientInstance();
            if (!active.load() || !client) return;
            auto scene = client->$getSceneFactory().createSceneFromUrl(kMenuAsset, OreUI::RouteMode::NoLeaveDelay);
            ::Bedrock::NonOwnerPointer<ISceneStack> stack = client->$getClientSceneStack();
            if (scene && stack) {
                OreUI::RouteAction const routeAction{route, action};
                stack->pushScreenWithRouteAction(scene, routeAction);
            }
        });
    return true;
}

LL_TYPE_INSTANCE_HOOK(MenuSendToServer, ll::memory::HookPriority::Normal,
    LoopbackPacketSender, &LoopbackPacketSender::$sendToServer, void, Packet& packet) {
    if (!filterMenuCommand(packet)) origin(packet);
}
LL_TYPE_INSTANCE_HOOK(MenuSend, ll::memory::HookPriority::Normal,
    LoopbackPacketSender, &LoopbackPacketSender::$send, void, Packet& packet) {
    if (!filterMenuCommand(packet)) origin(packet);
}
LL_TYPE_INSTANCE_HOOK(MenuEscapeDown, ll::memory::HookPriority::Highest,
    HIDControllerGameCoreDesktop, &HIDControllerGameCoreDesktop::$onKeyDown, void,
    int key, Bedrock::Input::KeyboardEventProcessor::InputOrigin source) {
    if (key == 27 && active.load() && menuOnTop.load()) {
        if (input::hotkeyCaptureActive()) { escapeHeld = true; input::captureHotkeyKey(27, 0); return; }
        // Claim the whole key pair, including auto-repeat after scheduling pop.
        if (!escapeHeld.exchange(true)) requestMenuClose();
        return;
    }
    if (key == 27 && escapeHeld.load()) return;
    origin(key, source);
}
LL_TYPE_INSTANCE_HOOK(MenuEscapeUp, ll::memory::HookPriority::Highest,
    HIDControllerGameCoreDesktop, &HIDControllerGameCoreDesktop::$onKeyUp, void, int key) {
    if (key == 27 && escapeHeld.exchange(false)) return;
    origin(key);
}
}

bool isMenuOnTop() { return menuOnTop.load(); }
void requestMenuOpen() { if (active.load()) openPending = true; }
void requestMenuClose() { closePending = true; }

void processMenuScreen() {
    if (!active.load()) return;
    static std::chrono::steady_clock::time_point waitingSince{};
    auto client = ll::service::getClientInstance();
    if (!client || client->$getClientInstanceState() != ClientInstanceState::Playing) {
        openPending = closePending = menuOnTop = false;
        resetMenuBridge();
        waitingSince = {};
        return;
    }
    ::Bedrock::NonOwnerPointer<ISceneStack> stack = client->$getClientSceneStack();
    if (!stack) return;
    auto* top = stack->getTopScene();
    auto const name = top ? top->getScreenName() : std::string{};
    menuOnTop = isMenuScene(name);
    if (closePending.exchange(false)) {
        if (menuOnTop.load()) {
            stack->schedulePopScreen(1);
            resetMenuBridge();
        }
        return;
    }
    if (!openPending.load()) { waitingSince = {}; return; }
    auto const now = std::chrono::steady_clock::now();
    if (waitingSince.time_since_epoch().count() == 0) waitingSince = now;
    if (name == "chat_screen") {
        if (now - waitingSince > std::chrono::seconds{5}) {
            openPending = false;
            waitingSince = {};
            logger().warn("[oreui] chat did not close; navigation cancelled");
        }
        return;
    }
    openPending = false;
    waitingSince = {};
    if (menuOnTop.load() || structure::isMenuInputCaptured()) return;
    if (!resourcesAvailable()) return;
    auto& router = client->$getCurrentUIRouter();
    if (!ensureRoute(router)) return;
    resetMenuBridge();
    auto const pushed = router._pushRoute(kMenuRoute, OreUI::Router::RouterPushMode::Normal);
    menuOnTop = pushed;
    structure::resetHotkeyState();
    if (!pushed) logger().error("[oreui] Failed to open the LHolo menu route");
}

bool installMenuScreen() {
    if (active.load()) return true;
    if (!installMenuResources()) return false;
    serverInstalled = MenuSendToServer::hook() == 0;
    sendInstalled = MenuSend::hook() == 0;
    downInstalled = MenuEscapeDown::hook() == 0;
    upInstalled = MenuEscapeUp::hook() == 0;
    if (!(serverInstalled && sendInstalled && downInstalled && upInstalled)) {
        uninstallMenuScreen();
        return false;
    }
    active = true;
    return true;
}

void uninstallMenuScreen() {
    // Called before bridge teardown; pop only our own top scene.
    if (active.load()) {
        closePending = true;
        processMenuScreen();
    }
    active = false;
    uninstallMenuResources();
    if (upInstalled) MenuEscapeUp::unhook();
    if (downInstalled) MenuEscapeDown::unhook();
    if (sendInstalled) MenuSend::unhook();
    if (serverInstalled) MenuSendToServer::unhook();
    sendInstalled = serverInstalled = downInstalled = upInstalled = false;
    openPending = closePending = menuOnTop = escapeHeld = false;
}
}
