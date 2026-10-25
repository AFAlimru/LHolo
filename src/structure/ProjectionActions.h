#pragma once

#include <string>

namespace lholo::structure {
// UI-independent intents. Executed by the existing application control frame.
bool requestProjectionLoad(std::string path);
bool requestProjectionRestore();
void requestProjectionClose();
bool projectionActionPending();
void processProjectionActions();
void resetProjectionActions();
}
