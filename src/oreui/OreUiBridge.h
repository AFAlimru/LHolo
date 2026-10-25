#pragma once

namespace lholo::oreui {
bool installMenuBridge();
void uninstallMenuBridge();
void resetMenuBridge();
// Native route/query registries retain callbacks until process exit.
bool hasMenuRegistrations();
}
