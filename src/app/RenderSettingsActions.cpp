#include "app/RenderSettingsActions.h"
#include "structure/StructureSession.h"
#include "projection/Projection.h"
#include <algorithm>
#include <cmath>
#include <mutex>
#include <optional>
#include <set>
#include <utility>

namespace lholo::app {
namespace {
std::mutex mutex;
std::optional<RenderSettingsEdit> pending;
std::uint64_t ack{};
bool accepted{};
int percent(float value) { return std::clamp(static_cast<int>(std::lround(value * 100)), 0, 100); }
RenderSettingsState snapshot() {
    auto layers = structure::detail::StructureSession::getInstance().layerSettings();
    return {{projection::getStructureBoundsEnabled(), layers.values,
        percent(projection::getCorrectionFillOpacity()), percent(projection::getCorrectionOutlineOpacity()),
        projection::getCorrectionSeeThrough(), projection::getMissingSeeThrough()}, layers.maximum, layers.context, ack, accepted};
}
}
RenderValue renderValue(RenderValues const& v, RenderField field) {
    switch (field) {
    case RenderField::Bounds: return v.bounds;
    case RenderField::Axis: return structure::toInt(v.layers.axis);
    case RenderField::Mode: return structure::toInt(v.layers.mode);
    case RenderField::Layer: return v.layers.layer;
    case RenderField::Fill: return v.fill;
    case RenderField::Outline: return v.outline;
    case RenderField::CorrectionSeeThrough: return v.correctionSeeThrough;
    case RenderField::MissingSeeThrough: return v.missingSeeThrough;
    }
    return false;
}
bool validRenderEdit(RenderSettingsEdit const& edit) {
    if (!edit.id || edit.changes.empty() || edit.changes.size() > 8 || structure::toInt(edit.axis) < 0 || structure::toInt(edit.axis) > 2) return false;
    std::set<RenderField> seen;
    for (auto const& c : edit.changes) {
        if (!seen.insert(c.field).second) return false;
        auto valid = [&](RenderValue const& value) {
            auto integerIn = [&](int maximum) {
                auto n = std::get_if<int>(&value);
                return n && *n >= 0 && *n <= maximum;
            };
            switch (c.field) {
            case RenderField::Bounds: case RenderField::CorrectionSeeThrough: case RenderField::MissingSeeThrough:
                return std::holds_alternative<bool>(value);
            case RenderField::Axis: return integerIn(2);
            case RenderField::Mode: return integerIn(3);
            case RenderField::Layer: return integerIn(2147483647);
            case RenderField::Fill: case RenderField::Outline: return integerIn(100);
            }
            return false;
        };
        if (!valid(c.expected) || !valid(c.value)) return false;
    }
    return true;
}
bool submitRenderSettingsEdit(RenderSettingsEdit const& edit) {
    std::lock_guard lock(mutex);
    if (!validRenderEdit(edit) || edit.id <= ack || (pending && (edit.id <= pending->id || edit.context <= pending->context))) return false;
    pending = edit;
    return true;
}
RenderSettingsState getRenderSettingsState() { std::lock_guard lock(mutex); return snapshot(); }
bool processRenderSettingsEdits() {
    std::lock_guard lock(mutex);
    if (!pending) return false;
    auto edit = *std::exchange(pending, std::nullopt);
    ack = edit.id; accepted = false;
    auto const before = snapshot();
    if (edit.context != before.context) return false;
    auto layers = before.values.layers;
    bool touchesLayers{}, changed{};
    for (auto const& c : edit.changes) {
        if (renderValue(before.values, c.field) != c.expected) return false;
        changed = changed || c.value != c.expected;
        switch (c.field) {
        case RenderField::Axis: layers.axis = structure::layerAxisFromInt(std::get<int>(c.value)); touchesLayers = true; break;
        case RenderField::Mode: layers.mode = structure::layerDisplayModeFromInt(std::get<int>(c.value)); touchesLayers = true; break;
        case RenderField::Layer: layers.layer = std::get<int>(c.value); touchesLayers = true; break;
        default: break;
        }
    }
    if (touchesLayers && edit.axis != before.values.layers.axis) return false;
    // Check the complete layer tuple under its owner lock before any visual setter.
    if (!structure::detail::StructureSession::getInstance().applyLayerSettings(edit.context, before.values.layers, layers)) return false;
    for (auto const& c : edit.changes) {
        if (c.value == c.expected) continue;
        switch (c.field) {
        case RenderField::Bounds: projection::setStructureBoundsEnabled(std::get<bool>(c.value)); break;
        case RenderField::Fill: projection::setCorrectionFillOpacity(std::get<int>(c.value) / 100.0f); break;
        case RenderField::Outline: projection::setCorrectionOutlineOpacity(std::get<int>(c.value) / 100.0f); break;
        case RenderField::CorrectionSeeThrough: projection::setCorrectionSeeThrough(std::get<bool>(c.value)); break;
        case RenderField::MissingSeeThrough: projection::setMissingSeeThrough(std::get<bool>(c.value)); break;
        default: break;
        }
    }
    accepted = true;
    return changed;
}
void resetRenderSettingsEdits() {
    std::lock_guard lock(mutex);
    if (pending) ack = pending->id;
    pending.reset(); accepted = false;
}
}
