// LHolo - Client-side projection renderer for Minecraft Bedrock Windows
// Copyright (C) 2026  MarmieQi
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#include "structure/StructureLoader.h"

#include "settings/SettingsStore.h"
#include "structure/formats/StructureFormatLoaders.h"
#include "structure/StructureSession.h"
#include "structure/StructurePaths.h"
#include "structure/StructureUiState.h"
#include "ui/HotkeyFormat.h"
#include "ui/MenuController.h"
#include "structure/capture/StructureCapture.h"
#include "structure/java_to_bedrock/JavaToBedrock.h"
#include "ui/FileDialog.h"
#include "ui/FluentTheme.h"
#include "ui/LHoloMenu.h"
#include "place/PlaceHelper.h"
#include "plugin/LHolo.h"
#include "projection/Projection.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <cwctype>
#include <fstream>
#include <iterator>
#include <limits>
#include <map>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <tuple>
#include <unordered_map>
#include <variant>
#include <vector>

#include <Windows.h>

#include "ll/api/mod/NativeMod.h"
#include "ll/api/service/Bedrock.h"
#include "imgui.h"
#include "mc/deps/core/string/HashedString.h"
#include "mc/client/game/ClientInstance.h"
#include "mc/client/game/IClientInstance.h"
#include "mc/client/player/LocalPlayer.h"
#include "mc/world/item/ItemStack.h"
#include "mc/world/item/Item.h"
#include "mc/world/item/registry/ItemRegistry.h"
#include "mc/world/item/registry/ItemRegistryManager.h"
#include "mc/world/level/Level.h"
#include "mc/world/level/block/Block.h"
#include "mc/world/level/material/Material.h"
#include "mc/locale/I18n.h"

