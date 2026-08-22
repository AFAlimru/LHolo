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

constexpr unsigned int   kHotkeyModifierControl    = 1u;
constexpr unsigned int   kHotkeyModifierAlt        = 2u;
constexpr unsigned int   kHotkeyModifierShift      = 4u;
constexpr char           kMaterialPopupName[]       = "材料清单###LHoloMaterialList";


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

std::filesystem::path settingsPath() {
    return LHolo::getInstance().getSelf().getConfigDir() / "config.json";
}

std::filesystem::path pathFromUtf8(std::string_view value) {
    if (value.empty()) return {};
    auto const wideSize = MultiByteToWideChar(
        CP_UTF8,
        MB_ERR_INVALID_CHARS,
        value.data(),
        static_cast<int>(value.size()),
        nullptr,
        0
    );
    if (wideSize <= 0) return std::filesystem::path{value};

    std::wstring wide(static_cast<std::size_t>(wideSize), L'\0');
    if (MultiByteToWideChar(
            CP_UTF8,
            MB_ERR_INVALID_CHARS,
            value.data(),
            static_cast<int>(value.size()),
            wide.data(),
            wideSize
        ) <= 0) {
        return std::filesystem::path{value};
    }
    return std::filesystem::path{wide};
}

std::string pathToUtf8(std::filesystem::path const& path) {
    auto const& wide = path.native();
    if (wide.empty()) return {};
    auto const size = WideCharToMultiByte(
        CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()), nullptr, 0, nullptr, nullptr
    );
    if (size <= 0) return path.string();
    std::string result(static_cast<std::size_t>(size), '\0');
    WideCharToMultiByte(
        CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()), result.data(), size, nullptr, nullptr
    );
    return result;
}

unsigned int currentHotkeyModifiers() {
    unsigned int modifiers{};
    if (detail::uiControlHeld().load(std::memory_order_acquire)) modifiers |= kHotkeyModifierControl;
    if (detail::uiAltHeld().load(std::memory_order_acquire)) modifiers |= kHotkeyModifierAlt;
    if (detail::uiShiftHeld().load(std::memory_order_acquire)) modifiers |= kHotkeyModifierShift;
    return modifiers;
}

} // namespace

void requestMaterialList() {
    detail::uiMaterialListRequested().store(true, std::memory_order_release);
}

void processPendingMaterialList() {
    if (!detail::uiMaterialListRequested().exchange(false, std::memory_order_acq_rel)) return;

    auto const loaded = getLoaded();
    std::string localeCode;
    if (auto client = ll::service::getClientInstance()) {
        if (auto* player = client->getLocalPlayer()) localeCode = player->getLocaleCode();
    }

    std::vector<MaterialRequirement> materials;
    if (loaded) materials = collectMaterials(loaded->renderBlocks, localeCode);

    std::lock_guard lock(detail::uiMaterialMutex());
    detail::uiMaterialRequirements() = std::move(materials);
}

void requestOpenGui() {
    auto const opening = !detail::uiGuiVisible().load(std::memory_order_acquire);
    detail::uiGuiVisible().store(opening, std::memory_order_release);
    if (opening) {
        detail::uiOpeningInputBlockFrames().store(3, std::memory_order_release);
    } else {
        // Consume the release half of the key/click that closed the menu.
        // Without this, Minecraft receives an unmatched Esc or mouse-up after
        // the ImGui window has already disappeared.
        detail::uiBlockGameInputUntil().store(GetTickCount64() + 180, std::memory_order_release);
    }
}

bool isGuiVisible() { return detail::uiGuiVisible().load(std::memory_order_acquire); }

bool isInputTransitionBlocked() {
    return GetTickCount64() <= detail::uiBlockGameInputUntil().load(std::memory_order_acquire);
}

