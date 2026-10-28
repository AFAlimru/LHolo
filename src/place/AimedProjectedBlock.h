#pragma once

#include <memory>
#include <string>

class ItemStack;

namespace lholo::place {
// One coherent tick snapshot; the render thread never resolves game items.
struct AimedProjectedBlock {
    std::string name;
    std::string iconId;
    std::shared_ptr<ItemStack const> iconItem;
};
}
