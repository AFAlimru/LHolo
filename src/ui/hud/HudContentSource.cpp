#include "ui/hud/HudContent.h"
#include "structure/StructureLoader.h"
#include "structure/StructureSession.h"
#include "structure/StructureUiState.h"
#include "structure/StructurePaths.h"
#include "projection/Projection.h"
#include "place/PlaceHelper.h"
#include "oreui/OreUiScreen.h"
#include <algorithm>

namespace lholo::ui::hud {
HudContent readHudContent() {
    HudContent result;
    if (structure::isGuiVisible() || oreui::isMenuOnTop() || projection::isDimensionSuspended()) return result;
    auto& state = structure::detail::StructureUiState::getInstance();
    auto const settings = state.hud();
    auto const materialEnabled = state.materialHudEnabled();
    if (!settings.enabled && !materialEnabled) return result;
    auto const session = structure::detail::StructureSession::getInstance().snapshot();
    if (!session.loaded) return result;
    result.context = session.loaded->generation;
    if (settings.enabled) {
        ProjectionHudData data;
        data.fileName = structure::detail::pathToUtf8(session.loaded->sourcePath.filename());
        data.axis = session.transform.layerAxis;
        data.mode = session.transform.layerDisplayMode;
        data.layer = session.transform.displayLayer;
        data.maxLayer = data.axis == structure::LayerAxis::Material
            ? std::max(0, static_cast<int>(session.loaded->materialCount) - 1)
            : data.axis == structure::LayerAxis::X ? session.maxLayerX : session.maxLayerY;
        if (settings.showOverallProgress || settings.showProgress || settings.showWrongState || settings.showWrongType || settings.showExtraBlocks)
            data.progress = projection::getBuildProgress();
        if (settings.showProjectedBlockName) data.aimedBlock = place::getAimedProjectedBlockName();
        using i18n::TextKey;
        if (place::isManualMode()) data.placementMode = i18n::tr(TextKey::ModeManual);
        else if (place::isEnabled()) data.placementMode = i18n::tr(TextKey::ModeEasy);
        else if (place::isRangeEnabled()) data.placementMode = i18n::tr(TextKey::ModeRange);
        result.projection = makeProjectionHud(settings, data);
    }
    if (materialEnabled) result.materials = makeMaterialHud(state.materialHudSnapshot(), state.materialHudPosition());
    return result;
}
}
