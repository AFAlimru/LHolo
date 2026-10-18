// LHolo - Bounded retry of meshes sampled before world chunks were ready.
#pragma once

#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>

namespace lholo::projection::detail {

inline constexpr unsigned int kMaxProjectionBiomeAutoRefreshes = 3;
inline constexpr std::uint64_t kProjectionBiomePollInterval = 60;

struct ProjectionBiomeRefresh {
    std::array<int, 3> minimum{};
    std::array<int, 3> maximum{};
    bool pending{};
    bool sampleFailed{};
    bool blackGrassFace{};
    unsigned int automaticRefreshes{};
    std::uint64_t notBeforeFrame{};
};

inline std::uint64_t projectionBiomeRetryDelay(unsigned int attempts) {
    return std::uint64_t{60} << std::min(attempts > 0 ? attempts - 1 : 0,
                                         kMaxProjectionBiomeAutoRefreshes - 1);
}

// A mesh result preserves the budget consumed by the task which produced it.
// A successful color sample must not reset it while chunk readiness disagrees
// between the main region and the worker view. Only an external invalidation
// or a new placement starts another budget.
inline void finishProjectionBiomeRefresh(ProjectionBiomeRefresh& result,
                                         ProjectionBiomeRefresh const& previous) {
    assert(previous.automaticRefreshes <= kMaxProjectionBiomeAutoRefreshes);
    result.automaticRefreshes = previous.automaticRefreshes;
    bool const suspectBlackFace = result.blackGrassFace && result.automaticRefreshes == 0;
    result.pending = (result.pending || result.sampleFailed || suspectBlackFace) &&
                     result.automaticRefreshes < kMaxProjectionBiomeAutoRefreshes;
}

// Deadlines belong to publication on the render thread, not CPU construction
// in an asynchronous snapshot. Both upload and synchronous fallback use this.
inline std::uint64_t armProjectionBiomeRefresh(ProjectionBiomeRefresh& refresh, std::uint64_t frameNumber) {
    auto const delay = refresh.pending ? projectionBiomeRetryDelay(refresh.automaticRefreshes + 1) : 0;
    refresh.notBeforeFrame = refresh.pending ? frameNumber + delay : 0;
    return delay;
}

// Both budgets are independent of projection size. The rotating cursor gives
// unloaded/offscreen sections a chance without repeatedly rebuilding them.
template <class Sections, class Ready, class Invalidate>
void refreshProjectionBiomes(Sections& sections, std::size_t& visibleCursor, std::size_t& backgroundCursor,
                             Ready&& ready, Invalidate&& invalidate, std::uint64_t frameNumber) {
    auto const count = sections.size();
    if (count == 0) {
        visibleCursor = backgroundCursor = 0;
        return;
    }
    auto scan = [&](std::size_t& cursor, bool visibleOnly, std::size_t budget) {
        std::size_t checks{};
        for (std::size_t scanned = 0; scanned < std::min<std::size_t>(count, 128) && checks < budget;
             ++scanned) {
            cursor %= count;
            auto const index = cursor++;
            auto& section = sections[index];
            if (visibleOnly && !section.buildVisible)
                continue;
            if (!section.biomeRefresh.pending || section.dirty || section.buildInFlight)
                continue;
            if (section.biomeRefresh.automaticRefreshes >= kMaxProjectionBiomeAutoRefreshes) {
                section.biomeRefresh.pending = false;
                continue;
            }
            if (section.biomeRefresh.notBeforeFrame > frameNumber)
                continue;
            ++checks;
            if (!ready(section.biomeRefresh)) {
                section.biomeRefresh.notBeforeFrame = frameNumber + kProjectionBiomePollInterval;
                continue;
            }
            section.biomeRefresh.pending = false;
            ++section.biomeRefresh.automaticRefreshes;
            invalidate(index);
        }
    };
    scan(visibleCursor, true, 8);
    scan(backgroundCursor, false, 2);
}

} // namespace lholo::projection::detail
