#include "oreui/OreUiBridge.h"

#include "oreui/MenuBindings.h"
#include "oreui/CaptureBindings.h"
#include "oreui/TransformBindings.h"
#include "oreui/RenderSettingsBindings.h"
#include "oreui/SettingsBindings.h"
#include "i18n/Translator.h"
#include "mc/client/gui/oreui/binding/FacetRegistry.h"
#include "oreui/IntentFacet.h"
#include "oreui/OreUiScreen.h"
#include "plugin/LHolo.h"
#include "ll/api/memory/Hook.h"
#include "ll/api/mod/NativeMod.h"
#include "mc/client/gui/oreui/binding/Binder.h"
#include "mc/client/gui/oreui/binding/QueryRegistry.h"
#include "mc/client/gui/oreui/binding/ViewContextFactory.h"
#include "mc/client/gui/oreui/binding/interface/IHandlerFactory.h"
#include "mc/client/gui/oreui/views/View.h"
#include "mc/external/gameface/cohtml/IEventHandler.h"

#include <atomic>
#include <exception>
#include <memory>
#include <new>
#include <string>

namespace lholo::oreui {
namespace {
std::atomic_bool active{}, labelsPending{}, browsePending{}, fileStatePending{};
std::atomic_bool loadPending{}, closeProjectionPending{}, restorePending{}, projectionStatePending{};
bool registered{}, viewInstalled{}, intentInstalled{};
auto& logger() { return LHolo::getInstance().getSelf().getLogger(); }
enum class MenuQuery { Ready, Close, BrowseStructure, LoadProjection, CloseProjection, RestoreProjection, CancelHotkeyCapture };
std::optional<i18n::Language> publishedLanguage;

// Typed facet activation is the supported scalar-payload entry point. No
// layout assumptions about the opaque cohtml::ArgumentsBinder are required.
using FacetPayload = std::unordered_map<std::string, std::variant<double, bool, std::string>>;
bool receiveCaptureIntent(std::string_view request) {
    if (!active.load() || !isMenuOnTop()) return false;
    return queueCaptureIntent(request);
}
bool receiveTransformIntent(std::string_view request) {
    return active.load() && isMenuOnTop() && queueTransformIntent(request);
}
bool receiveRenderSettingsIntent(std::string_view request) {
    return active.load() && isMenuOnTop() && queueRenderSettingsIntent(request);
}
bool receiveSettingsIntent(std::string_view request) {
    return active.load() && isMenuOnTop() && queueSettingsIntent(request);
}
LL_TYPE_INSTANCE_HOOK(MenuIntentActivation, ll::memory::HookPriority::Normal,
    OreUI::FacetRegistry, &OreUI::FacetRegistry::$activateFacet,
    std::optional<OreUI::IFacetRegistry::Error>, OreUI::FacetBinder& binder,
    std::string const& name, std::string const& id, FacetPayload const& payload) {
    if (name == "lholo.capture.intent" || name == "lholo.transform.intent" || name == "lholo.render.intent" || name == "lholo.settings.intent") {
        if (!active.load() || !isMenuOnTop()) return OreUI::IFacetRegistry::Error::ActivateNotFound;
        bool known{};
        for (auto const& prototype : mFacetPrototypes.get()) if (prototype.name.get() == name) known = true;
        if (!known) {
            if (name == "lholo.capture.intent")
                $registerFacet(name, [] { return std::make_unique<IntentFacet>(receiveCaptureIntent); });
            else if (name == "lholo.transform.intent")
                $registerFacet(name, [] { return std::make_unique<IntentFacet>(receiveTransformIntent, "lholo.transform.intent"); });
            else if (name == "lholo.render.intent")
                $registerFacet(name, [] { return std::make_unique<IntentFacet>(receiveRenderSettingsIntent, "lholo.render.intent"); });
            else $registerFacet(name, [] { return std::make_unique<IntentFacet>(receiveSettingsIntent, "lholo.settings.intent"); });
        }
    }
    return origin(binder, name, id, payload);
}

class MenuHandler final : public cohtml::IEventHandler {
public:
    explicit MenuHandler(MenuQuery query) : mQuery(query) {}
    void Invoke(cohtml::ArgumentsBinder*) override {
        if (!active.load()) return;
        switch (mQuery) {
        case MenuQuery::Close: requestMenuClose(); break;
        case MenuQuery::Ready: labelsPending = fileStatePending = projectionStatePending = true; break;
        case MenuQuery::BrowseStructure: browsePending = true; break;
        case MenuQuery::LoadProjection: loadPending = true; break;
        case MenuQuery::CloseProjection: closeProjectionPending = true; break;
        case MenuQuery::RestoreProjection: restorePending = true; break;
        case MenuQuery::CancelHotkeyCapture: if (isMenuOnTop()) cancelSettingsHotkeyCapture(); break;
        }
    }
    void const* GetTarget() override { return nullptr; }
    void SetTarget(void*) override {}
private:
    MenuQuery mQuery;
};
class MenuHandlerFactory final : public OreUI::Detail::IHandlerFactory {
public:
    explicit MenuHandlerFactory(MenuQuery query) : mQuery(query) {}
    cohtml::IEventHandler* makeHandler() const override { return new (std::nothrow) MenuHandler{mQuery}; }
private:
    MenuQuery mQuery;
};

LL_TYPE_INSTANCE_HOOK(MenuViewUpdate, ll::memory::HookPriority::Normal,
    OreUI::View, &OreUI::View::$update, void, double time) {
    origin(time);
    if (!active.load()) return;
    try {
        auto const menuView = mUrl.get().find("lholo-menu.html") != std::string::npos;
        bool load{}, close{}, restore{};
        // Accept business intents before scheduling a menu pop. Once accepted,
        // loading belongs to the structure session and survives closing the UI.
        if (menuView && isMenuOnTop()) {
            load = loadPending.exchange(false);
            close = closeProjectionPending.exchange(false);
            restore = restorePending.exchange(false);
            if (close) closeActiveProjection();
            else if (restore) restoreLastProjection();
            else if (load) loadSelectedProjection();
        }
        processMenuScreen();
        if (!menuView || !isMenuOnTop()) return;
        if (labelsPending.exchange(false) || publishedLanguage != i18n::language()) {
            publishedLanguage = i18n::language();
            $triggerEvent("lholo:menu-labels", buildMenuLabels());
        }
        if (browsePending.exchange(false)) requestStructureFileBrowse();
        auto const forceProjection = projectionStatePending.exchange(false) || load || close || restore;
        if (auto update = takeTransformUpdate(forceProjection)) $triggerEvent("lholo:transform", *update);
        if (auto update = takeRenderSettingsUpdate(forceProjection)) $triggerEvent("lholo:render", *update);
        if (auto update = takeSettingsUpdate(forceProjection)) $triggerEvent("lholo:settings", *update);
        if (auto update = takeCaptureUpdate(forceProjection)) {
            $triggerEvent("lholo:capture", *update);
        }
        if (auto update = takeProjectionUpdate(forceProjection)) {
            $triggerEvent("lholo:projection", *update);
        }
        if (auto update = takeStructureFileUpdate(fileStatePending.exchange(false))) {
            $triggerEvent("lholo:structure-file", *update);
        }
    } catch (std::exception const& error) {
        logger().error("[oreui] menu update failed: {}", error.what());
    }
}


}

bool installMenuBridge() {
    if (active.load()) return true;
    viewInstalled = MenuViewUpdate::hook() == 0;
    if (!viewInstalled) return false;
    intentInstalled = MenuIntentActivation::hook() == 0;
    if (!intentInstalled) { uninstallMenuBridge(); return false; }
    if (!registered) {
        OreUI::Detail::ViewContextFactory::_queryStorage().push_back(
            [](OreUI::Detail::QueryRegistry& registry, OreUI::Detail::ViewContextFactory&) {
                struct QueryDefinition { char const* name; MenuQuery action; };
                for (auto const& definition : {
                    QueryDefinition{"lholo.menu.ready", MenuQuery::Ready},
                    QueryDefinition{"lholo.menu.close", MenuQuery::Close},
                    QueryDefinition{"lholo.structure.browse", MenuQuery::BrowseStructure},
                    QueryDefinition{"lholo.projection.load", MenuQuery::LoadProjection},
                    QueryDefinition{"lholo.projection.close", MenuQuery::CloseProjection},
                    QueryDefinition{"lholo.projection.restore", MenuQuery::RestoreProjection},
                    QueryDefinition{"lholo.hotkeys.cancel", MenuQuery::CancelHotkeyCapture}
                }) {
                    OreUI::Detail::QueryRegistry::QueryTemplate query;
                    query.mHandlerFactory = std::make_unique<MenuHandlerFactory>(definition.action);
                    query.bind(definition.name, registry.mBinder.get().get());
                    registry.mQueryTemplates.get()[definition.name] = std::move(query);
                }
            });
        registered = true;
    }
    labelsPending = false;
    active = true;
    return true;
}

void resetMenuBridge() {
    labelsPending = browsePending = fileStatePending = false;
    loadPending = closeProjectionPending = restorePending = projectionStatePending = false;
    resetMenuBindings();
    resetCaptureBindings();
    resetTransformBindings();
    resetRenderSettingsBindings();
    resetSettingsBindings();
    publishedLanguage.reset();
}
bool hasMenuRegistrations() { return registered; }
void uninstallMenuBridge() {
    active = false;
    resetMenuBridge();
    if (intentInstalled) MenuIntentActivation::unhook();
    intentInstalled = false;
    if (viewInstalled) MenuViewUpdate::unhook();
    viewInstalled = false;
    shutdownMenuBindings();
    shutdownCaptureBindings();
}
}
