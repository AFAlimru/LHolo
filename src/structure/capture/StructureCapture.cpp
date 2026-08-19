#include "structure/capture/StructureCapture.h"

#include "structure/capture/McstructureExporter.h"

#include <algorithm>
#include <mutex>

#include "ll/api/service/Bedrock.h"
#include "mc/client/game/ClientInstance.h"
#include "mc/client/player/LocalPlayer.h"
#include "mc/world/level/BlockPos.h"
#include "mc/world/level/BlockSource.h"
#include "mc/world/level/Level.h"
#include "mc/world/level/dimension/Dimension.h"
#include "mc/world/level/levelgen/structure/BoundingBox.h"
#include "mc/world/level/levelgen/structure/StructureTemplate.h"

namespace lholo::structure::capture {
namespace {

std::mutex    gMutex;
Draft         gDraft;
std::string   gStatus{"请设置选区点 1 和点 2"};
Level*        gLevel{};
Dimension*    gDimension{};
std::uint64_t gRevision{};

struct ClientContext {
    LocalPlayer* player{};
    Level*       level{};
    Dimension*   dimension{};
};

ClientContext currentContext() {
    auto client = ll::service::getClientInstance();
    auto* player = client ? client->getLocalPlayer() : nullptr;
    if (!player) return {};
    return {player, &player->getLevel(), &player->getDimension()};
}

void resetLocked() {
    gDraft = {};
    gStatus = "请设置选区点 1 和点 2";
    ++gRevision;
}

void syncContextLocked(ClientContext const& context) {
    if (!context.player) {
        if (gLevel || gDimension || gDraft.first || gDraft.second) resetLocked();
        gLevel = nullptr;
        gDimension = nullptr;
        return;
    }
    if (gLevel && (gLevel != context.level || gDimension != context.dimension)) resetLocked();
    gLevel = context.level;
    gDimension = context.dimension;
}

Bounds normalizedBounds(Draft const& draft) {
    auto const& first = *draft.first;
    auto const& second = *draft.second;
    return {
        {std::min(first.x, second.x), std::min(first.y, second.y), std::min(first.z, second.z)},
        {std::max(first.x, second.x), std::max(first.y, second.y), std::max(first.z, second.z)}
    };
}

void setStatus(std::string const& status) {
    std::lock_guard lock(gMutex);
    gStatus = status;
}

} // namespace

Snapshot getSnapshot() {
    auto const context = currentContext();
    std::lock_guard lock(gMutex);
    syncContextLocked(context);
    return {gDraft, static_cast<bool>(context.player), gStatus, gRevision};
}

std::optional<Bounds> getBounds() {
    auto const context = currentContext();
    std::lock_guard lock(gMutex);
    syncContextLocked(context);
    if (!context.player || !gDraft.first || !gDraft.second) return std::nullopt;
    return normalizedBounds(gDraft);
}

void updateDraft(Draft const& draft) {
    auto const context = currentContext();
    std::lock_guard lock(gMutex);
    syncContextLocked(context);
    if (!context.player) return;
    if (gDraft == draft) return;
    gDraft = draft;
    if (gDraft.first && gDraft.second) {
        gStatus = "选区已设置";
    } else if (gDraft.first || gDraft.second) {
        gStatus = "请继续设置另一个选区点";
    } else {
        gStatus = "请设置选区点 1 和点 2";
    }
    ++gRevision;
}

void setPointFromPlayer(PointSlot slot) {
    auto const context = currentContext();
    if (!context.player) {
        setStatus("尚未进入世界");
        return;
    }
    auto const position = context.player->getFeetBlockPos();

    std::lock_guard lock(gMutex);
    syncContextLocked(context);
    auto& point = slot == PointSlot::First ? gDraft.first : gDraft.second;
    point = Point{position.x, position.y, position.z};
    gStatus = slot == PointSlot::First ? "已记录选区点 1" : "已记录选区点 2";
    ++gRevision;
}

void exportStructure(Draft const& draft, std::filesystem::path const& output) {
    auto const context = currentContext();
    if (!context.player) {
        setStatus("尚未进入世界");
        return;
    }
    if (draft.mode != CaptureMode::Client) {
        setStatus("单人存档模式尚未实现");
        return;
    }
    if (!draft.first || !draft.second) {
        setStatus("请先设置选区点 1 和点 2");
        return;
    }

    Bounds bounds;
    {
        std::lock_guard lock(gMutex);
        syncContextLocked(context);
        if (gDraft != draft) {
            gDraft = draft;
            ++gRevision;
        }
        bounds = normalizedBounds(gDraft);
    }

    BlockPos const min{bounds.min.x, bounds.min.y, bounds.min.z};
    BlockPos const max{bounds.max.x, bounds.max.y, bounds.max.z};
    auto& region = context.player->getDimensionBlockSource();
    if (!region.areChunksFullyLoaded(min, max)) {
        setStatus("选区包含客户端尚未完整加载的区域");
        return;
    }

    auto structure = StructureTemplate::create(
        "lholo:client_export",
        region,
        BoundingBox{min, max},
        false,
        !draft.includeEntities
    );
    if (!structure) {
        setStatus("原版 StructureTemplate 无法捕获该选区");
        return;
    }
    if (!exportMcstructure(*structure, output)) {
        setStatus("写入 .mcstructure 文件失败");
        return;
    }
    setStatus("结构已成功导出");
}

void clear() {
    std::lock_guard lock(gMutex);
    resetLocked();
}

} // namespace lholo::structure::capture