bool handleGuiHotkeyKeyDown(unsigned int virtualKey) {
    auto const modifierKey = ui::isModifierKey(virtualKey);
    if (virtualKey == VK_CONTROL || virtualKey == VK_LCONTROL || virtualKey == VK_RCONTROL) {
        detail::uiControlHeld().store(true, std::memory_order_release);
    } else if (virtualKey == VK_MENU || virtualKey == VK_LMENU || virtualKey == VK_RMENU) {
        detail::uiAltHeld().store(true, std::memory_order_release);
    } else if (virtualKey == VK_SHIFT || virtualKey == VK_LSHIFT || virtualKey == VK_RSHIFT) {
        detail::uiShiftHeld().store(true, std::memory_order_release);
    }

    std::atomic_uint* captureKey{};
    std::atomic_uint* captureModifiers{};
    if (detail::uiCapturingGuiHotkey().load(std::memory_order_acquire)) {
        captureKey = &detail::uiGuiHotkey();
        captureModifiers = &detail::uiGuiHotkeyModifiers();
    } else if (detail::uiCapturingLayerIncreaseHotkey().load(std::memory_order_acquire)) {
        captureKey = &detail::uiLayerIncreaseHotkey();
        captureModifiers = &detail::uiLayerIncreaseHotkeyModifiers();
    } else if (detail::uiCapturingLayerDecreaseHotkey().load(std::memory_order_acquire)) {
        captureKey = &detail::uiLayerDecreaseHotkey();
        captureModifiers = &detail::uiLayerDecreaseHotkeyModifiers();
    } else {
        for (std::size_t index = 0; index < detail::uiCapturingMoveHotkey().size(); ++index) {
            if (detail::uiCapturingMoveHotkey()[index].load(std::memory_order_acquire)) {
                captureKey = &detail::uiMoveHotkeys()[index];
                captureModifiers = &detail::uiMoveHotkeyModifiers()[index];
                break;
            }
        }
    }
    if (captureKey) {
        // F11 belongs to Minecraft's fullscreen toggle. Never capture or
        // consume it as a mod shortcut, including while rebinding controls.
        if (virtualKey == VK_F11) return false;
        auto stopCapturing = [] {
            detail::uiCapturingGuiHotkey().store(false, std::memory_order_release);
            detail::uiCapturingLayerIncreaseHotkey().store(false, std::memory_order_release);
            detail::uiCapturingLayerDecreaseHotkey().store(false, std::memory_order_release);
            for (auto& capturing : detail::uiCapturingMoveHotkey()) {
                capturing.store(false, std::memory_order_release);
            }
        };
        if (virtualKey == VK_ESCAPE) {
            stopCapturing();
        } else if (virtualKey == VK_DELETE || virtualKey == VK_BACK) {
            captureKey->store(0, std::memory_order_release);
            captureModifiers->store(0, std::memory_order_release);
            stopCapturing();
            detail::uiPendingSettingsSave().store(true, std::memory_order_release);
        } else if (!modifierKey) {
            auto const modifiers = currentHotkeyModifiers();
            auto clearDuplicate = [captureKey, captureModifiers, virtualKey, modifiers](
                                      std::atomic_uint& key,
                                      std::atomic_uint& keyModifiers
                                  ) {
                if (&key == captureKey && &keyModifiers == captureModifiers) return;
                if (key.load(std::memory_order_relaxed) == virtualKey
                    && keyModifiers.load(std::memory_order_relaxed) == modifiers) {
                    key.store(0, std::memory_order_relaxed);
                    keyModifiers.store(0, std::memory_order_relaxed);
                }
            };
            clearDuplicate(detail::uiGuiHotkey(), detail::uiGuiHotkeyModifiers());
            clearDuplicate(detail::uiLayerIncreaseHotkey(), detail::uiLayerIncreaseHotkeyModifiers());
            clearDuplicate(detail::uiLayerDecreaseHotkey(), detail::uiLayerDecreaseHotkeyModifiers());
            for (std::size_t index = 0; index < detail::uiMoveHotkeys().size(); ++index) {
                clearDuplicate(detail::uiMoveHotkeys()[index], detail::uiMoveHotkeyModifiers()[index]);
            }
            captureKey->store(virtualKey, std::memory_order_release);
            captureModifiers->store(modifiers, std::memory_order_release);
            stopCapturing();
            detail::uiIgnoreHotkeyUntil().store(GetTickCount64() + 250, std::memory_order_release);
            detail::uiPendingSettingsSave().store(true, std::memory_order_release);
        }
        return true;
    }

    if (modifierKey) return false;

    auto const modifiers = currentHotkeyModifiers();
    auto const hotkey = detail::uiGuiHotkey().load(std::memory_order_acquire);
    if (hotkey != 0 && virtualKey == hotkey
        && modifiers == detail::uiGuiHotkeyModifiers().load(std::memory_order_acquire)) {
        if (GetTickCount64() >= detail::uiIgnoreHotkeyUntil().load(std::memory_order_acquire)
            && !detail::uiGuiHotkeyHeld().exchange(true, std::memory_order_acq_rel)) {
            requestOpenGui();
        }
        return true;
    }
    if (isGuiVisible()) return false;

    for (std::size_t index = 0; index < detail::uiMoveHotkeys().size(); ++index) {
        if (detail::uiMoveHotkeys()[index].load(std::memory_order_acquire) == virtualKey
            && detail::uiMoveHotkeyModifiers()[index].load(std::memory_order_acquire) == modifiers) {
            if (GetTickCount64() >= detail::uiIgnoreHotkeyUntil().load(std::memory_order_acquire)
                && !detail::uiMoveHotkeyHeld()[index].exchange(true, std::memory_order_acq_rel)) {
                switch (index) {
                case 0: detail::uiPendingOffsetX().fetch_sub(1, std::memory_order_relaxed); break;
                case 1: detail::uiPendingOffsetX().fetch_add(1, std::memory_order_relaxed); break;
                case 2: detail::uiPendingOffsetZ().fetch_sub(1, std::memory_order_relaxed); break;
                case 3: detail::uiPendingOffsetZ().fetch_add(1, std::memory_order_relaxed); break;
                case 4: detail::uiPendingOffsetY().fetch_add(1, std::memory_order_relaxed); break;
                case 5: detail::uiPendingOffsetY().fetch_sub(1, std::memory_order_relaxed); break;
                default: break;
                }
            }
            return true;
        }
    }

    auto const layerIncreaseHotkey = detail::uiLayerIncreaseHotkey().load(std::memory_order_acquire);
    if (layerIncreaseHotkey != 0 && virtualKey == layerIncreaseHotkey
        && modifiers == detail::uiLayerIncreaseHotkeyModifiers().load(std::memory_order_acquire)) {
        if (detail::sessionLayerDisplayMode().load(std::memory_order_acquire) == 0) return false;
        if (GetTickCount64() >= detail::uiIgnoreHotkeyUntil().load(std::memory_order_acquire)
            && !detail::uiLayerIncreaseHotkeyHeld().exchange(true, std::memory_order_acq_rel)) {
            detail::uiPendingLayerDelta().fetch_add(1, std::memory_order_relaxed);
        }
        return true;
    }
    auto const layerDecreaseHotkey = detail::uiLayerDecreaseHotkey().load(std::memory_order_acquire);
    if (layerDecreaseHotkey != 0 && virtualKey == layerDecreaseHotkey
        && modifiers == detail::uiLayerDecreaseHotkeyModifiers().load(std::memory_order_acquire)) {
        if (detail::sessionLayerDisplayMode().load(std::memory_order_acquire) == 0) return false;
        if (GetTickCount64() >= detail::uiIgnoreHotkeyUntil().load(std::memory_order_acquire)
            && !detail::uiLayerDecreaseHotkeyHeld().exchange(true, std::memory_order_acq_rel)) {
            detail::uiPendingLayerDelta().fetch_sub(1, std::memory_order_relaxed);
        }
        return true;
    }
    return false;
}

