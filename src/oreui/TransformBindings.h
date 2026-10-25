#pragma once
#include <optional>
#include <string>
#include <string_view>
namespace lholo::oreui {
bool queueTransformIntent(std::string_view payload);
std::optional<std::string> takeTransformUpdate(bool force = false);
void resetTransformBindings();
}
