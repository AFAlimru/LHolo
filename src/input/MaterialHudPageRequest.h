#pragma once

namespace lholo::input {
// Input intent only. The HUD owns page selection and rendering.
void requestMaterialHudPage(int delta);
int consumeMaterialHudPageRequest();
void clearMaterialHudPageRequest();
}
