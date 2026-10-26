#pragma once
#include <optional>
namespace lholo::ui::hud {
bool installHudEditorScene();
void uninstallHudEditorScene();
void requestHudEditorScene();
// Completion is consumed by the app; true means persist the accepted draft.
std::optional<bool> consumeHudEditorCompletion();
}
