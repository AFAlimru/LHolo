#pragma once
#include "structure/StructureLoader.h"
#include "structure/LayerSettings.h"
#include <array>
#include <functional>
#include <span>
namespace lholo::structure {
struct CompositionFile {
    std::string id;
    std::shared_ptr<LoadedStructure const> loaded;
    bool visible{true};
    int x{}, y{}, z{}, rotation{}, mirror{};
    LayerSettings slice;
};
struct Composition {
    std::shared_ptr<LoadedStructure> loaded;
    std::array<int, 3> origin{};
};
using BlockTransform = std::function<Block const*(Block const*, int mirror, int rotation)>;
Composition composeTask(std::span<CompositionFile const>, BlockTransform const&);
} // namespace lholo::structure
