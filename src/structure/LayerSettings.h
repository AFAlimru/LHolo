#pragma once
#include "structure/LayerDisplayTypes.h"
#include <array>
#include <cstdint>

namespace lholo::structure {
struct LayerSettings {
    LayerAxis axis{LayerAxis::Y};
    LayerDisplayMode mode{LayerDisplayMode::All};
    int layer{};
    bool operator==(LayerSettings const&) const = default;
};
struct LayerSettingsSnapshot {
    LayerSettings values;
    std::array<int, 3> maximum{}; // Y, X, material; zero-based, empty = 0
    std::uint64_t context{};
};
}
