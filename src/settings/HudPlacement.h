#pragma once
#include <array>

namespace lholo::settings {
struct HudPlacement {
    bool custom{};
    float x{}, y{}; // Top-left position, normalized to the native viewport.
    bool operator==(HudPlacement const&) const = default;
};
using HudPlacements = std::array<HudPlacement, 2>; // Projection, materials.
}
