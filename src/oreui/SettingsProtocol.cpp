#include "oreui/SettingsProtocol.h"
#include "nlohmann/json.hpp"
#include <algorithm>
#include <limits>
namespace lholo::oreui {
std::optional<app::SettingsIntent> parseSettingsIntent(std::string_view payload) {
    if (payload.size() > 4096) return std::nullopt;
    try {
        auto j = nlohmann::json::parse(payload);
        auto safeId = [](auto const& n) {
            if (!n.is_number_unsigned() || n.template get<std::uint64_t>() > 9007199254740991ULL) throw std::invalid_argument("id");
            return n.template get<std::uint64_t>();
        };
        auto scalar = [](auto const& n) -> app::SettingsValue {
            if (n.is_boolean()) return n.template get<bool>();
            if (n.is_string()) return n.template get<std::string>();
            if (!n.is_number_integer() || (n.is_number_unsigned() && n.template get<std::uint64_t>() > 2147483647ULL)) throw std::invalid_argument("integer");
            auto i = n.template get<std::int64_t>();
            if (i < std::numeric_limits<int>::min() || i > std::numeric_limits<int>::max()) throw std::invalid_argument("range");
            return static_cast<int>(i);
        };
        app::SettingsIntent i; i.id = safeId(j.at("id")); i.context = safeId(j.at("context"));
        auto action = j.at("action").get<std::string>();
        if (action == "set") {
            auto field = j.at("field").get<std::string>();
            auto found = std::find(kSettingsNames.begin(), kSettingsNames.end(), field);
            if (found == kSettingsNames.end()) return std::nullopt;
            i.field = static_cast<app::SettingsField>(found - kSettingsNames.begin());
            i.expected = scalar(j.at("expected")); i.value = scalar(j.at("value"));
        } else if (action == "consent") {
            i.action = app::SettingsAction::Consent; i.expected = scalar(j.at("expected")); i.value = scalar(j.at("value"));
        } else {
            if (action == "capture") i.action = app::SettingsAction::BeginCapture;
            else if (action == "clearHotkey") i.action = app::SettingsAction::ClearHotkey;
            else if (action == "resetHotkey") i.action = app::SettingsAction::ResetHotkey;
            else if (action == "resetHotkeys") i.action = app::SettingsAction::ResetHotkeys;
            else return std::nullopt;
            i.hotkeysRevision = safeId(j.at("hotkeysRevision"));
            if (i.action != app::SettingsAction::ResetHotkeys) {
                auto slot = safeId(j.at("slot"));
                if (slot >= input::kHotkeyCount) return std::nullopt;
                i.slot = static_cast<std::size_t>(slot);
            }
        }
        return app::validSettingsIntent(i) ? std::optional{i} : std::nullopt;
    } catch (std::exception const&) { return std::nullopt; }
}
}
