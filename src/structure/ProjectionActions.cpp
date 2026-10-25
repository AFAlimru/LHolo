#include "structure/ProjectionActions.h"

#include "structure/StructureFileLoader.h"
#include "structure/StructureLoader.h"
#include "structure/StructureSession.h"

#include "ll/api/service/Bedrock.h"
#include "mc/client/game/ClientInstance.h"
#include "mc/client/player/LocalPlayer.h"
#include "mc/world/actor/Actor.h"

#include <cmath>
#include <mutex>
#include <optional>
#include <utility>
#include <gsl/util>

namespace lholo::structure {
namespace {
enum class Action { Load, Restore, Close };
struct Request { Action action; std::string path; };
std::mutex requestMutex;
std::optional<Request> pending;
bool executing{};

bool enqueue(Action action, std::string path = {}) {
    auto const loading = action != Action::Close && detail::isStructureFileLoading();
    std::lock_guard lock(requestMutex);
    if (action != Action::Close && (pending || executing || loading)) return false;
    pending = Request{action, std::move(path)};
    return true;
}
}

bool requestProjectionLoad(std::string path) { return enqueue(Action::Load, std::move(path)); }
bool requestProjectionRestore() { return enqueue(Action::Restore); }
void requestProjectionClose() { (void)enqueue(Action::Close); }

bool projectionActionPending() {
    std::lock_guard lock(requestMutex);
    return pending.has_value() || executing;
}

void resetProjectionActions() {
    std::lock_guard lock(requestMutex);
    pending.reset();
}

void processProjectionActions() {
    std::optional<Request> request;
    {
        std::lock_guard lock(requestMutex);
        if (!pending || executing) return;
        request = std::exchange(pending, std::nullopt);
        executing = true;
    }
    auto finish = gsl::finally([] {
        std::lock_guard lock(requestMutex);
        executing = false;
    });
    auto& session = detail::StructureSession::getInstance();
    if (request->action == Action::Close) {
        clear(); // freezes the restore snapshot and invalidates late parser results
        saveSettings();
        return;
    }
    if (detail::isStructureFileLoading()) return;
    auto client = ll::service::getClientInstance();
    auto* player = client ? client->getLocalPlayer() : nullptr;
    if (!player) {
        session.setStatus(i18n::Message{i18n::TextKey::CaptureStatusNoWorld});
        return;
    }
    if (request->action == Action::Restore) {
        auto const saved = session.savedProjection();
        if (!saved.available || saved.structurePath.empty()) {
            session.setStatus(i18n::Message{i18n::TextKey::HintNoSavedProjection});
            return;
        }
        (void)detail::startSavedProjectionLoad();
        return;
    }
    if (request->path.empty()) {
        session.setStatus(i18n::Message{i18n::TextKey::StatusPathEmpty});
        return;
    }
    auto const position = player->getPosition();
    (void)detail::startStructureFileLoad(std::move(request->path), detail::StructureLoadOrigin{
        static_cast<int>(std::floor(position.x)), static_cast<int>(std::floor(position.y)) - 1,
        static_cast<int>(std::floor(position.z)), static_cast<int>(player->getDimensionId())
    });
}
}
