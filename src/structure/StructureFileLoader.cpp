// LHolo - Client-side projection renderer for Minecraft Bedrock Windows
// Copyright (C) 2026  MarmieQi

#include "structure/StructureFileLoader.h"

#include "plugin/LHolo.h"
#include "structure/MaterialTracker.h"
#include "structure/StructureLoader.h"
#include "structure/StructurePaths.h"
#include "structure/StructureSession.h"
#include "structure/StructureUiState.h"
#include "structure/formats/StructureFormatLoaders.h"

#include <chrono>
#include <exception>
#include <future>
#include <mutex>
#include <optional>
#include <utility>

#include <gsl/util>
#include "ll/api/mod/NativeMod.h"
#include "ll/api/service/Bedrock.h"
#include "mc/world/level/Level.h"
#include "mc/world/level/LevelListener.h"

namespace lholo::structure::detail {
namespace {

struct FileLoader final : LevelListener {
    std::mutex mutex;
    std::future<void> task;
    Level* level{};
    bool stopped{true};

    void onLevelDestruction(std::string const&) override {
        std::lock_guard lock(mutex);
        // StructureTemplate uses this world's non-owning unknown-block
        // registry. Stop publication and finish parsing BEFORE it is freed,
        // including a first load which has no active projection listener yet.
        level = nullptr;
        StructureSession::getInstance().clearLoaded(i18n::Message{i18n::TextKey::StatusWorldExited});
        if (task.valid()) task.wait();
        // The dying Level removes its listeners; never remove through it later.
    }
};

FileLoader& fileLoader() {
    static FileLoader loader;
    return loader;
}

auto& logger() { return LHolo::getInstance().getSelf().getLogger(); }

bool startLoad(std::string path, StructureLoadOrigin origin, std::optional<SavedProjectionSnapshot> saved) {
    auto& loader = fileLoader();
    std::lock_guard lock(loader.mutex);
    if (loader.stopped) return false;
    if (loader.task.valid()) {
        if (loader.task.wait_for(std::chrono::seconds{0}) != std::future_status::ready) return false;
        loader.task.get();
    }

    auto currentLevel = ll::service::getMultiPlayerLevel();
    if (!currentLevel) return false;
    if (loader.level != &*currentLevel) {
        if (loader.level) loader.level->removeListener(loader);
        currentLevel->addListener(loader);
        loader.level = &*currentLevel;
    }

    auto& session = StructureSession::getInstance();
    auto const request = session.beginFileLoad(origin);
    auto& ui = StructureUiState::getInstance();
    auto const hint = ui.beginLoadingHint();
    auto const failureKey = saved ? i18n::TextKey::StatusRestoreFailed : i18n::TextKey::StatusLoadFailed;
    try {
        loader.task = std::async(std::launch::async, [path = std::move(path), saved = std::move(saved), request, hint, failureKey] {
            auto finishHint = gsl::finally([&] { StructureUiState::getInstance().finishLoadingHint(hint); });
            auto& session = StructureSession::getInstance();
            auto fail = [&](std::string error) {
                if (session.failFileLoad(request, i18n::Message{failureKey, {error}})) {
                    logger().error("Could not load structure {}: {}", path, error);
                }
            };
            try {
                std::string error;
                auto loaded = loadStructureFile(pathFromUtf8(path), error);
                if (!loaded) {
                    fail(std::move(error));
                    return;
                }
                auto const count = loaded->renderBlocks.size();
                auto const status = saved ? i18n::Message{i18n::TextKey::StatusRestoredPending}
                                          : makeLoadedStatusMessage(*loaded);
                bool const published = saved
                    ? session.completeRestoreLoad(request, std::move(loaded), *saved, status)
                    : session.completeFileLoad(request, std::move(loaded), path, status);
                if (!published) return;
                invalidateMaterialList();
                if (!saved) saveSettings();
                logger().info("Loaded structure {}: {} renderable blocks", path, count);
            } catch (std::exception const& error) {
                fail(error.what());
            } catch (...) {
                fail("Unknown exception while loading structure");
            }
        });
    } catch (std::exception const& error) {
        ui.finishLoadingHint(hint);
        session.failFileLoad(request, i18n::Message{failureKey, {error.what()}});
        logger().error("Could not start background structure load: {}", error.what());
        return false;
    }
    return true;
}

} // namespace

void enableStructureFileLoader() {
    auto& loader = fileLoader();
    std::lock_guard lock(loader.mutex);
    loader.stopped = false;
}

void shutdownStructureFileLoader() {
    auto& loader = fileLoader();
    std::lock_guard lock(loader.mutex);
    loader.stopped = true;
    StructureSession::getInstance().cancelPendingLoad();
    // Keep the task in the gate until it is joined: a concurrent destruction
    // callback must not mistake a moved-out future for a finished parser.
    // The parser never acquires loader.mutex.
    if (loader.task.valid()) loader.task.get();
    if (loader.level) {
        loader.level->removeListener(loader);
        loader.level = nullptr;
    }
    StructureUiState::getInstance().clearLoadingHint();
}

bool startStructureFileLoad(std::string path, StructureLoadOrigin origin) {
    return startLoad(std::move(path), origin, std::nullopt);
}

bool startSavedProjectionLoad() {
    auto saved = StructureSession::getInstance().savedProjection();
    auto const origin = StructureLoadOrigin{saved.anchorX, saved.anchorY, saved.anchorZ, std::nullopt};
    auto path = saved.structurePath;
    return startLoad(std::move(path), origin, std::move(saved));
}

} // namespace lholo::structure::detail
