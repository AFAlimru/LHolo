#pragma once
#include <atomic>
#include <cstdint>
namespace lholo::structure {
inline std::uint64_t nextStructureGeneration() {
    static std::atomic_uint64_t generation{};
    return generation.fetch_add(1, std::memory_order_relaxed) + 1;
}
} // namespace lholo::structure
