#pragma once
#include <string_view>

namespace lholo::oreui {
// Only the stable title screen is supported without a world, not login/loading
// dialogs or arbitrary menus. Playing preserves the existing in-world entry.
constexpr bool canOpenMenuContext(bool playing, bool idle, std::string_view screen) {
    return playing || (idle && screen == "start_screen");
}
}
