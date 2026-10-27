#pragma once
#include <cstddef>
#include "app/TaskActions.h"
#include <optional>
#include <string_view>
namespace lholo::oreui {
inline constexpr std::size_t kTaskValueMaxBytes = 32768;
inline constexpr std::size_t kTaskIntentMaxBytes = 262144;
std::optional<app::TaskIntent> parseTaskIntent(std::string_view);
}
