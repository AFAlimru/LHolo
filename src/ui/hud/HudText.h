#pragma once

#include <string>
#include <vector>

namespace lholo::ui::hud {
// Native font measurement is relatively expensive. Search UTF-8 character
// boundaries instead of measuring after removing every individual character.
template <class Measure>
std::string fitHudText(std::string text, float width, Measure&& measure) {
    if (measure(text) <= width) return text;
    std::string const suffix = "...";
    if (measure(suffix) > width) return {};
    std::vector<std::size_t> boundaries{0};
    for (std::size_t index = 1; index <= text.size(); ++index) {
        if (index == text.size() || (static_cast<unsigned char>(text[index]) & 0xc0) != 0x80)
            boundaries.push_back(index);
    }
    std::size_t low{}, high = boundaries.size() - 1;
    while (low < high) {
        auto const middle = low + (high - low + 1) / 2;
        if (measure(text.substr(0, boundaries[middle]) + suffix) <= width) low = middle;
        else high = middle - 1;
    }
    text.resize(boundaries[low]);
    return text + suffix;
}
}
