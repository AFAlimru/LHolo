// LHolo - Client-side projection renderer for Minecraft Bedrock Windows
// Copyright (C) 2026  MarmieQi

#include "ui/MenuController.h"

#include "place/PlaceHelper.h"
#include "plugin/LHolo.h"
#include "projection/Projection.h"
#include "structure/StructurePaths.h"
#include "structure/StructureLoader.h"
#include "structure/StructureSession.h"
#include "structure/StructureUiState.h"
#include "structure/capture/StructureCapture.h"
#include "structure/formats/StructureFormatLoaders.h"
#include "ui/FileDialog.h"
#include "ui/FluentTheme.h"
#include "ui/HotkeyFormat.h"
#include "ui/LHoloMenu.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstddef>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>

#include <Windows.h>

#include "imgui.h"
#include "ll/api/mod/NativeMod.h"

namespace lholo::ui {
namespace {

auto& logger() {
    return LHolo::getInstance().getSelf().getLogger();
}

void stopHotkeyCapture() {
    structure::detail::uiCapturingGuiHotkey().store(false, std::memory_order_release);
    structure::detail::uiCapturingLayerIncreaseHotkey().store(false, std::memory_order_release);
    structure::detail::uiCapturingLayerDecreaseHotkey().store(false, std::memory_order_release);
    for (auto& capturing : structure::detail::uiCapturingMoveHotkey()) {
        capturing.store(false, std::memory_order_release);
    }
}

struct HotkeyDefinition { HotkeyId id; char const* label; };
constexpr std::array<HotkeyDefinition, 9> kHotkeyDefinitions{{
    {HotkeyId::Gui, "打开投影菜单"},
    {HotkeyId::MoveXMinus, "结构偏移 X -1"},
    {HotkeyId::MoveXPlus, "结构偏移 X +1"},
    {HotkeyId::MoveZMinus, "结构偏移 Z -1"},
    {HotkeyId::MoveZPlus, "结构偏移 Z +1"},
    {HotkeyId::MoveYPlus, "结构偏移 Y +1"},
    {HotkeyId::MoveYMinus, "结构偏移 Y -1"},
    {HotkeyId::LayerIncrease, "上一层"},
    {HotkeyId::LayerDecrease, "下一层"}
}};

} // namespace

HotkeyBinding hotkeyBinding(HotkeyId id) {
    switch (id) {
    case HotkeyId::Gui:
        return {&structure::detail::uiGuiHotkey(), &structure::detail::uiGuiHotkeyModifiers(), &structure::detail::uiCapturingGuiHotkey()};
    case HotkeyId::MoveXMinus:
        return {&structure::detail::uiMoveHotkeys()[0], &structure::detail::uiMoveHotkeyModifiers()[0], &structure::detail::uiCapturingMoveHotkey()[0]};
    case HotkeyId::MoveXPlus:
        return {&structure::detail::uiMoveHotkeys()[1], &structure::detail::uiMoveHotkeyModifiers()[1], &structure::detail::uiCapturingMoveHotkey()[1]};
    case HotkeyId::MoveZMinus:
        return {&structure::detail::uiMoveHotkeys()[2], &structure::detail::uiMoveHotkeyModifiers()[2], &structure::detail::uiCapturingMoveHotkey()[2]};
    case HotkeyId::MoveZPlus:
        return {&structure::detail::uiMoveHotkeys()[3], &structure::detail::uiMoveHotkeyModifiers()[3], &structure::detail::uiCapturingMoveHotkey()[3]};
    case HotkeyId::MoveYPlus:
        return {&structure::detail::uiMoveHotkeys()[4], &structure::detail::uiMoveHotkeyModifiers()[4], &structure::detail::uiCapturingMoveHotkey()[4]};
    case HotkeyId::MoveYMinus:
        return {&structure::detail::uiMoveHotkeys()[5], &structure::detail::uiMoveHotkeyModifiers()[5], &structure::detail::uiCapturingMoveHotkey()[5]};
    case HotkeyId::LayerIncrease:
        return {&structure::detail::uiLayerIncreaseHotkey(), &structure::detail::uiLayerIncreaseHotkeyModifiers(), &structure::detail::uiCapturingLayerIncreaseHotkey()};
    case HotkeyId::LayerDecrease:
        return {&structure::detail::uiLayerDecreaseHotkey(), &structure::detail::uiLayerDecreaseHotkeyModifiers(), &structure::detail::uiCapturingLayerDecreaseHotkey()};
    }
    return {};
}

MenuModel buildStructureMenuModel(float effectiveUiScale) {
    MenuModel model;
    model.page = structure::detail::uiActivePage();
    model.pathBuffer = structure::detail::uiPathBuffer().data();
    model.pathBufferSize = structure::detail::uiPathBuffer().size();
    model.blockOpeningInput
        = structure::detail::uiOpeningInputBlockFrames().load(std::memory_order_acquire) > 0;
    model.uiScale = effectiveUiScale;
    auto const captureSnapshot = structure::capture::getSnapshot();
    model.capture.mode = static_cast<int>(captureSnapshot.draft.mode);
    model.captureRevision = captureSnapshot.revision;
    model.capture.includeEntities = captureSnapshot.draft.includeEntities;
    model.captureWorldAvailable = captureSnapshot.worldAvailable;
    model.captureStatus = captureSnapshot.status;
    if (captureSnapshot.draft.first) {
        auto const& point = *captureSnapshot.draft.first;
        model.capture.first = {true, point.x, point.y, point.z};
    }
    if (captureSnapshot.draft.second) {
        auto const& point = *captureSnapshot.draft.second;
        model.capture.second = {true, point.x, point.y, point.z};
    }
    model.layerAxis = std::clamp(
        structure::detail::sessionLayerAxis().load(std::memory_order_relaxed), 0, 1
    );

    {
        std::lock_guard lock(structure::detail::sessionLoadedMutex());
        model.status = structure::detail::sessionStatus();
        model.hasLoadedStructure = static_cast<bool>(structure::detail::sessionLoaded());
        model.hasSavedProjection
            = structure::detail::sessionHasSavedProjection().load(std::memory_order_relaxed);
        model.savedAnchorX
            = structure::detail::sessionSavedAnchorX().load(std::memory_order_relaxed);
        model.savedAnchorY
            = structure::detail::sessionSavedAnchorY().load(std::memory_order_relaxed);
        model.savedAnchorZ
            = structure::detail::sessionSavedAnchorZ().load(std::memory_order_relaxed);
        if (structure::detail::sessionLoaded()) {
            model.maxLayerY
                = structure::detail::maxLayerFor(*structure::detail::sessionLoaded(), 0);
            model.maxLayerX
                = structure::detail::maxLayerFor(*structure::detail::sessionLoaded(), 1);
        }
    }
    model.structureBoundsEnabled = projection::getStructureBoundsEnabled();
    model.easyPlaceEnabled = place::isEnabled();
    model.manualPlace = place::isManualMode();
    model.rangeEnabled = place::isRangeEnabled();
    model.placementRadius = place::getPlacementRadius();
    model.offsetX = structure::detail::sessionOffsetX().load(std::memory_order_relaxed);
    model.offsetY = structure::detail::sessionOffsetY().load(std::memory_order_relaxed);
    model.offsetZ = structure::detail::sessionOffsetZ().load(std::memory_order_relaxed);
    model.rotation = std::clamp(
        structure::detail::sessionRotationQuarterTurns().load(std::memory_order_relaxed), 0, 3
    );
    model.mirror = std::clamp(
        structure::detail::sessionMirror().load(std::memory_order_relaxed), 0, 2
    );
    model.opacity = projection::getOpacity();
    model.correctionFillOpacity = projection::getCorrectionFillOpacity();
    model.correctionOutlineOpacity = projection::getCorrectionOutlineOpacity();
    model.layerDisplayMode = std::clamp(
        structure::detail::sessionLayerDisplayMode().load(std::memory_order_relaxed), 0, 3
    );
    model.displayLayer = std::clamp(
        structure::detail::sessionDisplayLayer().load(std::memory_order_relaxed), 0,
        model.layerAxis == 1 ? model.maxLayerX : model.maxLayerY
    );
    model.hudEnabled = structure::detail::uiHudEnabled().load(std::memory_order_relaxed);
    model.hudPosition
        = std::clamp(structure::detail::uiHudPosition().load(std::memory_order_relaxed), 0, 3);
    model.hudShowFileName = structure::detail::uiHudShowFileName().load(std::memory_order_relaxed);
    model.hudShowLayer = structure::detail::uiHudShowLayer().load(std::memory_order_relaxed);
    model.hudShowOverallProgress
        = structure::detail::uiHudShowOverallProgress().load(std::memory_order_relaxed);
    model.hudShowProgress = structure::detail::uiHudShowProgress().load(std::memory_order_relaxed);
    model.hudShowWrongState
        = structure::detail::uiHudShowWrongState().load(std::memory_order_relaxed);
    model.hudShowWrongType
        = structure::detail::uiHudShowWrongType().load(std::memory_order_relaxed);
    model.hudShowBlockEntity
        = structure::detail::uiHudShowBlockEntity().load(std::memory_order_relaxed);
    for (auto const& definition : kHotkeyDefinitions) {
        auto const binding = hotkeyBinding(definition.id);
        auto& row = model.hotkeys[static_cast<std::size_t>(definition.id)];
        row.id = definition.id;
        row.label = definition.label;
        row.display = hotkeyChordName(
            binding.modifiers->load(std::memory_order_relaxed),
            binding.key->load(std::memory_order_relaxed)
        );
        row.capturing = binding.capturing->load(std::memory_order_acquire);
    }
    {
        std::lock_guard lock(structure::detail::uiMaterialMutex());
        model.materials.reserve(structure::detail::uiMaterialRequirements().size());
        for (auto const& material : structure::detail::uiMaterialRequirements()) {
            model.materials.push_back({material.displayName, material.typeName, material.count});
        }
    }
    return model;
}

void applyStructureMenuModel(MenuModel const& model, float effectiveUiScale) {
    bool changed = false;
    auto update = [&changed](auto& target, auto value) {
        if (target.load(std::memory_order_relaxed) == value) return;
        target.store(value, std::memory_order_relaxed);
        changed = true;
    };
    if (std::abs(model.uiScale - effectiveUiScale) > 0.001f) {
        auto const scale = std::clamp(model.uiScale, 1.0f, 5.0f);
        if (std::abs(structure::detail::uiUiScale().load(std::memory_order_relaxed) - scale)
            > 0.001f) {
            structure::detail::uiUiScale().store(scale, std::memory_order_relaxed);
            changed = true;
        }
    }
    if (projection::getStructureBoundsEnabled() != model.structureBoundsEnabled) {
        projection::setStructureBoundsEnabled(model.structureBoundsEnabled);
        changed = true;
    }
    // Assisted-placement modes are session-only safety controls. Applying a
    // mode must not dirty or rewrite the persistent settings file.
    if (place::isEnabled() != model.easyPlaceEnabled) place::setEnabled(model.easyPlaceEnabled);
    if (place::isManualMode() != model.manualPlace) place::setManualMode(model.manualPlace);
    if (place::isRangeEnabled() != model.rangeEnabled) place::setRangeEnabled(model.rangeEnabled);
    auto const radius = std::clamp(model.placementRadius, 1, 4);
    if (place::getPlacementRadius() != radius) {
        place::setPlacementRadius(radius);
        changed = true;
    }
    update(structure::detail::sessionOffsetX(), model.offsetX);
    update(structure::detail::sessionOffsetY(), model.offsetY);
    update(structure::detail::sessionOffsetZ(), model.offsetZ);
    update(
        structure::detail::sessionRotationQuarterTurns(), std::clamp(model.rotation, 0, 3)
    );
    update(structure::detail::sessionMirror(), std::clamp(model.mirror, 0, 2));

    auto const opacity = std::clamp(model.opacity, 0.0f, 1.0f);
    if (std::abs(projection::getOpacity() - opacity) > 0.0001f) {
        projection::setOpacity(opacity);
        changed = true;
    }
    auto const fill = std::clamp(model.correctionFillOpacity, 0.0f, 1.0f);
    if (std::abs(projection::getCorrectionFillOpacity() - fill) > 0.0001f) {
        projection::setCorrectionFillOpacity(fill);
        changed = true;
    }
    auto const outline = std::clamp(model.correctionOutlineOpacity, 0.0f, 1.0f);
    if (std::abs(projection::getCorrectionOutlineOpacity() - outline) > 0.0001f) {
        projection::setCorrectionOutlineOpacity(outline);
        changed = true;
    }
    auto const layerAxis = std::clamp(model.layerAxis, 0, 1);
    update(structure::detail::sessionLayerAxis(), layerAxis);
    update(
        structure::detail::sessionLayerDisplayMode(), std::clamp(model.layerDisplayMode, 0, 3)
    );
    auto displayMax = 0;
    {
        std::lock_guard lock(structure::detail::sessionLoadedMutex());
        if (structure::detail::sessionLoaded()) {
            displayMax = structure::detail::maxLayerFor(
                *structure::detail::sessionLoaded(), layerAxis
            );
        }
    }
    update(
        structure::detail::sessionDisplayLayer(), std::clamp(model.displayLayer, 0, displayMax)
    );
    update(structure::detail::uiHudEnabled(), model.hudEnabled);
    update(structure::detail::uiHudPosition(), std::clamp(model.hudPosition, 0, 3));
    update(structure::detail::uiHudShowFileName(), model.hudShowFileName);
    update(structure::detail::uiHudShowLayer(), model.hudShowLayer);
    update(structure::detail::uiHudShowOverallProgress(), model.hudShowOverallProgress);
    update(structure::detail::uiHudShowProgress(), model.hudShowProgress);
    update(structure::detail::uiHudShowWrongState(), model.hudShowWrongState);
    update(structure::detail::uiHudShowWrongType(), model.hudShowWrongType);
    update(structure::detail::uiHudShowBlockEntity(), model.hudShowBlockEntity);
    structure::capture::Draft captureDraft;
    captureDraft.mode = static_cast<structure::capture::CaptureMode>(
        std::clamp(model.capture.mode, 0, 1)
    );
    captureDraft.includeEntities = model.capture.includeEntities;
    if (model.capture.first.set) {
        captureDraft.first = structure::capture::Point{
            model.capture.first.x, model.capture.first.y, model.capture.first.z
        };
    }
    if (model.capture.second.set) {
        captureDraft.second = structure::capture::Point{
            model.capture.second.x, model.capture.second.y, model.capture.second.z
        };
    }
    structure::capture::updateDraft(captureDraft);
    if (changed) structure::saveSettings();
}

MenuActions buildStructureMenuActions(bool& refreshModel) {
    MenuActions actions;
    actions.browseStructure = [](std::string_view current) -> std::optional<std::string> {
        auto const selected = openStructureFile(structure::detail::pathFromUtf8(current));
        return selected ? std::optional<std::string>{structure::detail::pathToUtf8(*selected)}
                        : std::nullopt;
    };
    actions.loadStructure = [&refreshModel](std::string_view pathValue) {
        auto const pathText = std::string{pathValue};
        if (pathText.empty()) {
            std::lock_guard lock(structure::detail::sessionLoadedMutex());
            structure::detail::sessionStatus() = "请选择或输入 .mcstructure / .litematic 文件路径";
            return;
        }
        std::string error;
        auto loaded = structure::detail::loadStructureFile(
            structure::detail::pathFromUtf8(pathText), error
        );
        if (!loaded) {
            std::lock_guard lock(structure::detail::sessionLoadedMutex());
            structure::detail::sessionStatus() = "加载失败: " + error;
            logger().error("Could not load structure {}: {}", pathText, error);
            return;
        }
        std::string status;
        {
            std::lock_guard lock(structure::detail::sessionLoadedMutex());
            structure::detail::sessionLastPath() = pathText;
            structure::detail::sessionStatus() = structure::detail::makeStructureStatus(*loaded);
            status = structure::detail::sessionStatus();
            structure::detail::sessionLoaded() = std::move(loaded);
        }
        structure::saveSettings();
        refreshModel = true;
        logger().info("{}", status);
    };
    actions.restoreProjection = [&refreshModel] {
        std::string savedPath;
        {
            std::lock_guard lock(structure::detail::sessionLoadedMutex());
            savedPath = structure::detail::sessionSavedStructurePath();
        }
        auto const x = structure::detail::sessionSavedAnchorX().load(std::memory_order_relaxed);
        auto const y = structure::detail::sessionSavedAnchorY().load(std::memory_order_relaxed);
        auto const z = structure::detail::sessionSavedAnchorZ().load(std::memory_order_relaxed);
        std::string error;
        auto loaded = structure::detail::loadStructureFile(
            structure::detail::pathFromUtf8(savedPath), error
        );
        if (!loaded) {
            std::lock_guard lock(structure::detail::sessionLoadedMutex());
            structure::detail::sessionStatus() = "恢复失败: " + error;
            logger().error("Could not restore structure {}: {}", savedPath, error);
            return;
        }
        structure::detail::sessionRotationQuarterTurns().store(
            structure::detail::sessionSavedRotation().load(std::memory_order_relaxed),
            std::memory_order_relaxed
        );
        structure::detail::sessionMirror().store(
            std::clamp(
                structure::detail::sessionSavedMirror().load(std::memory_order_relaxed), 0, 2
            ),
            std::memory_order_relaxed
        );
        structure::detail::sessionOffsetX().store(
            structure::detail::sessionSavedOffsetX().load(std::memory_order_relaxed),
            std::memory_order_relaxed
        );
        structure::detail::sessionOffsetY().store(
            structure::detail::sessionSavedOffsetY().load(std::memory_order_relaxed),
            std::memory_order_relaxed
        );
        structure::detail::sessionOffsetZ().store(
            structure::detail::sessionSavedOffsetZ().load(std::memory_order_relaxed),
            std::memory_order_relaxed
        );
        structure::detail::sessionLayerDisplayMode().store(
            structure::detail::sessionSavedLayerDisplayMode().load(std::memory_order_relaxed),
            std::memory_order_relaxed
        );
        structure::detail::sessionDisplayLayer().store(
            structure::detail::sessionSavedDisplayLayer().load(std::memory_order_relaxed),
            std::memory_order_relaxed
        );
        structure::detail::sessionLayerAxis().store(
            structure::detail::sessionSavedLayerAxis().load(std::memory_order_relaxed),
            std::memory_order_relaxed
        );
        projection::requestNextStructureAnchor(x, y, z);
        {
            std::lock_guard lock(structure::detail::sessionLoadedMutex());
            structure::detail::sessionLastPath() = savedPath;
            structure::detail::sessionStatus() = "已恢复上次投影记录，等待进入渲染";
            structure::detail::sessionLoaded() = std::move(loaded);
        }
        std::snprintf(
            structure::detail::uiPathBuffer().data(),
            structure::detail::uiPathBuffer().size(),
            "%s",
            savedPath.c_str()
        );
        refreshModel = true;
        logger().info("Restoring projection {} at ({}, {}, {})", savedPath, x, y, z);
    };
    actions.closeProjection = [&refreshModel] {
        structure::clear();
        refreshModel = true;
    };
    actions.requestMaterials = [] { structure::requestMaterialList(); };
    actions.beginHotkeyCapture = [](HotkeyId id) {
        stopHotkeyCapture();
        if (auto const binding = hotkeyBinding(id); binding.capturing) {
            binding.capturing->store(true, std::memory_order_release);
        }
    };
    actions.clearHotkey = [](HotkeyId id) {
        if (auto const binding = hotkeyBinding(id); binding.key && binding.modifiers) {
            binding.key->store(0, std::memory_order_release);
            binding.modifiers->store(0, std::memory_order_release);
            if (binding.capturing) binding.capturing->store(false, std::memory_order_release);
            structure::saveSettings();
        }
    };
    actions.resetHotkeys = [] {
        structure::detail::uiGuiHotkey().store('M', std::memory_order_relaxed);
        structure::detail::uiGuiHotkeyModifiers().store(
            kHotkeyModifierAlt, std::memory_order_relaxed
        );
        static unsigned int const moveKeys[]{VK_LEFT, VK_RIGHT, VK_UP, VK_DOWN, VK_UP, VK_DOWN};
        static unsigned int const moveModifiers[]{
            kHotkeyModifierControl,
            kHotkeyModifierControl,
            kHotkeyModifierControl,
            kHotkeyModifierControl,
            kHotkeyModifierShift,
            kHotkeyModifierShift
        };
        for (std::size_t index = 0; index < structure::detail::uiMoveHotkeys().size(); ++index) {
            structure::detail::uiMoveHotkeys()[index].store(
                moveKeys[index], std::memory_order_relaxed
            );
            structure::detail::uiMoveHotkeyModifiers()[index].store(
                moveModifiers[index], std::memory_order_relaxed
            );
        }
        structure::detail::uiLayerIncreaseHotkey().store(VK_UP, std::memory_order_relaxed);
        structure::detail::uiLayerDecreaseHotkey().store(VK_DOWN, std::memory_order_relaxed);
        structure::detail::uiLayerIncreaseHotkeyModifiers().store(
            kHotkeyModifierAlt, std::memory_order_relaxed
        );
        structure::detail::uiLayerDecreaseHotkeyModifiers().store(
            kHotkeyModifierAlt, std::memory_order_relaxed
        );
        stopHotkeyCapture();
        structure::resetHotkeyState();
        structure::saveSettings();
    };
    actions.resetCorrectionStyle = [] {
        projection::setCorrectionFillOpacity(0.15f);
        projection::setCorrectionOutlineOpacity(1.0f);
        structure::saveSettings();
    };
    actions.usePlayerCapturePosition = [&refreshModel](CapturePointId point) {
        structure::capture::setPointFromPlayer(
            point == CapturePointId::First
                ? structure::capture::PointSlot::First
                : structure::capture::PointSlot::Second
        );
        refreshModel = true;
    };
    actions.clearCapture = [&refreshModel] {
        structure::capture::clear();
        refreshModel = true;
    };
    actions.exportCapture = [&refreshModel](CaptureDraftModel const& model) {
        auto const output = saveMcstructureFile();
        if (!output) return;
        structure::capture::Draft draft;
        draft.mode = static_cast<structure::capture::CaptureMode>(
            std::clamp(model.mode, 0, 1)
        );
        draft.includeEntities = model.includeEntities;
        if (model.first.set) {
            draft.first = structure::capture::Point{
                model.first.x, model.first.y, model.first.z
            };
        }
        if (model.second.set) {
            draft.second = structure::capture::Point{
                model.second.x, model.second.y, model.second.z
            };
        }
        structure::capture::exportStructure(draft, *output);
        refreshModel = true;
    };
    return actions;
}

void renderStructureMenu() {
    if (!structure::isGuiVisible()) return;
    auto const displaySize = ImGui::GetIO().DisplaySize;
    auto const configuredScale
        = structure::detail::uiUiScale().load(std::memory_order_relaxed);
    auto const effectiveScale = configuredScale > 0.0f
        ? std::clamp(configuredScale, 1.0f, 5.0f)
        : std::clamp(
            std::min(displaySize.x / 1920.0f, displaySize.y / 1080.0f), 1.0f, 5.0f
        );
    if (!structure::detail::uiPathInitialized()) {
        std::lock_guard lock(structure::detail::sessionLoadedMutex());
        std::snprintf(
            structure::detail::uiPathBuffer().data(),
            structure::detail::uiPathBuffer().size(),
            "%s",
            structure::detail::sessionLastPath().c_str()
        );
        structure::detail::uiPathInitialized() = true;
    }
    auto const metrics = calculateMetrics(displaySize, effectiveScale);
    applyFluentTheme(metrics);
    auto model = buildStructureMenuModel(effectiveScale);
    bool refreshModel = false;
    auto const actions = buildStructureMenuActions(refreshModel);
    renderMenu(model, actions, metrics);
    structure::detail::uiActivePage() = model.page;
    if (!refreshModel) applyStructureMenuModel(model, effectiveScale);
    if (structure::detail::uiOpeningInputBlockFrames().load(std::memory_order_acquire) > 0) {
        structure::detail::uiOpeningInputBlockFrames().fetch_sub(1, std::memory_order_acq_rel);
    }
    if (model.closeRequested) {
        structure::detail::uiGuiVisible().store(false, std::memory_order_release);
        structure::detail::uiBlockGameInputUntil().store(
            GetTickCount64() + 180, std::memory_order_release
        );
    }
}

} // namespace lholo::ui
