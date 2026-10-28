#pragma once
#include <algorithm>
#include <cstddef>
#include <cstdint>

namespace lholo::ui::hud {
struct MaterialHudPageContext {
    std::uint64_t generation{};
    int source{-1}, pageSize{}, axis{}, mode{}, layer{};
    bool operator==(MaterialHudPageContext const&) const = default;
};
// Pure, transient UI state; never writes structure data or persistent settings.
struct MaterialHudPaging {
    std::size_t index{}, count{1};
    MaterialHudPageContext context;
    bool setContext(MaterialHudPageContext next) {
        if (context == next) return false;
        context = next;
        index = 0; count = 1;
        return true;
    }
    void update(std::size_t items, std::size_t pageSize, int delta = 0) {
        pageSize = std::max(std::size_t{1}, pageSize);
        count = items == 0 ? 1 : 1 + (items - 1) / pageSize;
        index = std::min(index, count - 1);
        if (delta > 0) index += std::min(static_cast<std::size_t>(delta), count - 1 - index);
        else if (delta < 0) index -= std::min(static_cast<std::size_t>(-static_cast<std::int64_t>(delta)), index);
    }
};
}