namespace lholo::structure {
namespace {

using detail::MaterialRequirement;

constexpr char           kMaterialPopupName[]       = "材料清单###LHoloMaterialList";
constexpr std::size_t    kGuiHotkeyIndex             = 0;
constexpr std::size_t    kLayerIncreaseHotkeyIndex   = detail::StructureUiState::kHotkeyCount - 2;
constexpr std::size_t    kLayerDecreaseHotkeyIndex   = detail::StructureUiState::kHotkeyCount - 1;


std::string localizedBlockName(Block const& block, std::string_view localeCode) {
    auto const& typeName = block.getTypeName();
    auto const itemId = ItemRegistry::getBlockItemId(block);
    auto const item = ItemRegistryManager::getItemRegistry().getItem(itemId);
    if (auto* itemPtr = item.get()) {
        ItemStack const itemStack(*itemPtr, 1, 0, nullptr);
        auto const name = itemStack.getName();
        if (!name.empty() && name != typeName) return name;
    }

    auto const translationKey = block.buildDescriptionName();
    if (!translationKey.empty()) {
        auto& i18n = ::getI18n();
        auto locale = localeCode.empty()
            ? i18n.getCurrentLanguage().get()
            : i18n.getLocaleFor(std::string{localeCode});
        if (locale) {
            auto const localized = i18n.get(
                translationKey,
                std::vector<std::string>{},
                locale
            );
            if (!localized.empty() && localized != translationKey) return localized;
        }
    }

    auto name = block.getDisplayName();
    if (name.empty()) name = typeName;
    return name;
}

std::vector<MaterialRequirement> collectMaterials(
    std::vector<LoadedStructure::RenderBlock> const& renderBlocks,
    std::string_view localeCode
) {
    std::map<std::string, MaterialRequirement> byType;
    std::map<std::string, MaterialRequirement> byLiquidType;
    auto aggregate = [&](auto& destination, Block const* block) {
        if (!block) return;

        auto const& typeName = block->getTypeName();
        auto [it, inserted] = destination.try_emplace(typeName);
        if (inserted) {
            it->second.displayName = localizedBlockName(*block, localeCode);
            it->second.typeName = typeName;
        }
        if (it->second.count != std::numeric_limits<std::uint64_t>::max()) {
            ++it->second.count;
        }
    };

    for (auto const& entry : renderBlocks) {
        aggregate(byType, entry.block);
        aggregate(byLiquidType, entry.liquid);
    }

    std::vector<MaterialRequirement> materials;
    auto appendSorted = [&materials](auto& source) {
        std::vector<MaterialRequirement> sorted;
        sorted.reserve(source.size());
        for (auto& entry : source) sorted.push_back(std::move(entry.second));
        std::sort(sorted.begin(), sorted.end(), [](auto const& left, auto const& right) {
            if (left.count != right.count) return left.count > right.count;
            return left.typeName < right.typeName;
        });
        materials.insert(
            materials.end(),
            std::make_move_iterator(sorted.begin()),
            std::make_move_iterator(sorted.end())
        );
    };
    materials.reserve(byType.size() + byLiquidType.size());
    appendSorted(byType);
    appendSorted(byLiquidType);
    return materials;
}

auto& logger() {
    return LHolo::getInstance().getSelf().getLogger();
}

auto& uiState() {
    return detail::StructureUiState::getInstance();
}

std::filesystem::path settingsPath() {
    return LHolo::getInstance().getSelf().getConfigDir() / "config.json";
}


unsigned int currentHotkeyModifiers() {
    return uiState().currentHotkeyModifiers();
}

} // namespace

void requestMaterialList() {
    uiState().requestMaterialList();
}

void processPendingMaterialList() {
    if (!uiState().consumeMaterialListRequest()) return;

    auto const loaded = getLoaded();
    std::string localeCode;
    if (auto client = ll::service::getClientInstance()) {
        if (auto* player = client->getLocalPlayer()) localeCode = player->getLocaleCode();
    }

    std::vector<MaterialRequirement> materials;
    if (loaded) materials = collectMaterials(loaded->renderBlocks, localeCode);

    uiState().replaceMaterialRequirements(std::move(materials));
}

void requestOpenGui() {
    auto const opening = uiState().toggleGuiVisible();
    if (opening) {
        uiState().setOpeningInputBlockFrames(3);
    } else {
        // Consume the release half of the key/click that closed the menu.
        // Without this, Minecraft receives an unmatched Esc or mouse-up after
        // the ImGui window has already disappeared.
        uiState().setBlockGameInputUntil(GetTickCount64() + 180);
    }
}

bool isGuiVisible() { return uiState().guiVisible(); }

bool shouldShowProjectedBlockName() {
    auto const hud = uiState().hud();
    return hud.enabled && hud.showProjectedBlockName;
}

bool isInputTransitionBlocked() {
    return GetTickCount64() <= uiState().blockGameInputUntil();
}

bool handleGuiHotkeyKeyDown(unsigned int virtualKey) {
    auto const modifierKey = ui::isModifierKey(virtualKey);
    if (virtualKey == VK_CONTROL || virtualKey == VK_LCONTROL || virtualKey == VK_RCONTROL) {
        uiState().setControlHeld(true);
    } else if (virtualKey == VK_MENU || virtualKey == VK_LMENU || virtualKey == VK_RMENU) {
        uiState().setAltHeld(true);
    } else if (virtualKey == VK_SHIFT || virtualKey == VK_LSHIFT || virtualKey == VK_RSHIFT) {
        uiState().setShiftHeld(true);
    }

    auto const captureIndex = uiState().capturingHotkey();
    if (captureIndex) {
        // F11 belongs to Minecraft's fullscreen toggle. Never capture or
        // consume it as a mod shortcut, including while rebinding controls.
        if (virtualKey == VK_F11) return false;
        if (virtualKey == VK_ESCAPE) {
            uiState().stopHotkeyCapture();
        } else if (virtualKey == VK_DELETE || virtualKey == VK_BACK) {
            uiState().clearHotkey(*captureIndex);
            uiState().stopHotkeyCapture();
            uiState().requestSettingsSave();
        } else if (!modifierKey) {
            auto const modifiers = currentHotkeyModifiers();
            uiState().bindCapturedHotkey(*captureIndex, virtualKey, modifiers);
            uiState().setIgnoreHotkeyUntil(GetTickCount64() + 250);
            uiState().requestSettingsSave();
        }
        return true;
    }

    if (modifierKey) return false;

    auto const modifiers = currentHotkeyModifiers();
    auto const guiHotkey = uiState().inputHotkey(kGuiHotkeyIndex);
    if (guiHotkey.key != 0 && virtualKey == guiHotkey.key
        && modifiers == guiHotkey.modifiers) {
        if (GetTickCount64() >= uiState().ignoreHotkeyUntil()
            && uiState().tryPressHotkey(kGuiHotkeyIndex)) {
            requestOpenGui();
        }
        return true;
    }
    if (isGuiVisible()) return false;

    for (std::size_t index = 0; index < detail::StructureUiState::kMoveHotkeyCount; ++index) {
        auto const hotkey = uiState().inputHotkey(index + 1);
        if (hotkey.key == virtualKey && hotkey.modifiers == modifiers) {
            if (GetTickCount64() >= uiState().ignoreHotkeyUntil()
                && uiState().tryPressHotkey(index + 1)) {
                uiState().queueMove(index);
            }
            return true;
        }
    }

    auto const layerIncreaseHotkey = uiState().inputHotkey(kLayerIncreaseHotkeyIndex);
    if (layerIncreaseHotkey.key != 0 && virtualKey == layerIncreaseHotkey.key
        && modifiers == layerIncreaseHotkey.modifiers) {
        if (!detail::StructureSession::getInstance().layerDisplayEnabled()) return false;
        if (GetTickCount64() >= uiState().ignoreHotkeyUntil()
            && uiState().tryPressHotkey(kLayerIncreaseHotkeyIndex)) {
            uiState().queueLayerDelta(1);
        }
        return true;
    }
    auto const layerDecreaseHotkey = uiState().inputHotkey(kLayerDecreaseHotkeyIndex);
    if (layerDecreaseHotkey.key != 0 && virtualKey == layerDecreaseHotkey.key
        && modifiers == layerDecreaseHotkey.modifiers) {
        if (!detail::StructureSession::getInstance().layerDisplayEnabled()) return false;
        if (GetTickCount64() >= uiState().ignoreHotkeyUntil()
            && uiState().tryPressHotkey(kLayerDecreaseHotkeyIndex)) {
            uiState().queueLayerDelta(-1);
        }
        return true;
    }
    return false;
}

bool handleGuiHotkeyKeyUp(unsigned int virtualKey) {
    if (virtualKey == VK_CONTROL || virtualKey == VK_LCONTROL || virtualKey == VK_RCONTROL) {
        uiState().setControlHeld(false);
        return false;
    }
    if (virtualKey == VK_MENU || virtualKey == VK_LMENU || virtualKey == VK_RMENU) {
        uiState().setAltHeld(false);
        return false;
    }
    if (virtualKey == VK_SHIFT || virtualKey == VK_LSHIFT || virtualKey == VK_RSHIFT) {
        uiState().setShiftHeld(false);
        return false;
    }

    return uiState().releaseHotkeysForKey(virtualKey, GetTickCount64());
}

void resetHotkeyState() {
    uiState().resetHotkeyState();
}

void processPendingHotkeyActions() {
    auto& session = detail::StructureSession::getInstance();
    auto const pending = uiState().consumePendingHotkeyActions();
    auto const layerActionEnabled = pending.layerDelta != 0 && session.transform().layerDisplayMode != 0;
    bool changed = pending.offsetX != 0 || pending.offsetY != 0 || pending.offsetZ != 0 || layerActionEnabled;
    session.adjustOffsets(pending.offsetX, pending.offsetY, pending.offsetZ);
    if (layerActionEnabled) session.adjustDisplayLayer(pending.layerDelta);

    changed = pending.settingsSave || changed;
    if (changed) saveSettings();
}

bool hasHudInfo() {
    auto const hud = uiState().hud();
    if (!hud.enabled) return false;
    if (!hud.showFileName
        && !hud.showLayer
        && !hud.showOverallProgress
        && !hud.showProgress
        && !hud.showWrongState
        && !hud.showWrongType
        && !hud.showProjectedBlockName) return false;
    return detail::StructureSession::getInstance().hasLoaded();
}

void renderHud() {
    if (isGuiVisible()) return;
    auto const hud = uiState().hud();
    if (!hud.enabled) return;
    auto const showFileName = hud.showFileName;
    auto const showLayer = hud.showLayer;
    auto const showOverallProgress = hud.showOverallProgress;
    auto const showProgress = hud.showProgress;
    auto const showWrongState = hud.showWrongState;
    auto const showWrongType = hud.showWrongType;
    auto const showProjectedBlockName = hud.showProjectedBlockName;
    if (!showFileName && !showLayer && !showOverallProgress && !showProgress
        && !showWrongState && !showWrongType && !showProjectedBlockName) return;

    auto const sessionSnapshot = detail::StructureSession::getInstance().snapshot();
    if (!sessionSnapshot.loaded) return;
    auto const fileName = detail::pathToUtf8(sessionSnapshot.loaded->sourcePath.filename());
    auto const layerAxis = sessionSnapshot.transform.layerAxis;
    auto const maxLayer = layerAxis == 1 ? sessionSnapshot.maxLayerX : sessionSnapshot.maxLayerY;

    auto const displaySize = ImGui::GetIO().DisplaySize;
    auto uiScale = hud.uiScale;
    if (uiScale <= 0.0f) {
        uiScale = std::clamp(
            std::min(displaySize.x / 1920.0f, displaySize.y / 1080.0f),
            1.0f,
            5.0f
        );
    }
    auto const hudMetrics = lholo::ui::calculateMetrics(displaySize, uiScale);
    lholo::ui::applyFluentTheme(hudMetrics);
    auto const layerMode = sessionSnapshot.transform.layerDisplayMode;
    auto const currentLayer = std::clamp(
        sessionSnapshot.transform.displayLayer,
        0,
        maxLayer
    );

    auto const hudPosition = std::clamp(hud.position, 0, 3);
    auto const right = hudPosition >= 2;
    auto const bottom = (hudPosition & 1) != 0;
    auto const margin = hudMetrics.outerPadding;
    ImGui::SetNextWindowPos(
        ImVec2(right ? displaySize.x - margin : margin, bottom ? displaySize.y - margin : margin),
        ImGuiCond_Always,
        ImVec2(right ? 1.0f : 0.0f, bottom ? 1.0f : 0.0f)
    );
    ImGui::SetNextWindowBgAlpha(0.68f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, hudMetrics.rounding * 0.7f);
    ImGui::PushStyleVar(
        ImGuiStyleVar_WindowPadding,
        ImVec2(hudMetrics.sectionPadding, hudMetrics.gap)
    );
    constexpr auto flags = ImGuiWindowFlags_NoDecoration
        | ImGuiWindowFlags_AlwaysAutoResize
        | ImGuiWindowFlags_NoSavedSettings
        | ImGuiWindowFlags_NoFocusOnAppearing
        | ImGuiWindowFlags_NoBringToFrontOnFocus
        | ImGuiWindowFlags_NoNavInputs
        | ImGuiWindowFlags_NoNavFocus
        | ImGuiWindowFlags_NoInputs;
    if (ImGui::Begin("##LHoloHud", nullptr, flags)) {
        if (showFileName) ImGui::Text("投影：%s", fileName.c_str());
        if (showLayer && layerMode == 0) {
            ImGui::TextUnformatted("显示范围：完整结构");
        } else if (showLayer && layerMode == 1) {
            ImGui::Text(
                "当前层：%d / %d（%s 轴）",
                currentLayer,
                maxLayer,
                layerAxis == 1 ? "X" : "Y"
            );
        } else if (showLayer && layerMode == 2) {
            ImGui::Text(
                "显示范围：第 0～%d 层（%s 轴）",
                currentLayer,
                layerAxis == 1 ? "X" : "Y"
            );
        } else if (showLayer) {
            ImGui::Text(
                "显示范围：第 %d～%d 层（%s 轴）",
                currentLayer,
                maxLayer,
                layerAxis == 1 ? "X" : "Y"
            );
        }
        if (showOverallProgress || showProgress || showWrongState || showWrongType) {
            auto const progress = projection::getBuildProgress();
            if (showOverallProgress) {
                ImGui::Text(
                    "总体进度：%llu / %llu",
                    static_cast<unsigned long long>(progress.placed),
                    static_cast<unsigned long long>(progress.total)
                );
            }
            if (showProgress) {
                ImGui::Text(
                    "建造进度：%llu / %llu",
                    static_cast<unsigned long long>(progress.visiblePlaced),
                    static_cast<unsigned long long>(progress.visibleTotal)
                );
            }
            if (showWrongState && progress.wrongState != 0) {
                ImGui::TextColored(
                    ImVec4(1.0f, 0.62f, 0.18f, 1.0f),
                    "朝向错误：%llu",
                    static_cast<unsigned long long>(progress.wrongState)
                );
            }
            if (showWrongType && progress.wrongType != 0) {
                ImGui::TextColored(
                    ImVec4(1.0f, 0.28f, 0.24f, 1.0f),
                    "放置错误：%llu",
                    static_cast<unsigned long long>(progress.wrongType)
                );
            }
        }
        auto const aimedProjectedBlock = place::getAimedProjectedBlockName();
        if (showProjectedBlockName && !aimedProjectedBlock.empty()) {
            ImGui::TextColored(
                ImVec4(0.55f, 0.85f, 1.0f, 1.0f),
                "投影方块：%s",
                aimedProjectedBlock.c_str()
            );
        }
    }
    ImGui::End();
    ImGui::PopStyleVar(2);
}

void renderGui() {
    lholo::ui::renderStructureMenu();
}

void loadSettings() {
    auto const path = settingsPath();
    try {
        auto& session = detail::StructureSession::getInstance();
        lholo::settings::Settings settings;
        if (!lholo::settings::loadSettingsFile(path, settings)) {
            saveSettings();
            return;
        }
        session.setLastPath(settings.lastStructurePath);
        uiState().setUiScale(std::clamp(settings.uiScale, 0.0f, 5.0f));
        projection::setOpacity(settings.opacity);
        projection::setCorrectionFillOpacity(settings.correctionFillOpacity);
        projection::setCorrectionOutlineOpacity(settings.correctionOutlineOpacity);
        projection::setStructureBoundsEnabled(settings.structureBoundsEnabled);
        // Transform and layer state are session-local. Only the explicit
        // "restore last projection" record below is persisted.
        session.resetTransform();
        auto hud = uiState().hud();
        hud.enabled = settings.hudEnabled;
        hud.showFileName = settings.hudShowFileName;
        hud.showLayer = settings.hudShowLayer;
        hud.showOverallProgress = settings.hudShowOverallProgress;
        hud.showProgress = settings.hudShowProgress;
        hud.showWrongState = settings.hudShowWrongState;
        hud.showWrongType = settings.hudShowWrongType;
        hud.showProjectedBlockName = settings.hudShowProjectedBlockName;
        hud.position = std::clamp(settings.hudPosition, 0, 3);
        uiState().applyHud(hud);
        // Assisted-placement modes are intentionally session-only. Ignore
        // legacy persisted values and always begin a new game session disabled.
        place::setEnabled(false);
        place::setManualMode(false);
        place::setRangeEnabled(false);
        place::setPlacementRadius(std::clamp(settings.placementRadius, 1, 4));
        uiState().setHotkey(
            kGuiHotkeyIndex,
            std::clamp(settings.guiHotkey, 0, 255),
            std::clamp(settings.guiHotkeyModifiers, 0, 7)
        );
        uiState().setHotkey(
            kLayerIncreaseHotkeyIndex,
            std::clamp(settings.layerIncreaseHotkey, 0, 255),
            std::clamp(settings.layerIncreaseHotkeyModifiers, 0, 7)
        );
        uiState().setHotkey(
            kLayerDecreaseHotkeyIndex,
            std::clamp(settings.layerDecreaseHotkey, 0, 255),
            std::clamp(settings.layerDecreaseHotkeyModifiers, 0, 7)
        );
        for (std::size_t index = 0; index < detail::StructureUiState::kMoveHotkeyCount; ++index) {
            uiState().setHotkey(
                index + 1,
                std::clamp(settings.moveHotkeys[index], 0, 255),
                std::clamp(settings.moveHotkeyModifiers[index], 0, 7)
            );
        }
        session.setSavedProjection({
            settings.hasSavedProjection,
            settings.savedAnchorX,
            settings.savedAnchorY,
            settings.savedAnchorZ,
            {
                settings.savedRotation,
                std::clamp(settings.savedMirror, 0, 2),
                settings.savedOffsetX,
                settings.savedOffsetY,
                settings.savedOffsetZ,
                settings.savedLayerDisplayMode,
                settings.savedDisplayLayer,
                std::clamp(settings.savedLayerAxis, 0, 1)
            },
            settings.savedStructurePath
        });
        logger().info("Loaded projection settings from {}", path.string());
    } catch (std::exception const& exception) {
        logger().error("Could not load projection settings {}: {}", path.string(), exception.what());
    }
}

void saveSettings() {
    auto const path = settingsPath();
    try {
        auto& session = detail::StructureSession::getInstance();
        // Only an active projection may update its restore snapshot. At
        // startup the session-local transform/layer values intentionally
        // reset to defaults; copying those values before the user restores
        // a structure would silently destroy the saved state.
        session.refreshSavedTransformIfActive();
        auto const sessionSnapshot = session.snapshot();
        auto const hud = uiState().hud();
        lholo::settings::Settings settings;
        settings.lastStructurePath = sessionSnapshot.lastPath;
        settings.uiScale = hud.uiScale;
        settings.opacity = projection::getOpacity();
        settings.correctionFillOpacity = projection::getCorrectionFillOpacity();
        settings.correctionOutlineOpacity = projection::getCorrectionOutlineOpacity();
        settings.structureBoundsEnabled = projection::getStructureBoundsEnabled();
        settings.placementRadius = place::getPlacementRadius();
        settings.hudEnabled = hud.enabled;
        settings.hudShowFileName = hud.showFileName;
        settings.hudShowLayer = hud.showLayer;
        settings.hudShowOverallProgress = hud.showOverallProgress;
        settings.hudShowProgress = hud.showProgress;
        settings.hudShowWrongState = hud.showWrongState;
        settings.hudShowWrongType = hud.showWrongType;
        settings.hudShowProjectedBlockName = hud.showProjectedBlockName;
        settings.hudPosition = hud.position;
        auto const guiHotkey = uiState().hotkey(kGuiHotkeyIndex);
        auto const layerIncreaseHotkey = uiState().hotkey(kLayerIncreaseHotkeyIndex);
        auto const layerDecreaseHotkey = uiState().hotkey(kLayerDecreaseHotkeyIndex);
        settings.guiHotkey = guiHotkey.key;
        settings.guiHotkeyModifiers = guiHotkey.modifiers;
        settings.layerIncreaseHotkey = layerIncreaseHotkey.key;
        settings.layerDecreaseHotkey = layerDecreaseHotkey.key;
        settings.layerIncreaseHotkeyModifiers = layerIncreaseHotkey.modifiers;
        settings.layerDecreaseHotkeyModifiers = layerDecreaseHotkey.modifiers;
        for (std::size_t index = 0; index < settings.moveHotkeys.size(); ++index) {
            auto const moveHotkey = uiState().hotkey(index + 1);
            settings.moveHotkeys[index] = moveHotkey.key;
            settings.moveHotkeyModifiers[index] = moveHotkey.modifiers;
        }
        settings.hasSavedProjection = sessionSnapshot.saved.available;
        settings.savedAnchorX = sessionSnapshot.saved.anchorX;
        settings.savedAnchorY = sessionSnapshot.saved.anchorY;
        settings.savedAnchorZ = sessionSnapshot.saved.anchorZ;
        settings.savedRotation = sessionSnapshot.saved.transform.rotation;
        settings.savedMirror = sessionSnapshot.saved.transform.mirror;
        settings.savedOffsetX = sessionSnapshot.saved.transform.offsetX;
        settings.savedOffsetY = sessionSnapshot.saved.transform.offsetY;
        settings.savedOffsetZ = sessionSnapshot.saved.transform.offsetZ;
        settings.savedLayerDisplayMode = sessionSnapshot.saved.transform.layerDisplayMode;
        settings.savedDisplayLayer = sessionSnapshot.saved.transform.displayLayer;
        settings.savedLayerAxis = sessionSnapshot.saved.transform.layerAxis;
        settings.savedStructurePath = sessionSnapshot.saved.structurePath;
        lholo::settings::saveSettingsFile(path, settings);
    } catch (std::exception const& exception) {
        logger().error("Could not save projection settings {}: {}", path.string(), exception.what());
    }
}

std::shared_ptr<LoadedStructure const> getLoaded() {
    return detail::StructureSession::getInstance().loaded();
}

int getRotationQuarterTurns() {
    return detail::StructureSession::getInstance().transform().rotation;
}

int getMirrorMode() {
    return std::clamp(detail::StructureSession::getInstance().transform().mirror, 0, 2);
}

int getOffsetX() { return detail::StructureSession::getInstance().transform().offsetX; }
int getOffsetY() { return detail::StructureSession::getInstance().transform().offsetY; }
int getOffsetZ() { return detail::StructureSession::getInstance().transform().offsetZ; }
int getLayerDisplayMode() { return detail::StructureSession::getInstance().transform().layerDisplayMode; }
int getDisplayLayer() { return detail::StructureSession::getInstance().transform().displayLayer; }
int getLayerAxis() { return detail::StructureSession::getInstance().transform().layerAxis; }

void recordProjectionAnchor(int x, int y, int z) {
    detail::StructureSession::getInstance().recordProjectionAnchor(x, y, z);
    saveSettings();
}

void clear() {
    // Withdraw the requested structure before waiting for the mesh worker.
    // Otherwise the render hook can observe the old loaded structure in the gap after
    // projection::disable() and immediately enable the projection again.
    detail::StructureSession::getInstance().clearLoaded("已关闭投影");

    // The active projection and in-flight worker keep non-owning Block pointers
    // into the Java mapper registry. Stop them before releasing that registry.
    projection::disable();
    resetJavaBlockMappingCache();
    uiState().clearMaterials();
}

} // namespace lholo::structure
