#pragma once
#include <optional>
#include <string>
#include <string_view>
namespace lholo::oreui {
bool queueTaskIntent(std::string_view);
std::optional<std::string> takeTaskUpdate(bool force);
void resetTaskBindings();
void shutdownTaskBindings();
} // namespace lholo::oreui
