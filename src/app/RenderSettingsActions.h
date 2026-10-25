#pragma once
#include "app/RenderSettingsTypes.h"
namespace lholo::app {
bool submitRenderSettingsEdit(RenderSettingsEdit const& edit);
RenderSettingsState getRenderSettingsState();
bool processRenderSettingsEdits(); // control frame; true when persistence is needed
void resetRenderSettingsEdits();
}
