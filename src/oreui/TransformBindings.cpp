#include "oreui/TransformBindings.h"
#include "oreui/TransformProtocol.h"
#include "structure/TransformActions.h"
#include "nlohmann/json.hpp"
#include <mutex>
namespace lholo::oreui {
namespace { std::mutex mutex; std::string published; }
bool queueTransformIntent(std::string_view payload) {
    auto edit = parseTransformIntent(payload);
    return edit && structure::submitTransformEdit(*edit);
}
std::optional<std::string> takeTransformUpdate(bool force) {
    std::lock_guard lock(mutex);
    auto s = structure::getTransformState();
    auto state = nlohmann::json{{"values", {{"rotation", s.values.rotation}, {"mirror", s.values.mirror},
        {"x", s.values.x}, {"y", s.values.y}, {"z", s.values.z}}},
        {"context", s.context}, {"ack", s.ack}, {"accepted", s.accepted}}.dump();
    if (!force && state == published) return std::nullopt;
    published = state;
    return state;
}
void resetTransformBindings() {
    std::lock_guard lock(mutex);
    published.clear();
    structure::resetTransformEdits();
}
}
