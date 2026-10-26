#pragma once

#include "projection/ProjectionTypes.h"
#include <string>
#include <optional>
#include <vector>

namespace lholo::structure::detail {
struct HudStateSnapshot;
struct MaterialHudSnapshot;
}

namespace lholo::ui::hud {
enum class TextTone { Normal, Muted, Success, WrongState, WrongType, Extra, Placement };
struct HudLine {
    std::string text;
    TextTone tone{TextTone::Normal};
    std::optional<std::string> iconId;
};
struct HudPanel {
    int position{}; // 0 TL, 1 BL, 2 TR, 3 BR; matches persisted settings.
    std::vector<HudLine> lines;
    bool compactRows{};
};
struct HudContent {
    std::uint64_t context{}; // Loaded-structure generation for icon cache invalidation.
    HudPanel projection;
    HudPanel materials;
};
struct ProjectionHudData {
    std::string fileName;
    structure::LayerAxis axis{structure::LayerAxis::Y};
    structure::LayerDisplayMode mode{structure::LayerDisplayMode::All};
    int layer{}, maxLayer{};
    projection::BuildProgress progress;
    std::string aimedBlock;
    std::string placementMode;
};

// Pure presentation rules, independent of native font/context and game objects.
HudPanel makeProjectionHud(structure::detail::HudStateSnapshot const& settings, ProjectionHudData const& data);
HudPanel makeMaterialHud(structure::detail::MaterialHudSnapshot const& snapshot, int position);

// Read existing business snapshots only; no world/inventory scans or settings writes.
HudContent readHudContent();
}
