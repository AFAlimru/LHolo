#pragma once

#include <cstddef>

namespace lholo::projection::detail {
// Every file still runs on every opaque frame. Rotate only the first visitor
// so a perpetually dirty file cannot monopolize the single mesh worker.
template <class Render>
void forEachIndependentProjectionFile(std::size_t count, std::size_t& cursor, Render&& render) {
    if (!count) { cursor = 0; return; }
    auto const first = cursor % count;
    cursor = (first + 1) % count;
    for (std::size_t offset = 0; offset < count; ++offset)
        render((first + offset) % count);
}
}