bool handleGuiHotkeyKeyUp(unsigned int virtualKey) {
    if (virtualKey == VK_CONTROL || virtualKey == VK_LCONTROL || virtualKey == VK_RCONTROL) {
        detail::uiControlHeld().store(false, std::memory_order_release);
        return false;
    }
    if (virtualKey == VK_MENU || virtualKey == VK_LMENU || virtualKey == VK_RMENU) {
        detail::uiAltHeld().store(false, std::memory_order_release);
        return false;
    }
    if (virtualKey == VK_SHIFT || virtualKey == VK_LSHIFT || virtualKey == VK_RSHIFT) {
        detail::uiShiftHeld().store(false, std::memory_order_release);
        return false;
    }

    bool consumed{};
    if (virtualKey == detail::uiGuiHotkey().load(std::memory_order_acquire)) {
        consumed = detail::uiGuiHotkeyHeld().exchange(false, std::memory_order_acq_rel) || consumed;
    }
    if (virtualKey == detail::uiLayerIncreaseHotkey().load(std::memory_order_acquire)) {
        consumed = detail::uiLayerIncreaseHotkeyHeld().exchange(false, std::memory_order_acq_rel) || consumed;
    }
    if (virtualKey == detail::uiLayerDecreaseHotkey().load(std::memory_order_acquire)) {
        consumed = detail::uiLayerDecreaseHotkeyHeld().exchange(false, std::memory_order_acq_rel) || consumed;
    }
    for (std::size_t index = 0; index < detail::uiMoveHotkeys().size(); ++index) {
        if (virtualKey == detail::uiMoveHotkeys()[index].load(std::memory_order_acquire)) {
            consumed = detail::uiMoveHotkeyHeld()[index].exchange(false, std::memory_order_acq_rel) || consumed;
        }
    }
    if (virtualKey < detail::uiConsumeKeyReleaseUntil().size()) {
        auto const now = GetTickCount64();
        auto& deadline = detail::uiConsumeKeyReleaseUntil()[virtualKey];
        if (consumed) {
            deadline.store(now + 100, std::memory_order_release);
            return true;
        }
        if (now <= deadline.load(std::memory_order_acquire)) return true;
    }
    return false;
}

void resetHotkeyState() {
    detail::uiControlHeld().store(false, std::memory_order_release);
    detail::uiAltHeld().store(false, std::memory_order_release);
    detail::uiShiftHeld().store(false, std::memory_order_release);
    detail::uiGuiHotkeyHeld().store(false, std::memory_order_release);
    detail::uiLayerIncreaseHotkeyHeld().store(false, std::memory_order_release);
    detail::uiLayerDecreaseHotkeyHeld().store(false, std::memory_order_release);
    for (auto& held : detail::uiMoveHotkeyHeld()) held.store(false, std::memory_order_release);
    for (auto& deadline : detail::uiConsumeKeyReleaseUntil()) deadline.store(0, std::memory_order_release);
}

void processPendingHotkeyActions() {
    auto const offsetX = detail::uiPendingOffsetX().exchange(0, std::memory_order_acq_rel);
    auto const offsetY = detail::uiPendingOffsetY().exchange(0, std::memory_order_acq_rel);
    auto const offsetZ = detail::uiPendingOffsetZ().exchange(0, std::memory_order_acq_rel);
    auto const layerDelta = detail::uiPendingLayerDelta().exchange(0, std::memory_order_acq_rel);
    auto const layerActionEnabled = layerDelta != 0
        && detail::sessionLayerDisplayMode().load(std::memory_order_relaxed) != 0;
    bool changed = offsetX != 0 || offsetY != 0 || offsetZ != 0 || layerActionEnabled;

    auto applyOffset = [](std::atomic_int& target, int delta) {
        if (delta == 0) return;
        auto const current = static_cast<long long>(target.load(std::memory_order_relaxed));
        auto const next = std::clamp(
            current + static_cast<long long>(delta),
            static_cast<long long>(std::numeric_limits<int>::min()),
            static_cast<long long>(std::numeric_limits<int>::max())
        );
        target.store(static_cast<int>(next), std::memory_order_relaxed);
    };
    applyOffset(detail::sessionOffsetX(), offsetX);
    applyOffset(detail::sessionOffsetY(), offsetY);
    applyOffset(detail::sessionOffsetZ(), offsetZ);

    if (layerActionEnabled) {
        auto maxLayer = 0;
        auto const layerAxis = detail::sessionLayerAxis().load(std::memory_order_relaxed);
        {
            std::lock_guard lock(detail::sessionLoadedMutex());
            if (detail::sessionLoaded()) maxLayer = detail::maxLayerFor(*detail::sessionLoaded(), layerAxis);
        }
        auto const current = static_cast<long long>(detail::sessionDisplayLayer().load(std::memory_order_relaxed));
        auto const next = std::clamp(current + static_cast<long long>(layerDelta), 0LL, static_cast<long long>(maxLayer));
        detail::sessionDisplayLayer().store(static_cast<int>(next), std::memory_order_relaxed);
    }

    changed = detail::uiPendingSettingsSave().exchange(false, std::memory_order_acq_rel) || changed;
    if (changed) saveSettings();
}

