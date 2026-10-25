#pragma once

namespace lholo::oreui {
inline constexpr char kMenuRoute[] = "/lholo-menu";
inline constexpr char kMenuAsset[] = "/hbui/lholo-menu.html";

bool installMenuScreen();
void uninstallMenuScreen();
// Called by View update; never retain a View or Scene between frames.
void processMenuScreen();
void requestMenuClose();
bool isMenuOnTop();
}
