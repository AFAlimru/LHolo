// LHolo - Client-side projection renderer for Minecraft Bedrock Windows
// Copyright (C) 2026  MarmieQi

#include "place/PlacementState.h"

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>

namespace lholo::place::detail {
namespace {

std::atomic_bool gEnabled{false};
std::atomic_bool gRangeEnabled{false};
std::atomic_bool gManualMode{false};
std::atomic_bool gManualHeld{false};
std::atomic_bool gManualPlaceRequested{false};
std::atomic_uint64_t gManualPressAt{0};
std::atomic_uint64_t gLastManualPlaceAt{0};
std::atomic_int gPlacementRadius{4};
std::atomic_uint64_t gNextPlaceAt{0};
std::atomic_uint64_t gNextSwapAt{0};
std::mutex                                      gRecentMutex;
std::unordered_map<std::int64_t, std::uint64_t> gRecentPlacements;
std::unordered_map<FailedPlanKey, std::uint64_t, FailedPlanKeyHash> gFailedRangePlans;
std::string gAimedBlockEntityName;
std::mutex  gAimedNameMutex;

} // namespace

std::atomic_bool& placementEnabled() { return gEnabled; }
std::atomic_bool& placementRangeEnabled() { return gRangeEnabled; }
std::atomic_bool& placementManualMode() { return gManualMode; }
std::atomic_bool& placementManualHeld() { return gManualHeld; }
std::atomic_bool& placementManualPlaceRequested() { return gManualPlaceRequested; }
std::atomic_uint64_t& placementManualPressAt() { return gManualPressAt; }
std::atomic_uint64_t& placementLastManualPlaceAt() { return gLastManualPlaceAt; }
std::atomic_int& placementRadius() { return gPlacementRadius; }
std::atomic_uint64_t& placementNextPlaceAt() { return gNextPlaceAt; }
std::atomic_uint64_t& placementNextSwapAt() { return gNextSwapAt; }

std::mutex& placementRecentMutex() { return gRecentMutex; }
std::unordered_map<std::int64_t, std::uint64_t>& placementRecentPlacements() {
    return gRecentPlacements;
}
std::unordered_map<FailedPlanKey, std::uint64_t, FailedPlanKeyHash>& placementFailedRangePlans() {
    return gFailedRangePlans;
}

std::string& placementAimedBlockEntityName() { return gAimedBlockEntityName; }
std::mutex& placementAimedNameMutex() { return gAimedNameMutex; }

} // namespace lholo::place::detail