bool hasHudInfo() {
    if (!detail::uiHudEnabled().load(std::memory_order_relaxed)) return false;
    if (!detail::uiHudShowFileName().load(std::memory_order_relaxed)
        && !detail::uiHudShowLayer().load(std::memory_order_relaxed)
        && !detail::uiHudShowOverallProgress().load(std::memory_order_relaxed)
        && !detail::uiHudShowProgress().load(std::memory_order_relaxed)
        && !detail::uiHudShowWrongState().load(std::memory_order_relaxed)
        && !detail::uiHudShowWrongType().load(std::memory_order_relaxed)
        && !detail::uiHudShowBlockEntity().load(std::memory_order_relaxed)) return false;
    std::lock_guard lock(detail::sessionLoadedMutex());
    return static_cast<bool>(detail::sessionLoaded());
}

void renderHud() {
    if (isGuiVisible()) return;
    if (!detail::uiHudEnabled().load(std::memory_order_relaxed)) return;
    auto const showFileName = detail::uiHudShowFileName().load(std::memory_order_relaxed);
    auto const showLayer = detail::uiHudShowLayer().load(std::memory_order_relaxed);
    auto const showOverallProgress = detail::uiHudShowOverallProgress().load(std::memory_order_relaxed);
    auto const showProgress = detail::uiHudShowProgress().load(std::memory_order_relaxed);
    auto const showWrongState = detail::uiHudShowWrongState().load(std::memory_order_relaxed);
    auto const showWrongType = detail::uiHudShowWrongType().load(std::memory_order_relaxed);
    auto const showBlockEntity = detail::uiHudShowBlockEntity().load(std::memory_order_relaxed);
    if (!showFileName && !showLayer && !showOverallProgress && !showProgress
        && !showWrongState && !showWrongType && !showBlockEntity) return;

    std::string fileName;
    int maxLayer{};
    auto const layerAxis = detail::sessionLayerAxis().load(std::memory_order_relaxed);
    {
        std::lock_guard lock(detail::sessionLoadedMutex());
        if (!detail::sessionLoaded()) return;
        fileName = pathToUtf8(detail::sessionLoaded()->sourcePath.filename());
        maxLayer = detail::maxLayerFor(*detail::sessionLoaded(), layerAxis);
    }

    auto const displaySize = ImGui::GetIO().DisplaySize;
    auto uiScale = detail::uiUiScale().load(std::memory_order_relaxed);
    if (uiScale <= 0.0f) {
        uiScale = std::clamp(
            std::min(displaySize.x / 1920.0f, displaySize.y / 1080.0f),
            1.0f,
            5.0f
        );
    }
    auto const hudMetrics = lholo::ui::calculateMetrics(displaySize, uiScale);
    lholo::ui::applyFluentTheme(hudMetrics);
    auto const layerMode = detail::sessionLayerDisplayMode().load(std::memory_order_relaxed);
    auto const currentLayer = std::clamp(
        detail::sessionDisplayLayer().load(std::memory_order_relaxed),
        0,
        maxLayer
    );

    auto const hudPosition = std::clamp(detail::uiHudPosition().load(std::memory_order_relaxed), 0, 3);
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
        auto const aimedBlockEntity = place::getAimedBlockEntityName();
        if (showBlockEntity && !aimedBlockEntity.empty()) {
            ImGui::TextColored(
                ImVec4(0.55f, 0.85f, 1.0f, 1.0f),
                "方块实体：%s",
                aimedBlockEntity.c_str()
            );
        }
    }
    ImGui::End();
    ImGui::PopStyleVar(2);
}

