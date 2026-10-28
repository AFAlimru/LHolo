#pragma once

#include <algorithm>
#include <cstddef>
#include <deque>
#include <utility>
#include <vector>

namespace lholo::projection::detail {
// The shared completion queue obeys the upload budget. Taking one result must
// retain the others, including ownership of their CPU meshes.
template <class Result>
std::vector<Result> takeProjectionBuildResults(std::deque<Result>& pending, std::size_t limit) {
    std::vector<Result> results;
    auto const count = std::min(limit, pending.size());
    results.reserve(count);
    for (std::size_t index = 0; index < count; ++index) {
        results.push_back(std::move(pending.front()));
        pending.pop_front();
    }
    return results;
}
}
