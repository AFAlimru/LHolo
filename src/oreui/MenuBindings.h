#pragma once

#include <string>
#include <optional>

namespace lholo::oreui {
// Menu use cases delegate game actions to the structure layer.
std::string buildMenuLabels();
void requestStructureFileBrowse();
void loadSelectedProjection();
void closeActiveProjection();
void restoreLastProjection();
std::optional<std::string> takeStructureFileUpdate(bool force = false);
std::optional<std::string> takeProjectionUpdate(bool force = false);
void resetMenuBindings();
void shutdownMenuBindings();
}