namespace {


void stopHotkeyCapture() {
    detail::uiCapturingGuiHotkey().store(false, std::memory_order_release);
    detail::uiCapturingLayerIncreaseHotkey().store(false, std::memory_order_release);
    detail::uiCapturingLayerDecreaseHotkey().store(false, std::memory_order_release);
    for (auto& capturing : detail::uiCapturingMoveHotkey()) capturing.store(false, std::memory_order_release);
}



lholo::ui::MenuActions makeMenuActions(bool& refreshModel) {
    lholo::ui::MenuActions actions;
    actions.browseStructure = [](std::string_view current) -> std::optional<std::string> {
        auto const selected = lholo::ui::openStructureFile(pathFromUtf8(current));
        return selected ? std::optional<std::string>{pathToUtf8(*selected)} : std::nullopt;
    };
    actions.loadStructure = [&refreshModel](std::string_view pathValue) {
        auto const pathText = std::string{pathValue};
        if (pathText.empty()) {
            std::lock_guard lock(detail::sessionLoadedMutex());
            detail::sessionStatus() = "请选择或输入 .mcstructure / .litematic 文件路径";
            return;
        }
        std::string error;
        auto loaded = detail::loadStructureFile(pathFromUtf8(pathText), error);
        if (!loaded) {
            std::lock_guard lock(detail::sessionLoadedMutex());
            detail::sessionStatus() = "加载失败: " + error;
            logger().error("Could not load structure {}: {}", pathText, error);
            return;
        }
        std::string status;
        {
            std::lock_guard lock(detail::sessionLoadedMutex());
            detail::sessionLastPath() = pathText;
            detail::sessionStatus() = detail::makeStructureStatus(*loaded);
            status = detail::sessionStatus();
            detail::sessionLoaded() = std::move(loaded);
        }
        saveSettings();
        refreshModel = true;
        logger().info("{}", status);
    };
    actions.restoreProjection = [&refreshModel] {
        std::string savedPath;
        {
            std::lock_guard lock(detail::sessionLoadedMutex());
            savedPath = detail::sessionSavedStructurePath();
        }
        auto const x = detail::sessionSavedAnchorX().load(std::memory_order_relaxed);
        auto const y = detail::sessionSavedAnchorY().load(std::memory_order_relaxed);
        auto const z = detail::sessionSavedAnchorZ().load(std::memory_order_relaxed);
        std::string error;
        auto loaded = detail::loadStructureFile(pathFromUtf8(savedPath), error);
        if (!loaded) {
            std::lock_guard lock(detail::sessionLoadedMutex());
            detail::sessionStatus() = "恢复失败: " + error;
            logger().error("Could not restore structure {}: {}", savedPath, error);
            return;
        }
        detail::sessionRotationQuarterTurns().store(detail::sessionSavedRotation().load(std::memory_order_relaxed), std::memory_order_relaxed);
        detail::sessionMirror().store(
            std::clamp(detail::sessionSavedMirror().load(std::memory_order_relaxed), 0, 2),
            std::memory_order_relaxed
        );
        detail::sessionOffsetX().store(detail::sessionSavedOffsetX().load(std::memory_order_relaxed), std::memory_order_relaxed);
        detail::sessionOffsetY().store(detail::sessionSavedOffsetY().load(std::memory_order_relaxed), std::memory_order_relaxed);
        detail::sessionOffsetZ().store(detail::sessionSavedOffsetZ().load(std::memory_order_relaxed), std::memory_order_relaxed);
        detail::sessionLayerDisplayMode().store(detail::sessionSavedLayerDisplayMode().load(std::memory_order_relaxed), std::memory_order_relaxed);
        detail::sessionDisplayLayer().store(detail::sessionSavedDisplayLayer().load(std::memory_order_relaxed), std::memory_order_relaxed);
        detail::sessionLayerAxis().store(detail::sessionSavedLayerAxis().load(std::memory_order_relaxed), std::memory_order_relaxed);
        projection::requestNextStructureAnchor(x, y, z);
        {
            std::lock_guard lock(detail::sessionLoadedMutex());
            detail::sessionLastPath() = savedPath;
            detail::sessionStatus() = "已恢复上次投影记录，等待进入渲染";
            detail::sessionLoaded() = std::move(loaded);
        }
        std::snprintf(detail::uiPathBuffer().data(), detail::uiPathBuffer().size(), "%s", savedPath.c_str());
        refreshModel = true;
        logger().info("Restoring projection {} at ({}, {}, {})", savedPath, x, y, z);
    };
    actions.closeProjection = [&refreshModel] {
        clear();
        refreshModel = true;
    };
    actions.requestMaterials = [] { requestMaterialList(); };
    actions.beginHotkeyCapture = [](lholo::ui::HotkeyId id) {
        stopHotkeyCapture();
        if (auto const binding = lholo::ui::hotkeyBinding(id); binding.capturing) binding.capturing->store(true, std::memory_order_release);
    };
    actions.clearHotkey = [](lholo::ui::HotkeyId id) {
        if (auto const binding = lholo::ui::hotkeyBinding(id); binding.key && binding.modifiers) {
            binding.key->store(0, std::memory_order_release);
            binding.modifiers->store(0, std::memory_order_release);
            if (binding.capturing) binding.capturing->store(false, std::memory_order_release);
            saveSettings();
        }
    };
    actions.resetHotkeys = [] {
        detail::uiGuiHotkey().store('M', std::memory_order_relaxed);
        detail::uiGuiHotkeyModifiers().store(kHotkeyModifierAlt, std::memory_order_relaxed);
        static unsigned int const moveKeys[]{VK_LEFT, VK_RIGHT, VK_UP, VK_DOWN, VK_UP, VK_DOWN};
        static unsigned int const moveModifiers[]{kHotkeyModifierControl, kHotkeyModifierControl, kHotkeyModifierControl, kHotkeyModifierControl, kHotkeyModifierShift, kHotkeyModifierShift};
        for (std::size_t index = 0; index < detail::uiMoveHotkeys().size(); ++index) {
            detail::uiMoveHotkeys()[index].store(moveKeys[index], std::memory_order_relaxed);
            detail::uiMoveHotkeyModifiers()[index].store(moveModifiers[index], std::memory_order_relaxed);
        }
        detail::uiLayerIncreaseHotkey().store(VK_UP, std::memory_order_relaxed);
        detail::uiLayerDecreaseHotkey().store(VK_DOWN, std::memory_order_relaxed);
        detail::uiLayerIncreaseHotkeyModifiers().store(kHotkeyModifierAlt, std::memory_order_relaxed);
        detail::uiLayerDecreaseHotkeyModifiers().store(kHotkeyModifierAlt, std::memory_order_relaxed);
        stopHotkeyCapture();
        resetHotkeyState();
        saveSettings();
    };
    actions.resetCorrectionStyle = [] {
        projection::setCorrectionFillOpacity(0.15f);
        projection::setCorrectionOutlineOpacity(1.0f);
        saveSettings();
    };
    actions.usePlayerCapturePosition = [&refreshModel](lholo::ui::CapturePointId point) {
        capture::setPointFromPlayer(
            point == lholo::ui::CapturePointId::First
                ? capture::PointSlot::First
                : capture::PointSlot::Second
        );
        refreshModel = true;
    };
    actions.clearCapture = [&refreshModel] {
        capture::clear();
        refreshModel = true;
    };
    actions.exportCapture = [&refreshModel](lholo::ui::CaptureDraftModel const& model) {
        auto const output = lholo::ui::saveMcstructureFile();
        if (!output) return;
        capture::Draft draft;
        draft.mode = static_cast<capture::CaptureMode>(std::clamp(model.mode, 0, 1));
        draft.includeEntities = model.includeEntities;
        if (model.first.set) draft.first = capture::Point{model.first.x, model.first.y, model.first.z};
        if (model.second.set) draft.second = capture::Point{model.second.x, model.second.y, model.second.z};
        capture::exportStructure(draft, *output);
        refreshModel = true;
    };
    return actions;
}

} // namespace

