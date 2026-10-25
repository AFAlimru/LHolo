#include "structure/TransformActions.h"
#include "structure/StructureSession.h"
#include <mutex>
#include <optional>
#include <utility>

namespace lholo::structure {
namespace {
std::mutex mutex;
std::optional<TransformEdit> pending;
std::uint64_t ack{};
bool accepted{};
}
bool submitTransformEdit(TransformEdit const& edit) {
    std::lock_guard lock(mutex);
    if (!validTransformEdit(edit) || edit.id <= ack
        || (pending && (edit.id <= pending->id || edit.context <= pending->context))) return false;
    pending = edit;
    return true;
}
TransformState getTransformState() {
    std::lock_guard lock(mutex);
    auto state = detail::StructureSession::getInstance().transformEditorState();
    state.ack = ack; state.accepted = accepted;
    return state;
}
bool processTransformEdits() {
    std::lock_guard lock(mutex);
    if (!pending) return false;
    auto edit = *std::exchange(pending, std::nullopt);
    accepted = detail::StructureSession::getInstance().applyTransformEdit(edit);
    ack = edit.id;
    return accepted && edit.value != edit.expected;
}
void resetTransformEdits() {
    std::lock_guard lock(mutex);
    if (pending) ack = pending->id;
    pending.reset(); accepted = false;
}
}
