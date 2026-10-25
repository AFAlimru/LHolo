#include "oreui/MenuBindings.h"

#include "i18n/Translator.h"
#include "structure/StructurePaths.h"
#include "structure/ProjectionActions.h"
#include "structure/StructureFileLoader.h"
#include "structure/StructureSession.h"
#include "ui/StructureFilePicker.h"
#include "nlohmann/json.hpp"

#include <mutex>
#include <exception>
#include <cstdio>

namespace lholo::oreui {
namespace {
std::mutex bindingsMutex;
ui::StructureFilePicker fileBrowser;
std::string selectedPath;
unsigned long browseError{};
bool fileDirty{}, publishedBusy{};
std::string publishedProjection;
std::uint64_t observedGeneration{};
}

std::string buildMenuLabels() {
    nlohmann::json labels = nlohmann::json::object();
    auto const language = i18n::language();
    for (std::size_t index = 0; index < i18n::kTextKeyCount; ++index) {
        labels[std::string{i18n::kTextKeyIds[index]}] = i18n::tr(static_cast<i18n::TextKey>(index), language);
    }
    return labels.dump();
}

void requestStructureFileBrowse() {
    std::lock_guard lock(bindingsMutex);
    fileDirty = true; // Always acknowledge even when the shared picker gate refuses.
    if (structure::projectionActionPending() || structure::detail::isStructureFileLoading()) return;
    try {
        if (fileBrowser.start(structure::detail::pathFromUtf8(selectedPath))) {
            browseError = 0;
            fileDirty = true;
        }
    } catch (std::exception const&) {
        browseError = 1;
        fileDirty = true;
    }
}

void loadSelectedProjection() {
    std::lock_guard lock(bindingsMutex);
    if (!fileBrowser.busy()) (void)structure::requestProjectionLoad(selectedPath);
}

void closeActiveProjection() { structure::requestProjectionClose(); }

void restoreLastProjection() {
    std::lock_guard lock(bindingsMutex);
    if (fileBrowser.busy()) return;
    auto const saved = structure::detail::StructureSession::getInstance().savedProjection();
    if (structure::requestProjectionRestore() && saved.available) {
        selectedPath = saved.structurePath;
        fileDirty = true;
    }
}

std::optional<std::string> takeProjectionUpdate(bool force) {
    std::lock_guard lock(bindingsMutex);
    auto const snapshot = structure::detail::StructureSession::getInstance().snapshot();
    if (snapshot.loaded && snapshot.loaded->generation != observedGeneration) {
        observedGeneration = snapshot.loaded->generation;
        selectedPath = snapshot.lastPath;
        fileDirty = true;
    }
    char origin[512]{};
    if (snapshot.saved.available) std::snprintf(origin, sizeof(origin),
        i18n::tr(i18n::TextKey::LabelSavedOrigin), snapshot.saved.anchorX, snapshot.saved.anchorY, snapshot.saved.anchorZ);
    auto state = nlohmann::json{
        {"status", snapshot.status},
        {"loading", structure::projectionActionPending() || structure::detail::isStructureFileLoading()},
        {"hasProjection", static_cast<bool>(snapshot.loaded)},
        {"canRestore", snapshot.saved.available && !snapshot.saved.structurePath.empty()},
        {"savedOrigin", origin}
    }.dump();
    if (!force && state == publishedProjection) return std::nullopt;
    publishedProjection = state;
    return state;
}

std::optional<std::string> takeStructureFileUpdate(bool force) {
    std::lock_guard lock(bindingsMutex);
    if (auto result = fileBrowser.takeResult()) {
        if (result->path) selectedPath = structure::detail::pathToUtf8(*result->path);
        browseError = result->errorCode;
        fileDirty = true;
    }
    auto const busy = fileBrowser.busy();
    if (!force && !fileDirty && busy == publishedBusy) return std::nullopt;
    publishedBusy = busy;
    fileDirty = false;
    return nlohmann::json{{"path", selectedPath}, {"browsing", busy}, {"error", browseError}}.dump();
}

void resetMenuBindings() {
    std::lock_guard lock(bindingsMutex);
    fileBrowser.cancel();
    browseError = 0;
    fileDirty = true;
    publishedProjection.clear();
}

void shutdownMenuBindings() {
    std::lock_guard lock(bindingsMutex);
    fileBrowser.shutdown();
    browseError = 0;
    fileDirty = true;
}
}