void renderGui() {
    if (!isGuiVisible()) return;
    auto const displaySize = ImGui::GetIO().DisplaySize;
    auto const configuredScale = detail::uiUiScale().load(std::memory_order_relaxed);
    auto const effectiveScale = configuredScale > 0.0f
        ? std::clamp(configuredScale, 1.0f, 5.0f)
        : std::clamp(std::min(displaySize.x / 1920.0f, displaySize.y / 1080.0f), 1.0f, 5.0f);
    if (!detail::uiPathInitialized()) {
        std::lock_guard lock(detail::sessionLoadedMutex());
        std::snprintf(detail::uiPathBuffer().data(), detail::uiPathBuffer().size(), "%s", detail::sessionLastPath().c_str());
        detail::uiPathInitialized() = true;
    }
    auto const metrics = lholo::ui::calculateMetrics(displaySize, effectiveScale);
    lholo::ui::applyFluentTheme(metrics);
    auto model = lholo::ui::buildStructureMenuModel(effectiveScale);
    bool refreshModel = false;
    auto const actions = makeMenuActions(refreshModel);
    lholo::ui::renderMenu(model, actions, metrics);
    detail::uiActivePage() = model.page;
    if (!refreshModel) lholo::ui::applyStructureMenuModel(model, effectiveScale);
    if (detail::uiOpeningInputBlockFrames().load(std::memory_order_acquire) > 0) {
        detail::uiOpeningInputBlockFrames().fetch_sub(1, std::memory_order_acq_rel);
    }
    if (model.closeRequested) {
        detail::uiGuiVisible().store(false, std::memory_order_release);
        detail::uiBlockGameInputUntil().store(GetTickCount64() + 180, std::memory_order_release);
    }
}

void loadSettings() {
    auto const path = settingsPath();
    try {
        lholo::settings::Settings settings;
        if (!lholo::settings::loadSettingsFile(path, settings)) {
            saveSettings();
            return;
        }
        std::lock_guard lock(detail::sessionLoadedMutex());
        detail::sessionLastPath() = settings.lastStructurePath;
        detail::uiUiScale().store(std::clamp(settings.uiScale, 0.0f, 5.0f), std::memory_order_relaxed);
        projection::setOpacity(settings.opacity);
        projection::setCorrectionFillOpacity(settings.correctionFillOpacity);
        projection::setCorrectionOutlineOpacity(settings.correctionOutlineOpacity);
        projection::setStructureBoundsEnabled(settings.structureBoundsEnabled);
        // Transform and layer state are session-local. Only the explicit
        // "restore last projection" record below is persisted.
        detail::sessionRotationQuarterTurns().store(0, std::memory_order_relaxed);
        detail::sessionMirror().store(0, std::memory_order_relaxed);
        detail::sessionOffsetX().store(0, std::memory_order_relaxed);
        detail::sessionOffsetY().store(0, std::memory_order_relaxed);
        detail::sessionOffsetZ().store(0, std::memory_order_relaxed);
        detail::sessionLayerDisplayMode().store(0, std::memory_order_relaxed);
        detail::sessionDisplayLayer().store(0, std::memory_order_relaxed);
        detail::sessionLayerAxis().store(0, std::memory_order_relaxed);
        detail::uiHudEnabled().store(settings.hudEnabled, std::memory_order_relaxed);
        detail::uiHudShowFileName().store(settings.hudShowFileName, std::memory_order_relaxed);
        detail::uiHudShowLayer().store(settings.hudShowLayer, std::memory_order_relaxed);
        detail::uiHudShowOverallProgress().store(settings.hudShowOverallProgress, std::memory_order_relaxed);
        detail::uiHudShowProgress().store(settings.hudShowProgress, std::memory_order_relaxed);
        detail::uiHudShowWrongState().store(settings.hudShowWrongState, std::memory_order_relaxed);
        detail::uiHudShowWrongType().store(settings.hudShowWrongType, std::memory_order_relaxed);
        detail::uiHudShowBlockEntity().store(settings.hudShowBlockEntity, std::memory_order_relaxed);
        detail::uiHudPosition().store(std::clamp(settings.hudPosition, 0, 3), std::memory_order_relaxed);
        // Assisted-placement modes are intentionally session-only. Ignore
        // legacy persisted values and always begin a new game session disabled.
        place::setEnabled(false);
        place::setManualMode(false);
        place::setRangeEnabled(false);
        place::setPlacementRadius(std::clamp(settings.placementRadius, 1, 4));
        detail::uiGuiHotkey().store(std::clamp(settings.guiHotkey, 0, 255), std::memory_order_relaxed);
        detail::uiGuiHotkeyModifiers().store(
            std::clamp(settings.guiHotkeyModifiers, 0, 7), std::memory_order_relaxed
        );
        detail::uiLayerIncreaseHotkey().store(
            std::clamp(settings.layerIncreaseHotkey, 0, 255), std::memory_order_relaxed
        );
        detail::uiLayerDecreaseHotkey().store(
            std::clamp(settings.layerDecreaseHotkey, 0, 255), std::memory_order_relaxed
        );
        detail::uiLayerIncreaseHotkeyModifiers().store(
            std::clamp(settings.layerIncreaseHotkeyModifiers, 0, 7), std::memory_order_relaxed
        );
        detail::uiLayerDecreaseHotkeyModifiers().store(
            std::clamp(settings.layerDecreaseHotkeyModifiers, 0, 7), std::memory_order_relaxed
        );
        for (std::size_t index = 0; index < detail::uiMoveHotkeys().size(); ++index) {
            detail::uiMoveHotkeys()[index].store(
                std::clamp(settings.moveHotkeys[index], 0, 255), std::memory_order_relaxed
            );
            detail::uiMoveHotkeyModifiers()[index].store(
                std::clamp(settings.moveHotkeyModifiers[index], 0, 7), std::memory_order_relaxed
            );
        }
        detail::sessionHasSavedProjection().store(settings.hasSavedProjection, std::memory_order_relaxed);
        detail::sessionSavedAnchorX().store(settings.savedAnchorX, std::memory_order_relaxed);
        detail::sessionSavedAnchorY().store(settings.savedAnchorY, std::memory_order_relaxed);
        detail::sessionSavedAnchorZ().store(settings.savedAnchorZ, std::memory_order_relaxed);
        detail::sessionSavedRotation().store(settings.savedRotation, std::memory_order_relaxed);
        detail::sessionSavedMirror().store(std::clamp(settings.savedMirror, 0, 2), std::memory_order_relaxed);
        detail::sessionSavedOffsetX().store(settings.savedOffsetX, std::memory_order_relaxed);
        detail::sessionSavedOffsetY().store(settings.savedOffsetY, std::memory_order_relaxed);
        detail::sessionSavedOffsetZ().store(settings.savedOffsetZ, std::memory_order_relaxed);
        detail::sessionSavedLayerDisplayMode().store(settings.savedLayerDisplayMode, std::memory_order_relaxed);
        detail::sessionSavedDisplayLayer().store(settings.savedDisplayLayer, std::memory_order_relaxed);
        detail::sessionSavedLayerAxis().store(std::clamp(settings.savedLayerAxis, 0, 1), std::memory_order_relaxed);
        detail::sessionSavedStructurePath() = settings.savedStructurePath;
        logger().info("Loaded projection settings from {}", path.string());
    } catch (std::exception const& exception) {
        logger().error("Could not load projection settings {}: {}", path.string(), exception.what());
    }
}

