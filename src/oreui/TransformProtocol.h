#pragma once
#include "structure/TransformTypes.h"
#include <optional>
#include <string_view>
namespace lholo::oreui {
std::optional<structure::TransformEdit> parseTransformIntent(std::string_view payload);
}
