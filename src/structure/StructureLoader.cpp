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
#include "structure/StructureUiState.h"
#include "ui/HotkeyFormat.h"
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

std::mutex                       gLoadedMutex;
std::shared_ptr<LoadedStructure> gLoaded;
std::atomic_int                  gRotationQuarterTurns{0};
std::atomic_int                  gMirrorMode{0};
std::atomic_int                  gOffsetX{0};
std::atomic_int                  gOffsetY{0};
std::atomic_int                  gOffsetZ{0};
std::atomic_int                  gLayerDisplayMode{0};
std::atomic_int                  gDisplayLayer{0};
std::atomic_int                  gLayerAxis{0};
std::atomic_bool                 gHasSavedProjection{false};
std::atomic_int                  gSavedAnchorX{0};
std::atomic_int                  gSavedAnchorY{0};
std::atomic_int                  gSavedAnchorZ{0};
std::atomic_int                  gSavedRotation{0};
std::atomic_int                  gSavedMirror{0};
std::atomic_int                  gSavedOffsetX{0};
std::atomic_int                  gSavedOffsetY{0};
std::atomic_int                  gSavedOffsetZ{0};
std::atomic_int                  gSavedLayerDisplayMode{0};
std::atomic_int                  gSavedDisplayLayer{0};
std::atomic_int                  gSavedLayerAxis{0};
std::string                      gSavedStructurePath;
std::string                      gLastPath;
std::string                      gStatus = "尚未加载结构文件";
lholo::ui::MenuPage             gActivePage{lholo::ui::MenuPage::Projection};

