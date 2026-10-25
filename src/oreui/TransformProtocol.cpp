#include "oreui/TransformProtocol.h"
#include "nlohmann/json.hpp"
#include <limits>
namespace lholo::oreui {
std::optional<structure::TransformEdit> parseTransformIntent(std::string_view payload) {
    if (payload.size() > 4096) return std::nullopt;
    try {
        auto j = nlohmann::json::parse(payload);
        auto id = [](auto const& n) {
            if (!n.is_number_unsigned() || n.template get<std::uint64_t>() > 9007199254740991ULL)
                throw std::invalid_argument("id");
            return n.template get<std::uint64_t>();
        };
        auto integer = [](auto const& n) {
            if (!n.is_number_integer() || (n.is_number_unsigned() && n.template get<std::uint64_t>() > 2147483647ULL))
                throw std::invalid_argument("integer");
            auto v = n.template get<std::int64_t>();
            if (v < std::numeric_limits<int>::min() || v > std::numeric_limits<int>::max()) throw std::invalid_argument("range");
            return static_cast<int>(v);
        };
        auto field = j.at("field").get<std::string>();
        using F = structure::TransformField;
        F f;
        if (field == "rotation") f = F::Rotation;
        else if (field == "mirror") f = F::Mirror;
        else if (field == "x") f = F::X;
        else if (field == "y") f = F::Y;
        else if (field == "z") f = F::Z;
        else return std::nullopt;
        structure::TransformEdit edit{id(j.at("id")), id(j.at("context")), f,
            integer(j.at("expected")), integer(j.at("value"))};
        return structure::validTransformEdit(edit) ? std::optional{edit} : std::nullopt;
    } catch (std::exception const&) { return std::nullopt; }
}
}
