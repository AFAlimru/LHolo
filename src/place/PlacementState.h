// LHolo - Client-side projection renderer for Minecraft Bedrock Windows
// Copyright (C) 2026  MarmieQi
//
// Placement session state owned by the place module: toggles, manual/range
// timers, recent cells, failed-plan cache and the aimed block-entity name.
// Accessors return the underlying storage so logic keeps its exact shapes.

#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <unordered_map>

namespace lholo::place::detail {

struct FailedPlanKey {
    std::int64_t cell;
    std::uint32_t runtimeId;
    int          itemAux;
    int          eyeX;
    int          eyeY;
    int          eyeZ;
    int          viewX;
    int          viewY;
    int          viewZ;

    bool operator==(FailedPlanKey const&) const = default;
};

struct FailedPlanKeyHash {
    std::size_t operator()(FailedPlanKey const& key) const noexcept {
        std::size_t result = std::hash<std::int64_t>{}(key.cell);
        auto const combine = [&result](auto value) {
            std::size_t const hash = std::hash<decltype(value)>{}(value);
            result ^= hash + 0x9e3779b9U + (result << 6U) + (result >> 2U);
        };
        combine(key.runtimeId);
        combine(key.itemAux);
        combine(key.eyeX);
        combine(key.eyeY);
        combine(key.eyeZ);
        combine(key.viewX);
        combine(key.viewY);
        combine(key.viewZ);
        return result;
    }
};

std::atomic_bool& placementEnabled();
std::atomic_bool& placementRangeEnabled();
std::atomic_bool& placementManualMode();
std::atomic_bool& placementManualHeld();
std::atomic_bool& placementManualPlaceRequested();
std::atomic_uint64_t& placementManualPressAt();
std::atomic_uint64_t& placementLastManualPlaceAt();
std::atomic_int& placementRadius();
std::atomic_uint64_t& placementNextPlaceAt();
std::atomic_uint64_t& placementNextSwapAt();

std::mutex&                                      placementRecentMutex();
std::unordered_map<std::int64_t, std::uint64_t>& placementRecentPlacements();
std::unordered_map<FailedPlanKey, std::uint64_t, FailedPlanKeyHash>& placementFailedRangePlans();

std::string& placementAimedBlockEntityName();
std::mutex&  placementAimedNameMutex();

} // namespace lholo::place::detail