int maxLayerFor(LoadedStructure const& structure, int axis) {
    return std::max(0, (axis == 1 ? structure.sizeX : structure.sizeY) - 1);
}

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
        if (gLayerDisplayMode.load(std::memory_order_acquire) == 0) return false;
        if (GetTickCount64() >= detail::uiIgnoreHotkeyUntil().load(std::memory_order_acquire)
            && !detail::uiLayerIncreaseHotkeyHeld().exchange(true, std::memory_order_acq_rel)) {
            detail::uiPendingLayerDelta().fetch_add(1, std::memory_order_relaxed);
        }
        return true;
    }
    auto const layerDecreaseHotkey = detail::uiLayerDecreaseHotkey().load(std::memory_order_acquire);
    if (layerDecreaseHotkey != 0 && virtualKey == layerDecreaseHotkey
        && modifiers == detail::uiLayerDecreaseHotkeyModifiers().load(std::memory_order_acquire)) {
        if (gLayerDisplayMode.load(std::memory_order_acquire) == 0) return false;
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
        && gLayerDisplayMode.load(std::memory_order_relaxed) != 0;
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
    applyOffset(gOffsetX, offsetX);
    applyOffset(gOffsetY, offsetY);
    applyOffset(gOffsetZ, offsetZ);

    if (layerActionEnabled) {
        auto maxLayer = 0;
        auto const layerAxis = gLayerAxis.load(std::memory_order_relaxed);
        {
            std::lock_guard lock(gLoadedMutex);
            if (gLoaded) maxLayer = maxLayerFor(*gLoaded, layerAxis);
        }
        auto const current = static_cast<long long>(gDisplayLayer.load(std::memory_order_relaxed));
        auto const next = std::clamp(current + static_cast<long long>(layerDelta), 0LL, static_cast<long long>(maxLayer));
        gDisplayLayer.store(static_cast<int>(next), std::memory_order_relaxed);
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
    std::lock_guard lock(gLoadedMutex);
    return static_cast<bool>(gLoaded);
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
    auto const layerAxis = gLayerAxis.load(std::memory_order_relaxed);
    {
        std::lock_guard lock(gLoadedMutex);
        if (!gLoaded) return;
        fileName = pathToUtf8(gLoaded->sourcePath.filename());
        maxLayer = maxLayerFor(*gLoaded, layerAxis);
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
    auto const layerMode = gLayerDisplayMode.load(std::memory_order_relaxed);
    auto const currentLayer = std::clamp(
        gDisplayLayer.load(std::memory_order_relaxed),
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

struct UiHotkeyBinding {
    std::atomic_uint* key{};
    std::atomic_uint* modifiers{};
    std::atomic_bool* capturing{};
};

UiHotkeyBinding hotkeyBinding(lholo::ui::HotkeyId id) {
    switch (id) {
    case lholo::ui::HotkeyId::Gui: return {&detail::uiGuiHotkey(), &detail::uiGuiHotkeyModifiers(), &detail::uiCapturingGuiHotkey()};
    case lholo::ui::HotkeyId::MoveXMinus: return {&detail::uiMoveHotkeys()[0], &detail::uiMoveHotkeyModifiers()[0], &detail::uiCapturingMoveHotkey()[0]};
    case lholo::ui::HotkeyId::MoveXPlus: return {&detail::uiMoveHotkeys()[1], &detail::uiMoveHotkeyModifiers()[1], &detail::uiCapturingMoveHotkey()[1]};
    case lholo::ui::HotkeyId::MoveZMinus: return {&detail::uiMoveHotkeys()[2], &detail::uiMoveHotkeyModifiers()[2], &detail::uiCapturingMoveHotkey()[2]};
    case lholo::ui::HotkeyId::MoveZPlus: return {&detail::uiMoveHotkeys()[3], &detail::uiMoveHotkeyModifiers()[3], &detail::uiCapturingMoveHotkey()[3]};
    case lholo::ui::HotkeyId::MoveYPlus: return {&detail::uiMoveHotkeys()[4], &detail::uiMoveHotkeyModifiers()[4], &detail::uiCapturingMoveHotkey()[4]};
    case lholo::ui::HotkeyId::MoveYMinus: return {&detail::uiMoveHotkeys()[5], &detail::uiMoveHotkeyModifiers()[5], &detail::uiCapturingMoveHotkey()[5]};
    case lholo::ui::HotkeyId::LayerIncrease: return {&detail::uiLayerIncreaseHotkey(), &detail::uiLayerIncreaseHotkeyModifiers(), &detail::uiCapturingLayerIncreaseHotkey()};
    case lholo::ui::HotkeyId::LayerDecrease: return {&detail::uiLayerDecreaseHotkey(), &detail::uiLayerDecreaseHotkeyModifiers(), &detail::uiCapturingLayerDecreaseHotkey()};
    }
    return {};
}

void stopHotkeyCapture() {
    detail::uiCapturingGuiHotkey().store(false, std::memory_order_release);
    detail::uiCapturingLayerIncreaseHotkey().store(false, std::memory_order_release);
    detail::uiCapturingLayerDecreaseHotkey().store(false, std::memory_order_release);
    for (auto& capturing : detail::uiCapturingMoveHotkey()) capturing.store(false, std::memory_order_release);
}

struct HotkeyDefinition { lholo::ui::HotkeyId id; char const* label; };
constexpr std::array<HotkeyDefinition, 9> kHotkeyDefinitions{{
    {lholo::ui::HotkeyId::Gui, "打开投影菜单"},
    {lholo::ui::HotkeyId::MoveXMinus, "结构偏移 X -1"},
    {lholo::ui::HotkeyId::MoveXPlus, "结构偏移 X +1"},
    {lholo::ui::HotkeyId::MoveZMinus, "结构偏移 Z -1"},
    {lholo::ui::HotkeyId::MoveZPlus, "结构偏移 Z +1"},
    {lholo::ui::HotkeyId::MoveYPlus, "结构偏移 Y +1"},
    {lholo::ui::HotkeyId::MoveYMinus, "结构偏移 Y -1"},
    {lholo::ui::HotkeyId::LayerIncrease, "上一层"},
    {lholo::ui::HotkeyId::LayerDecrease, "下一层"}
}};

lholo::ui::MenuModel makeMenuModel(float effectiveUiScale) {
    lholo::ui::MenuModel model;
    model.page = gActivePage;
    model.pathBuffer = detail::uiPathBuffer().data();
    model.pathBufferSize = detail::uiPathBuffer().size();
    model.blockOpeningInput = detail::uiOpeningInputBlockFrames().load(std::memory_order_acquire) > 0;
    model.uiScale = effectiveUiScale;
    auto const captureSnapshot = capture::getSnapshot();
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
    model.layerAxis = std::clamp(gLayerAxis.load(std::memory_order_relaxed), 0, 1);

    {
        std::lock_guard lock(gLoadedMutex);
        model.status = gStatus;
        model.hasLoadedStructure = static_cast<bool>(gLoaded);
        model.hasSavedProjection = gHasSavedProjection.load(std::memory_order_relaxed);
        model.savedAnchorX = gSavedAnchorX.load(std::memory_order_relaxed);
        model.savedAnchorY = gSavedAnchorY.load(std::memory_order_relaxed);
        model.savedAnchorZ = gSavedAnchorZ.load(std::memory_order_relaxed);
        if (gLoaded) {
            model.maxLayerY = maxLayerFor(*gLoaded, 0);
            model.maxLayerX = maxLayerFor(*gLoaded, 1);
        }
    }
    model.structureBoundsEnabled = projection::getStructureBoundsEnabled();
    model.easyPlaceEnabled = place::isEnabled();
    model.manualPlace = place::isManualMode();
    model.rangeEnabled = place::isRangeEnabled();
    model.placementRadius = place::getPlacementRadius();
    model.offsetX = gOffsetX.load(std::memory_order_relaxed);
    model.offsetY = gOffsetY.load(std::memory_order_relaxed);
    model.offsetZ = gOffsetZ.load(std::memory_order_relaxed);
    model.rotation = std::clamp(gRotationQuarterTurns.load(std::memory_order_relaxed), 0, 3);
    model.mirror = std::clamp(gMirrorMode.load(std::memory_order_relaxed), 0, 2);
    model.opacity = projection::getOpacity();
    model.correctionFillOpacity = projection::getCorrectionFillOpacity();
    model.correctionOutlineOpacity = projection::getCorrectionOutlineOpacity();
    model.layerDisplayMode = std::clamp(gLayerDisplayMode.load(std::memory_order_relaxed), 0, 3);
    model.displayLayer = std::clamp(
        gDisplayLayer.load(std::memory_order_relaxed), 0,
        model.layerAxis == 1 ? model.maxLayerX : model.maxLayerY
    );
    model.hudEnabled = detail::uiHudEnabled().load(std::memory_order_relaxed);
    model.hudPosition = std::clamp(detail::uiHudPosition().load(std::memory_order_relaxed), 0, 3);
    model.hudShowFileName = detail::uiHudShowFileName().load(std::memory_order_relaxed);
    model.hudShowLayer = detail::uiHudShowLayer().load(std::memory_order_relaxed);
    model.hudShowOverallProgress = detail::uiHudShowOverallProgress().load(std::memory_order_relaxed);
    model.hudShowProgress = detail::uiHudShowProgress().load(std::memory_order_relaxed);
    model.hudShowWrongState = detail::uiHudShowWrongState().load(std::memory_order_relaxed);
    model.hudShowWrongType = detail::uiHudShowWrongType().load(std::memory_order_relaxed);
    model.hudShowBlockEntity = detail::uiHudShowBlockEntity().load(std::memory_order_relaxed);
    for (auto const& definition : kHotkeyDefinitions) {
        auto const binding = hotkeyBinding(definition.id);
        auto& row = model.hotkeys[static_cast<std::size_t>(definition.id)];
        row.id = definition.id;
        row.label = definition.label;
        row.display = ui::hotkeyChordName(
            binding.modifiers->load(std::memory_order_relaxed),
            binding.key->load(std::memory_order_relaxed)
        );
        row.capturing = binding.capturing->load(std::memory_order_acquire);
    }
    {
        std::lock_guard lock(detail::uiMaterialMutex());
        model.materials.reserve(detail::uiMaterialRequirements().size());
        for (auto const& material : detail::uiMaterialRequirements()) {
            model.materials.push_back({material.displayName, material.typeName, material.count});
        }
    }
    return model;
}

void applyMenuModel(lholo::ui::MenuModel const& model, float effectiveUiScale) {
    bool changed = false;
    auto update = [&changed](auto& target, auto value) {
        if (target.load(std::memory_order_relaxed) == value) return;
        target.store(value, std::memory_order_relaxed);
        changed = true;
    };
    if (std::abs(model.uiScale - effectiveUiScale) > 0.001f) {
        auto const scale = std::clamp(model.uiScale, 1.0f, 5.0f);
        if (std::abs(detail::uiUiScale().load(std::memory_order_relaxed) - scale) > 0.001f) {
            detail::uiUiScale().store(scale, std::memory_order_relaxed);
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
    if (place::getPlacementRadius() != radius) { place::setPlacementRadius(radius); changed = true; }
    update(gOffsetX, model.offsetX);
    update(gOffsetY, model.offsetY);
    update(gOffsetZ, model.offsetZ);
    update(gRotationQuarterTurns, std::clamp(model.rotation, 0, 3));
    update(gMirrorMode, std::clamp(model.mirror, 0, 2));

    auto const opacity = std::clamp(model.opacity, 0.0f, 1.0f);
    if (std::abs(projection::getOpacity() - opacity) > 0.0001f) { projection::setOpacity(opacity); changed = true; }
    auto const fill = std::clamp(model.correctionFillOpacity, 0.0f, 1.0f);
    if (std::abs(projection::getCorrectionFillOpacity() - fill) > 0.0001f) { projection::setCorrectionFillOpacity(fill); changed = true; }
    auto const outline = std::clamp(model.correctionOutlineOpacity, 0.0f, 1.0f);
    if (std::abs(projection::getCorrectionOutlineOpacity() - outline) > 0.0001f) { projection::setCorrectionOutlineOpacity(outline); changed = true; }
    auto const layerAxis = std::clamp(model.layerAxis, 0, 1);
    update(gLayerAxis, layerAxis);
    update(gLayerDisplayMode, std::clamp(model.layerDisplayMode, 0, 3));
    auto displayMax = 0;
    {
        std::lock_guard lock(gLoadedMutex);
        if (gLoaded) displayMax = maxLayerFor(*gLoaded, layerAxis);
    }
    update(gDisplayLayer, std::clamp(model.displayLayer, 0, displayMax));
    update(detail::uiHudEnabled(), model.hudEnabled);
    update(detail::uiHudPosition(), std::clamp(model.hudPosition, 0, 3));
    update(detail::uiHudShowFileName(), model.hudShowFileName);
    update(detail::uiHudShowLayer(), model.hudShowLayer);
    update(detail::uiHudShowOverallProgress(), model.hudShowOverallProgress);
    update(detail::uiHudShowProgress(), model.hudShowProgress);
    update(detail::uiHudShowWrongState(), model.hudShowWrongState);
    update(detail::uiHudShowWrongType(), model.hudShowWrongType);
    update(detail::uiHudShowBlockEntity(), model.hudShowBlockEntity);
    capture::Draft captureDraft;
    captureDraft.mode = static_cast<capture::CaptureMode>(std::clamp(model.capture.mode, 0, 1));
    captureDraft.includeEntities = model.capture.includeEntities;
    if (model.capture.first.set) {
        captureDraft.first = capture::Point{
            model.capture.first.x, model.capture.first.y, model.capture.first.z
        };
    }
    if (model.capture.second.set) {
        captureDraft.second = capture::Point{
            model.capture.second.x, model.capture.second.y, model.capture.second.z
        };
    }
    capture::updateDraft(captureDraft);
    if (changed) saveSettings();
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
            std::lock_guard lock(gLoadedMutex);
            gStatus = "请选择或输入 .mcstructure / .litematic 文件路径";
            return;
        }
        std::string error;
        auto loaded = detail::loadStructureFile(pathFromUtf8(pathText), error);
        if (!loaded) {
            std::lock_guard lock(gLoadedMutex);
            gStatus = "加载失败: " + error;
            logger().error("Could not load structure {}: {}", pathText, error);
            return;
        }
        std::string status;
        {
            std::lock_guard lock(gLoadedMutex);
            gLastPath = pathText;
            gStatus = detail::makeStructureStatus(*loaded);
            status = gStatus;
            gLoaded = std::move(loaded);
        }
        saveSettings();
        refreshModel = true;
        logger().info("{}", status);
    };
    actions.restoreProjection = [&refreshModel] {
        std::string savedPath;
        {
            std::lock_guard lock(gLoadedMutex);
            savedPath = gSavedStructurePath;
        }
        auto const x = gSavedAnchorX.load(std::memory_order_relaxed);
        auto const y = gSavedAnchorY.load(std::memory_order_relaxed);
        auto const z = gSavedAnchorZ.load(std::memory_order_relaxed);
        std::string error;
        auto loaded = detail::loadStructureFile(pathFromUtf8(savedPath), error);
        if (!loaded) {
            std::lock_guard lock(gLoadedMutex);
            gStatus = "恢复失败: " + error;
            logger().error("Could not restore structure {}: {}", savedPath, error);
            return;
        }
        gRotationQuarterTurns.store(gSavedRotation.load(std::memory_order_relaxed), std::memory_order_relaxed);
        gMirrorMode.store(
            std::clamp(gSavedMirror.load(std::memory_order_relaxed), 0, 2),
            std::memory_order_relaxed
        );
        gOffsetX.store(gSavedOffsetX.load(std::memory_order_relaxed), std::memory_order_relaxed);
        gOffsetY.store(gSavedOffsetY.load(std::memory_order_relaxed), std::memory_order_relaxed);
        gOffsetZ.store(gSavedOffsetZ.load(std::memory_order_relaxed), std::memory_order_relaxed);
        gLayerDisplayMode.store(gSavedLayerDisplayMode.load(std::memory_order_relaxed), std::memory_order_relaxed);
        gDisplayLayer.store(gSavedDisplayLayer.load(std::memory_order_relaxed), std::memory_order_relaxed);
        gLayerAxis.store(gSavedLayerAxis.load(std::memory_order_relaxed), std::memory_order_relaxed);
        projection::requestNextStructureAnchor(x, y, z);
        {
            std::lock_guard lock(gLoadedMutex);
            gLastPath = savedPath;
            gStatus = "已恢复上次投影记录，等待进入渲染";
            gLoaded = std::move(loaded);
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
        if (auto const binding = hotkeyBinding(id); binding.capturing) binding.capturing->store(true, std::memory_order_release);
    };
    actions.clearHotkey = [](lholo::ui::HotkeyId id) {
        if (auto const binding = hotkeyBinding(id); binding.key && binding.modifiers) {
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
        std::lock_guard lock(gLoadedMutex);
        std::snprintf(detail::uiPathBuffer().data(), detail::uiPathBuffer().size(), "%s", gLastPath.c_str());
        detail::uiPathInitialized() = true;
    }
    auto const metrics = lholo::ui::calculateMetrics(displaySize, effectiveScale);
    lholo::ui::applyFluentTheme(metrics);
    auto model = makeMenuModel(effectiveScale);
    bool refreshModel = false;
    auto const actions = makeMenuActions(refreshModel);
    lholo::ui::renderMenu(model, actions, metrics);
    gActivePage = model.page;
    if (!refreshModel) applyMenuModel(model, effectiveScale);
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
        std::lock_guard lock(gLoadedMutex);
        gLastPath = settings.lastStructurePath;
        detail::uiUiScale().store(std::clamp(settings.uiScale, 0.0f, 5.0f), std::memory_order_relaxed);
        projection::setOpacity(settings.opacity);
        projection::setCorrectionFillOpacity(settings.correctionFillOpacity);
        projection::setCorrectionOutlineOpacity(settings.correctionOutlineOpacity);
        projection::setStructureBoundsEnabled(settings.structureBoundsEnabled);
        // Transform and layer state are session-local. Only the explicit
        // "restore last projection" record below is persisted.
        gRotationQuarterTurns.store(0, std::memory_order_relaxed);
        gMirrorMode.store(0, std::memory_order_relaxed);
        gOffsetX.store(0, std::memory_order_relaxed);
        gOffsetY.store(0, std::memory_order_relaxed);
        gOffsetZ.store(0, std::memory_order_relaxed);
        gLayerDisplayMode.store(0, std::memory_order_relaxed);
        gDisplayLayer.store(0, std::memory_order_relaxed);
        gLayerAxis.store(0, std::memory_order_relaxed);
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
        gHasSavedProjection.store(settings.hasSavedProjection, std::memory_order_relaxed);
        gSavedAnchorX.store(settings.savedAnchorX, std::memory_order_relaxed);
        gSavedAnchorY.store(settings.savedAnchorY, std::memory_order_relaxed);
        gSavedAnchorZ.store(settings.savedAnchorZ, std::memory_order_relaxed);
        gSavedRotation.store(settings.savedRotation, std::memory_order_relaxed);
        gSavedMirror.store(std::clamp(settings.savedMirror, 0, 2), std::memory_order_relaxed);
        gSavedOffsetX.store(settings.savedOffsetX, std::memory_order_relaxed);
        gSavedOffsetY.store(settings.savedOffsetY, std::memory_order_relaxed);
        gSavedOffsetZ.store(settings.savedOffsetZ, std::memory_order_relaxed);
        gSavedLayerDisplayMode.store(settings.savedLayerDisplayMode, std::memory_order_relaxed);
        gSavedDisplayLayer.store(settings.savedDisplayLayer, std::memory_order_relaxed);
        gSavedLayerAxis.store(std::clamp(settings.savedLayerAxis, 0, 1), std::memory_order_relaxed);
        gSavedStructurePath = settings.savedStructurePath;
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
            std::lock_guard lock(gLoadedMutex);
            hasActiveProjection = static_cast<bool>(gLoaded);
        }
        if (hasActiveProjection && gHasSavedProjection.load(std::memory_order_acquire)) {
            // Only an active projection may update its restore snapshot. At
            // startup the session-local transform/layer values intentionally
            // reset to defaults; copying those values before the user restores
            // a structure would silently destroy the saved state.
            gSavedRotation.store(gRotationQuarterTurns.load(std::memory_order_relaxed), std::memory_order_relaxed);
            gSavedMirror.store(gMirrorMode.load(std::memory_order_relaxed), std::memory_order_relaxed);
            gSavedOffsetX.store(gOffsetX.load(std::memory_order_relaxed), std::memory_order_relaxed);
            gSavedOffsetY.store(gOffsetY.load(std::memory_order_relaxed), std::memory_order_relaxed);
            gSavedOffsetZ.store(gOffsetZ.load(std::memory_order_relaxed), std::memory_order_relaxed);
            gSavedLayerDisplayMode.store(gLayerDisplayMode.load(std::memory_order_relaxed), std::memory_order_relaxed);
            gSavedDisplayLayer.store(gDisplayLayer.load(std::memory_order_relaxed), std::memory_order_relaxed);
            gSavedLayerAxis.store(gLayerAxis.load(std::memory_order_relaxed), std::memory_order_relaxed);
        }
        std::string lastPath;
        std::string savedStructurePath;
        {
            std::lock_guard lock(gLoadedMutex);
            lastPath = gLastPath;
            savedStructurePath = gSavedStructurePath;
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
        settings.hasSavedProjection = gHasSavedProjection.load(std::memory_order_relaxed);
        settings.savedAnchorX = gSavedAnchorX.load(std::memory_order_relaxed);
        settings.savedAnchorY = gSavedAnchorY.load(std::memory_order_relaxed);
        settings.savedAnchorZ = gSavedAnchorZ.load(std::memory_order_relaxed);
        settings.savedRotation = gSavedRotation.load(std::memory_order_relaxed);
        settings.savedMirror = gSavedMirror.load(std::memory_order_relaxed);
        settings.savedOffsetX = gSavedOffsetX.load(std::memory_order_relaxed);
        settings.savedOffsetY = gSavedOffsetY.load(std::memory_order_relaxed);
        settings.savedOffsetZ = gSavedOffsetZ.load(std::memory_order_relaxed);
        settings.savedLayerDisplayMode = gSavedLayerDisplayMode.load(std::memory_order_relaxed);
        settings.savedDisplayLayer = gSavedDisplayLayer.load(std::memory_order_relaxed);
        settings.savedLayerAxis = gSavedLayerAxis.load(std::memory_order_relaxed);
        settings.savedStructurePath = savedStructurePath;
        lholo::settings::saveSettingsFile(path, settings);
    } catch (std::exception const& exception) {
        logger().error("Could not save projection settings {}: {}", path.string(), exception.what());
    }
}

std::shared_ptr<LoadedStructure const> getLoaded() {
    std::lock_guard lock(gLoadedMutex);
    return gLoaded;
}

int getRotationQuarterTurns() {
    return gRotationQuarterTurns.load(std::memory_order_relaxed);
}

int getMirrorMode() {
    return std::clamp(gMirrorMode.load(std::memory_order_relaxed), 0, 2);
}

int getOffsetX() { return gOffsetX.load(std::memory_order_relaxed); }
int getOffsetY() { return gOffsetY.load(std::memory_order_relaxed); }
int getOffsetZ() { return gOffsetZ.load(std::memory_order_relaxed); }
int getLayerDisplayMode() { return gLayerDisplayMode.load(std::memory_order_relaxed); }
int getDisplayLayer() { return gDisplayLayer.load(std::memory_order_relaxed); }
int getLayerAxis() { return gLayerAxis.load(std::memory_order_relaxed); }

void recordProjectionAnchor(int x, int y, int z) {
    gSavedAnchorX.store(x, std::memory_order_relaxed);
    gSavedAnchorY.store(y, std::memory_order_relaxed);
    gSavedAnchorZ.store(z, std::memory_order_relaxed);
    gSavedRotation.store(gRotationQuarterTurns.load(std::memory_order_relaxed), std::memory_order_relaxed);
    gSavedMirror.store(gMirrorMode.load(std::memory_order_relaxed), std::memory_order_relaxed);
    gSavedOffsetX.store(gOffsetX.load(std::memory_order_relaxed), std::memory_order_relaxed);
    gSavedOffsetY.store(gOffsetY.load(std::memory_order_relaxed), std::memory_order_relaxed);
    gSavedOffsetZ.store(gOffsetZ.load(std::memory_order_relaxed), std::memory_order_relaxed);
    gSavedLayerDisplayMode.store(gLayerDisplayMode.load(std::memory_order_relaxed), std::memory_order_relaxed);
    gSavedDisplayLayer.store(gDisplayLayer.load(std::memory_order_relaxed), std::memory_order_relaxed);
    gSavedLayerAxis.store(gLayerAxis.load(std::memory_order_relaxed), std::memory_order_relaxed);
    {
        std::lock_guard lock(gLoadedMutex);
        gSavedStructurePath = gLastPath;
    }
    gHasSavedProjection.store(true, std::memory_order_release);
    saveSettings();
}

void clear() {
    // Withdraw the requested structure before waiting for the mesh worker.
    // Otherwise the render hook can observe the old gLoaded in the gap after
    // projection::disable() and immediately enable the projection again.
    {
        std::lock_guard lock(gLoadedMutex);
        gLoaded.reset();
        gStatus = "已关闭投影";
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
