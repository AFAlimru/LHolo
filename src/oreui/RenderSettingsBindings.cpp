#include "oreui/RenderSettingsBindings.h"
#include "oreui/RenderSettingsProtocol.h"
#include "app/RenderSettingsActions.h"
#include "nlohmann/json.hpp"
#include <mutex>
namespace lholo::oreui {
namespace { std::mutex mutex; std::string published; }
bool queueRenderSettingsIntent(std::string_view payload) {
    auto edit = parseRenderSettingsIntent(payload);
    return edit && app::submitRenderSettingsEdit(*edit);
}
std::optional<std::string> takeRenderSettingsUpdate(bool force) {
    std::lock_guard lock(mutex);
    auto s = app::getRenderSettingsState();
    auto const& v = s.values;
    auto state = nlohmann::json{{"values", {{"bounds", v.bounds}, {"axis", structure::toInt(v.layers.axis)},
        {"mode", structure::toInt(v.layers.mode)}, {"layer", v.layers.layer}, {"fill", v.fill}, {"outline", v.outline},
        {"correctionSeeThrough", v.correctionSeeThrough}, {"missingSeeThrough", v.missingSeeThrough}}},
        {"maximum", s.maximum}, {"context", s.context}, {"ack", s.ack}, {"accepted", s.accepted}}.dump();
    if (!force && published == state) return std::nullopt;
    published = state;
    return state;
}
void resetRenderSettingsBindings() {
    std::lock_guard lock(mutex);
    published.clear(); app::resetRenderSettingsEdits();
}
}
