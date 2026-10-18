#include "projection/core/ProjectionBiomeRefresh.h"
#include "projection/core/ProjectionTintRules.h"

#include <cstdio>
#include <limits>
#include <span>
#include <vector>

namespace {
struct Section {
    lholo::projection::detail::ProjectionBiomeRefresh biomeRefresh;
    bool dirty{};
    bool buildInFlight{};
    bool buildVisible{};
    int revisions{};
};

bool hasBlackProjectionTintQuad(std::span<std::uint32_t const> colors) {
    lholo::projection::detail::ProjectionTintQuadObserver observer;
    for (auto const color : colors)
        observer.observe(color);
    return observer.hasBlackFace();
}
} // namespace

int runProjectionBiomeRefreshTests() {
    using lholo::projection::detail::refreshProjectionBiomes;
    int checks{}, failures{};
    auto check = [&](bool passed) {
        ++checks;
        if (!passed)
            ++failures;
    };
    std::size_t visibleCursor{}, backgroundCursor{};
    std::vector<Section> sections(16384);
    for (auto& section : sections)
        section.biomeRefresh.pending = true;
    // Visible work at the far end must not queue behind thousands of unloaded
    // offscreen dependencies. No completion is possible before data arrives.
    sections.back().buildVisible = true;
    int worldChecks{}, invalidations{};
    bool loaded{};
    auto ready = [&](auto const&) {
        ++worldChecks;
        return loaded;
    };
    auto invalidate = [&](std::size_t index) {
        check(!sections[index].dirty && !sections[index].buildInFlight);
        sections[index].dirty = true;
        ++sections[index].revisions;
        ++invalidations;
    };
    std::uint64_t frameNumber{};
    auto tick = [&] {
        refreshProjectionBiomes(sections, visibleCursor, backgroundCursor, ready, invalidate, ++frameNumber);
    };
    for (int frame = 0; frame < 130; ++frame) {
        worldChecks = 0;
        tick();
        check(worldChecks <= 10);
        check(invalidations == 0);
    }
    loaded = true;
    for (int frame = 0; frame < 130; ++frame) {
        worldChecks = 0;
        tick();
        check(worldChecks <= 10);
    }
    check(sections.back().revisions == 1);
    check(!sections.back().biomeRefresh.pending);
    // Data-ready invalidation is consumed once, even after the rebuild is done.
    sections.back().dirty = false;
    for (int frame = 0; frame < 130; ++frame)
        tick();
    check(sections.back().revisions == 1);

    // In-flight/dirty results cannot be invalidated by old readiness metadata.
    sections.assign(3, {});
    for (auto& section : sections) {
        section.biomeRefresh.pending = true;
        section.buildVisible = true;
    }
    sections[0].buildInFlight = true;
    sections[1].dirty = true;
    worldChecks = 0;
    tick();
    check(worldChecks == 1);
    check(sections[0].revisions == 0 && sections[1].revisions == 0);
    check(sections[2].revisions == 1);
    sections[0].buildInFlight = false;
    sections[1].dirty = false;
    tick();
    check(sections[0].revisions == 1 && sections[1].revisions == 1);

    // Reset/shrink safely normalizes both cursors; new sections have no pending
    // dependency from the previous placement, dimension or world.
    sections.clear();
    tick();
    check(visibleCursor == 0 && backgroundCursor == 0);
    sections.resize(1);
    worldChecks = 0;
    tick();
    check(worldChecks == 0 && sections[0].revisions == 0);

    // Many visible dependencies still obey the shared per-frame bound.
    sections.assign(100, {});
    for (auto& section : sections) {
        section.biomeRefresh.pending = true;
        section.buildVisible = true;
    }
    worldChecks = 0;
    tick();
    check(worldChecks == 10);

    // A fully loaded chunk does not guarantee the color/lighting sample used
    // by its first mesh was valid. Retry such a result after its deadline,
    // without repeatedly invalidating it from the two scan lanes.
    sections.assign(1, {});
    sections[0].buildVisible = true;
    sections[0].biomeRefresh.pending = true;
    sections[0].biomeRefresh.sampleFailed = true;
    sections[0].biomeRefresh.notBeforeFrame = 61;
    worldChecks = 0;
    for (std::uint64_t frame = 1; frame < 61; ++frame)
        refreshProjectionBiomes(sections, visibleCursor, backgroundCursor, ready, invalidate, frame);
    check(worldChecks == 0 && sections[0].revisions == 0);
    refreshProjectionBiomes(sections, visibleCursor, backgroundCursor, ready, invalidate, 61);
    check(worldChecks == 1 && sections[0].revisions == 1);
    sections[0].dirty = false;
    refreshProjectionBiomes(sections, visibleCursor, backgroundCursor, ready, invalidate, 62);
    check(worldChecks == 1 && sections[0].revisions == 1);
    using namespace lholo::projection::detail;
    check(projectionBiomeRetryDelay(0) == 60);
    check(projectionBiomeRetryDelay(1) == 60);
    check(projectionBiomeRetryDelay(2) == 120);
    check(projectionBiomeRetryDelay(6) == 240);
    check(projectionBiomeRetryDelay(100) == 240);

    // Simulate the actual build -> publish -> automatic invalidation cycle.
    // More frames and successful color reads cannot replenish the budget.
    for (int cause = 0; cause < 3; ++cause) {
        sections.assign(1, {});
        sections[0].buildVisible = true;
        auto completeBuild = [&](std::uint64_t frame) {
            auto previous = sections[0].biomeRefresh;
            lholo::projection::detail::ProjectionBiomeRefresh next;
            next.sampleFailed = cause == 0;
            next.pending = cause == 1; // main region ready, worker view still not ready
            next.blackGrassFace = cause == 2;
            finishProjectionBiomeRefresh(next, previous);
            armProjectionBiomeRefresh(next, frame);
            sections[0].biomeRefresh = next;
            sections[0].dirty = false;
        };
        completeBuild(0);
        worldChecks = 0;
        for (std::uint64_t frame = 1; frame <= 10000; ++frame) {
            refreshProjectionBiomes(sections, visibleCursor, backgroundCursor, ready, invalidate, frame);
            if (sections[0].dirty)
                completeBuild(frame);
        }
        auto const expected = cause == 2 ? 1 : kMaxProjectionBiomeAutoRefreshes;
        check(sections[0].revisions == static_cast<int>(expected));
        check(sections[0].biomeRefresh.automaticRefreshes == expected);
        check(!sections[0].biomeRefresh.pending);
        check(worldChecks == static_cast<int>(expected)); // no further queries after exhaustion
    }
    // External invalidation/new placement drops the old numeric dependency.
    sections[0].biomeRefresh = {};
    sections[0].biomeRefresh.sampleFailed = true;
    auto reset = sections[0].biomeRefresh;
    finishProjectionBiomeRefresh(sections[0].biomeRefresh, reset);
    armProjectionBiomeRefresh(sections[0].biomeRefresh, 10001);
    check(sections[0].biomeRefresh.automaticRefreshes == 0 && sections[0].biomeRefresh.pending);
    // Waiting for an unloaded chunk is only a throttled readiness query, not
    // a mesh retry and not a budget reset.
    sections.assign(1, {});
    sections[0].buildVisible = true;
    sections[0].biomeRefresh.pending = true;
    loaded = false;
    worldChecks = 0;
    for (std::uint64_t frame = 1; frame <= 600; ++frame)
        refreshProjectionBiomes(sections, visibleCursor, backgroundCursor, ready, invalidate, frame);
    check(worldChecks == 10);
    check(sections[0].revisions == 0 && sections[0].biomeRefresh.automaticRefreshes == 0);
    loaded = true;
    // Defend against an exhausted result accidentally retaining pending=true.
    sections[0].biomeRefresh.automaticRefreshes = kMaxProjectionBiomeAutoRefreshes;
    sections[0].biomeRefresh.pending = true;
    worldChecks = 0;
    refreshProjectionBiomes(sections, visibleCursor, backgroundCursor, ready, invalidate, 10001);
    check(worldChecks == 0 && !sections[0].biomeRefresh.pending);
    ProjectionBiomeRefresh prior;
    prior.automaticRefreshes = 2;
    ProjectionBiomeRefresh goodSample;
    finishProjectionBiomeRefresh(goodSample, prior);
    check(goodSample.automaticRefreshes == 2 && !goodSample.pending);
    check(armProjectionBiomeRefresh(goodSample, 10002) == 0 && goodSample.notBeforeFrame == 0);
    ProjectionBiomeRefresh nextFailure;
    nextFailure.sampleFailed = true;
    finishProjectionBiomeRefresh(nextFailure, goodSample);
    check(nextFailure.automaticRefreshes == 2 && nextFailure.pending);
    check(nextFailure.notBeforeFrame == 0); // construction does not set an upload deadline
    check(armProjectionBiomeRefresh(nextFailure, 10003) == 240);
    check(nextFailure.notBeforeFrame == 10243);
    // A result delayed in the completion queue is armed at publication time.
    armProjectionBiomeRefresh(nextFailure, 20000);
    check(nextFailure.notBeforeFrame == 20240 && nextFailure.automaticRefreshes == 2);

    ProjectionTintRgb const grass{0.3f, 0.7f, 0.2f};
    ProjectionTintRgb const engineDefault{0.4f, 0.6f, 0.1f};
    check(validProjectionBiomeTint(grass));
    check(projectionFallbackBiomeTint(engineDefault) == engineDefault);
    auto const nan = std::numeric_limits<float>::quiet_NaN();
    auto const inf = std::numeric_limits<float>::infinity();
    for (auto const& invalid : std::vector<ProjectionTintRgb>{{0, 0, 0},
                                                              {nan, 0.5f, 0.2f},
                                                              {0.2f, inf, 0.2f},
                                                              {0.2f, 0.5f, -inf},
                                                              {-0.1f, 0.6f, 0.2f},
                                                              {0.1f, 1.1f, 0.2f},
                                                              {0.1f, 0.6f, nan}}) {
        check(!validProjectionBiomeTint(invalid));
    }
    check(validProjectionBiomeTint({0.01f, 0.03f, 0.01f})); // preserve legitimate dark tints
    check((projectionFallbackBiomeTint({nan, nan, nan}) == ProjectionTintRgb{1, 1, 1}));
    check(!hasBlackProjectionTintQuad({}));
    std::array<std::uint32_t, 8> colors{0xFF000000U, 0xFF000000U, 0xFF000000U, 0xFF102030U,
                                        0xFF102030U, 0xFF102030U, 0xFF102030U, 0xFF102030U};
    check(!hasBlackProjectionTintQuad(colors)); // shaded corners alone are not a failed face
    colors[3] = 0x80000000U;
    check(hasBlackProjectionTintQuad(colors)); // alpha cannot hide black RGB
    colors[0] = 0xFF102030U;
    check(!hasBlackProjectionTintQuad(colors));
    for (std::size_t vertex = 4; vertex < 8; ++vertex)
        colors[vertex] = 0xFF000000U;
    check(hasBlackProjectionTintQuad(colors)); // a later face of the block can fail independently
    check(!hasBlackProjectionTintQuad(std::span<std::uint32_t const>{colors}.subspan(5)));
    std::printf("ProjectionBiomeRefreshTests: %d checks, %d failures\n", checks, failures);
    return failures;
}
