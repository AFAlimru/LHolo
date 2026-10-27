#include "app/TaskContext.h"
#include "plugin/LHolo.h"
#include "ll/api/mod/NativeMod.h"
#include "ll/api/service/Bedrock.h"
#include "mc/client/game/ClientInstance.h"
#include "mc/client/game/ClientInstanceState.h"
#include "mc/client/player/LocalPlayer.h"
#include <cmath>

namespace lholo::app::detail {
std::filesystem::path taskStorePath() {
    return LHolo::getInstance().getSelf().getDataDir() / "tasks.json";
}
std::optional<structure::detail::StructureLoadOrigin> taskPlayerOrigin() {
    auto client = ll::service::getClientInstance();
    auto* player = client && client->$getClientInstanceState() == ClientInstanceState::Playing
        ? client->getLocalPlayer() : nullptr;
    if (!player) return {};
    auto pos = player->getPosition();
    return structure::detail::StructureLoadOrigin{static_cast<int>(std::floor(pos.x)),
        static_cast<int>(std::floor(pos.y)) - 1, static_cast<int>(std::floor(pos.z)),
        static_cast<int>(player->getDimensionId())};
}
}
