// LHolo - Client-side projection renderer for Minecraft Bedrock Windows
// Copyright (C) 2026  MarmieQi

#include "projection/section/ProjectionSectionStateStore.h"

#include <cstddef>

namespace lholo::projection::detail {

void initializeSectionStates(
    std::vector<SectionState>& sections,
    std::vector<Vec3> const&   centers
) {
    sections.resize(centers.size());
    for (std::size_t index = 0; index < sections.size(); ++index) {
        auto& section = sections[index];
        section = SectionState{};
        section.center = centers[index];
        section.dirty = true;
        section.requestedRevision = 1;
    }
}

} // namespace lholo::projection::detail