void saveSettings() {
    auto const path = settingsPath();
    try {
        bool hasActiveProjection = false;
        {
            std::lock_guard lock(detail::sessionLoadedMutex());
            hasActiveProjection = static_cast<bool>(detail::sessionLoaded());
        }
        if (hasActiveProjection && detail::sessionHasSavedProjection().load(std::memory_order_acquire)) {
            // Only an active projection may update its restore snapshot. At
            // startup the session-local transform/layer values intentionally
            // reset to defaults; copying those values before the user restores
            // a structure would silently destroy the saved state.
            detail::sessionSavedRotation().store(detail::sessionRotationQuarterTurns().load(std::memory_order_relaxed), std::memory_order_relaxed);
            detail::sessionSavedMirror().store(detail::sessionMirror().load(std::memory_order_relaxed), std::memory_order_relaxed);
            detail::sessionSavedOffsetX().store(detail::sessionOffsetX().load(std::memory_order_relaxed), std::memory_order_relaxed);
            detail::sessionSavedOffsetY().store(detail::sessionOffsetY().load(std::memory_order_relaxed), std::memory_order_relaxed);
            detail::sessionSavedOffsetZ().store(detail::sessionOffsetZ().load(std::memory_order_relaxed), std::memory_order_relaxed);
            detail::sessionSavedLayerDisplayMode().store(detail::sessionLayerDisplayMode().load(std::memory_order_relaxed), std::memory_order_relaxed);
            detail::sessionSavedDisplayLayer().store(detail::sessionDisplayLayer().load(std::memory_order_relaxed), std::memory_order_relaxed);
            detail::sessionSavedLayerAxis().store(detail::sessionLayerAxis().load(std::memory_order_relaxed), std::memory_order_relaxed);
        }
        std::string lastPath;
        std::string savedStructurePath;
        {
            std::lock_guard lock(detail::sessionLoadedMutex());
            lastPath = detail::sessionLastPath();
            savedStructurePath = detail::sessionSavedStructurePath();
        }
        lholo::settings::Settings settings;
        settings.lastStructurePath = lastPath;
        settings.uiScale = detail::uiUiScale().load(std::memory_order_relaxed);
        settings.opacity = projection::getOpacity();
        settings.correctionFillOpacity = projection::getCorrectionFillOpacity();
        settings.correctionOutlineOpacity = projection::getCorrectionOutlineOpacity();
        settings.structureBoundsEnabled = projection::getStructureBoundsEnabled();
        settings.placementRadius = place::getPlacementRadius();
        settings.hudEnabled = detail::uiHudEnabled().load(std::memory_order_relaxed);
        settings.hudShowFileName = detail::uiHudShowFileName().load(std::memory_order_relaxed);
        settings.hudShowLayer = detail::uiHudShowLayer().load(std::memory_order_relaxed);
        settings.hudShowOverallProgress = detail::uiHudShowOverallProgress().load(std::memory_order_relaxed);
        settings.hudShowProgress = detail::uiHudShowProgress().load(std::memory_order_relaxed);
        settings.hudShowWrongState = detail::uiHudShowWrongState().load(std::memory_order_relaxed);
        settings.hudShowWrongType = detail::uiHudShowWrongType().load(std::memory_order_relaxed);
        settings.hudShowBlockEntity = detail::uiHudShowBlockEntity().load(std::memory_order_relaxed);
        settings.hudPosition = detail::uiHudPosition().load(std::memory_order_relaxed);
        settings.guiHotkey = detail::uiGuiHotkey().load(std::memory_order_relaxed);
        settings.guiHotkeyModifiers = detail::uiGuiHotkeyModifiers().load(std::memory_order_relaxed);
        settings.layerIncreaseHotkey = detail::uiLayerIncreaseHotkey().load(std::memory_order_relaxed);
        settings.layerDecreaseHotkey = detail::uiLayerDecreaseHotkey().load(std::memory_order_relaxed);
        settings.layerIncreaseHotkeyModifiers
            = detail::uiLayerIncreaseHotkeyModifiers().load(std::memory_order_relaxed);
        settings.layerDecreaseHotkeyModifiers
            = detail::uiLayerDecreaseHotkeyModifiers().load(std::memory_order_relaxed);
        for (std::size_t index = 0; index < settings.moveHotkeys.size(); ++index) {
            settings.moveHotkeys[index] = detail::uiMoveHotkeys()[index].load(std::memory_order_relaxed);
            settings.moveHotkeyModifiers[index]
                = detail::uiMoveHotkeyModifiers()[index].load(std::memory_order_relaxed);
        }
        settings.hasSavedProjection = detail::sessionHasSavedProjection().load(std::memory_order_relaxed);
        settings.savedAnchorX = detail::sessionSavedAnchorX().load(std::memory_order_relaxed);
        settings.savedAnchorY = detail::sessionSavedAnchorY().load(std::memory_order_relaxed);
        settings.savedAnchorZ = detail::sessionSavedAnchorZ().load(std::memory_order_relaxed);
        settings.savedRotation = detail::sessionSavedRotation().load(std::memory_order_relaxed);
        settings.savedMirror = detail::sessionSavedMirror().load(std::memory_order_relaxed);
        settings.savedOffsetX = detail::sessionSavedOffsetX().load(std::memory_order_relaxed);
        settings.savedOffsetY = detail::sessionSavedOffsetY().load(std::memory_order_relaxed);
        settings.savedOffsetZ = detail::sessionSavedOffsetZ().load(std::memory_order_relaxed);
        settings.savedLayerDisplayMode = detail::sessionSavedLayerDisplayMode().load(std::memory_order_relaxed);
        settings.savedDisplayLayer = detail::sessionSavedDisplayLayer().load(std::memory_order_relaxed);
        settings.savedLayerAxis = detail::sessionSavedLayerAxis().load(std::memory_order_relaxed);
        settings.savedStructurePath = savedStructurePath;
        lholo::settings::saveSettingsFile(path, settings);
    } catch (std::exception const& exception) {
        logger().error("Could not save projection settings {}: {}", path.string(), exception.what());
    }
}

