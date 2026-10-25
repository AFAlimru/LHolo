#include "oreui/RenderSettingsProtocol.h"
#include "nlohmann/json.hpp"
#include <limits>
namespace lholo::oreui {
std::optional<app::RenderSettingsEdit> parseRenderSettingsIntent(std::string_view payload) {
    if (payload.size() > 4096) return std::nullopt;
    try {
        auto j = nlohmann::json::parse(payload);
        auto safeId = [](auto const& n) {
            if (!n.is_number_unsigned() || n.template get<std::uint64_t>() > 9007199254740991ULL) throw std::invalid_argument("id");
            return n.template get<std::uint64_t>();
        };
        auto integer = [](auto const& n) {
            if (!n.is_number_integer() || (n.is_number_unsigned() && n.template get<std::uint64_t>() > 2147483647ULL)) throw std::invalid_argument("integer");
            auto v = n.template get<std::int64_t>();
            if (v < std::numeric_limits<int>::min() || v > std::numeric_limits<int>::max()) throw std::invalid_argument("range");
            return static_cast<int>(v);
        };
        auto axis = integer(j.at("axis"));
        if (axis < 0 || axis > 2) return std::nullopt;
        app::RenderSettingsEdit edit{safeId(j.at("id")), safeId(j.at("context")), structure::layerAxisFromInt(axis), {}};
        auto const& changes = j.at("changes");
        if (!changes.is_array() || changes.empty() || changes.size() > 8) return std::nullopt;
        for (auto const& c : changes) {
            auto name = c.at("field").get<std::string>();
            using F = app::RenderField;
            F field;
            if (name == "bounds") field = F::Bounds;
            else if (name == "axis") field = F::Axis;
            else if (name == "mode") field = F::Mode;
            else if (name == "layer") field = F::Layer;
            else if (name == "fill") field = F::Fill;
            else if (name == "outline") field = F::Outline;
            else if (name == "correctionSeeThrough") field = F::CorrectionSeeThrough;
            else if (name == "missingSeeThrough") field = F::MissingSeeThrough;
            else return std::nullopt;
            auto value = [&](auto const& n) -> app::RenderValue { if (n.is_boolean()) return n.template get<bool>(); return integer(n); };
            edit.changes.push_back({field, value(c.at("expected")), value(c.at("value"))});
        }
        return app::validRenderEdit(edit) ? std::optional{edit} : std::nullopt;
    } catch (std::exception const&) { return std::nullopt; }
}
}
