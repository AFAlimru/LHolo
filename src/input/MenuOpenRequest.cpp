#include "input/MenuOpenRequest.h"

#include <atomic>

namespace lholo::input {
namespace { std::atomic_bool pending{}; }
void requestMenuOpen() { pending.store(true, std::memory_order_release); }
bool consumeMenuOpenRequest() { return pending.exchange(false, std::memory_order_acq_rel); }
void clearMenuOpenRequest() { pending.store(false, std::memory_order_release); }
}