std::shared_ptr<LoadedStructure const> getLoaded() {
    std::lock_guard lock(detail::sessionLoadedMutex());
    return detail::sessionLoaded();
}

int getRotationQuarterTurns() {
    return detail::sessionRotationQuarterTurns().load(std::memory_order_relaxed);
}

int getMirrorMode() {
    return std::clamp(detail::sessionMirror().load(std::memory_order_relaxed), 0, 2);
}

int getOffsetX() { return detail::sessionOffsetX().load(std::memory_order_relaxed); }
int getOffsetY() { return detail::sessionOffsetY().load(std::memory_order_relaxed); }
int getOffsetZ() { return detail::sessionOffsetZ().load(std::memory_order_relaxed); }
int getLayerDisplayMode() { return detail::sessionLayerDisplayMode().load(std::memory_order_relaxed); }
int getDisplayLayer() { return detail::sessionDisplayLayer().load(std::memory_order_relaxed); }
int getLayerAxis() { return detail::sessionLayerAxis().load(std::memory_order_relaxed); }

void recordProjectionAnchor(int x, int y, int z) {
    detail::sessionSavedAnchorX().store(x, std::memory_order_relaxed);
    detail::sessionSavedAnchorY().store(y, std::memory_order_relaxed);
    detail::sessionSavedAnchorZ().store(z, std::memory_order_relaxed);
    detail::sessionSavedRotation().store(detail::sessionRotationQuarterTurns().load(std::memory_order_relaxed), std::memory_order_relaxed);
    detail::sessionSavedMirror().store(detail::sessionMirror().load(std::memory_order_relaxed), std::memory_order_relaxed);
    detail::sessionSavedOffsetX().store(detail::sessionOffsetX().load(std::memory_order_relaxed), std::memory_order_relaxed);
    detail::sessionSavedOffsetY().store(detail::sessionOffsetY().load(std::memory_order_relaxed), std::memory_order_relaxed);
    detail::sessionSavedOffsetZ().store(detail::sessionOffsetZ().load(std::memory_order_relaxed), std::memory_order_relaxed);
    detail::sessionSavedLayerDisplayMode().store(detail::sessionLayerDisplayMode().load(std::memory_order_relaxed), std::memory_order_relaxed);
    detail::sessionSavedDisplayLayer().store(detail::sessionDisplayLayer().load(std::memory_order_relaxed), std::memory_order_relaxed);
    detail::sessionSavedLayerAxis().store(detail::sessionLayerAxis().load(std::memory_order_relaxed), std::memory_order_relaxed);
    {
        std::lock_guard lock(detail::sessionLoadedMutex());
        detail::sessionSavedStructurePath() = detail::sessionLastPath();
    }
    detail::sessionHasSavedProjection().store(true, std::memory_order_release);
    saveSettings();
}

void clear() {
    // Withdraw the requested structure before waiting for the mesh worker.
    // Otherwise the render hook can observe the old detail::sessionLoaded() in the gap after
    // projection::disable() and immediately enable the projection again.
    {
        std::lock_guard lock(detail::sessionLoadedMutex());
        detail::sessionLoaded().reset();
        detail::sessionStatus() = "已关闭投影";
    }

    // The active projection and in-flight worker keep non-owning Block pointers
    // into the Java mapper registry. Stop them before releasing that registry.
    projection::disable();
    resetJavaBlockMappingCache();
    detail::uiMaterialListRequested().store(false, std::memory_order_release);
    std::lock_guard materialLock(detail::uiMaterialMutex());
    detail::uiMaterialRequirements().clear();
}

} // namespace lholo::structure
