#include "input/MaterialHudPageRequest.h"
#include <atomic>

namespace lholo::input {
namespace { std::atomic_int pending{}; }
void requestMaterialHudPage(int delta) { pending.fetch_add(delta, std::memory_order_relaxed); }
int consumeMaterialHudPageRequest() { return pending.exchange(0, std::memory_order_acq_rel); }
void clearMaterialHudPageRequest() { pending.store(0, std::memory_order_release); }
}
