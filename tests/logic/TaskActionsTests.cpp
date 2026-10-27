#include "app/TaskActions.h"
#include "app/TaskContext.h"
#include "structure/StructureFileLoader.h"
#include "structure/IndependentTaskFiles.h"
#include "structure/MaterialTracker.h"
#include "tasks/TaskStore.h"
#include <cstdio>
#include <algorithm>
#include <array>
#include <unordered_map>
#include <utility>

namespace {
std::filesystem::path taskPath;
bool world{}, inFlight{};
std::uint64_t worldRevision{1};
lholo::structure::detail::StructureLoadOrigin playerOrigin{10, 20, 30, 0};
std::shared_ptr<lholo::structure::detail::CompositionLoadResult> requested, completed;
std::vector<std::string> requestedPaths;
std::unordered_map<std::string, std::shared_ptr<lholo::structure::LoadedStructure const>> cache;
}
// Only world/file I/O boundaries are replaced. The real control frame, session,
// composer, catalog rules and asynchronous store writer run in this test.
namespace lholo::app::detail {
std::filesystem::path taskStorePath() { return taskPath; }
std::uint64_t taskWorldRevision() { return worldRevision; }
std::optional<structure::detail::StructureLoadOrigin> taskPlayerOrigin() {
    if (!world) return {};
    return playerOrigin;
}
}
namespace lholo::structure {
void clear() { detail::StructureSession::getInstance().clearLoaded({}); }
}
namespace lholo::structure::detail {
void invalidateMaterialList() {}
bool isStructureFileLoading() { return inFlight; }
std::shared_ptr<LoadedStructure const> cachedTaskFile(std::string const& path) {
    auto it = cache.find(path);
    return it == cache.end() ? nullptr : it->second;
}
bool startTaskComposition(std::string id, std::uint64_t revision, StructureLoadOrigin origin,
    std::vector<CompositionFile> files, std::vector<std::string> paths, bool) {
    if (inFlight) return false;
    requested = std::make_shared<CompositionLoadResult>();
    requested->request = StructureSession::getInstance().beginCompositionLoad(origin);
    requested->revision = revision;
    requested->taskId = std::move(id);
    requested->files = std::move(files);
    requestedPaths = std::move(paths);
    inFlight = true;
    return true;
}
std::shared_ptr<CompositionLoadResult> takeTaskCompositionResult() {
    return std::exchange(completed, {});
}
}

int runTaskActionsTests() {
    using namespace lholo;
    using namespace app;
    int failures{};
    auto check = [&](bool ok, char const* message) {
        if (!ok) { ++failures; std::fprintf(stderr, "TaskActionsTests: %s\n", message); }
    };
    auto directory = std::filesystem::temp_directory_path() / tasks::newId();
    taskPath = directory / "tasks.json";
    world = true;
    auto& session = structure::detail::StructureSession::getInstance();
    session.clearLoaded({});
    loadTasks();
    auto command = [&](TaskAction action, std::string value = {}, std::string file = {}, std::string task = {}) {
        auto s = getTaskSnapshot();
        TaskIntent intent{s.ack + 1, s.revision, action, task.empty() ? s.catalog.selectedTaskId : task, file, value};
        check(submitTaskIntent(intent), "queue task action");
        processTaskActions();
        check(getTaskStatus().accepted, "action accepted by real control frame");
    };
    auto finish = [&](bool success) {
        check(inFlight && requested != nullptr, "composition was scheduled");
        if (!requested) return;
        if (success) {
            for (std::size_t i = 0; i < requested->files.size(); ++i) {
                auto& file = requested->files[i];
                auto loaded = std::make_shared<structure::LoadedStructure>();
                loaded->sizeX = loaded->sizeY = loaded->sizeZ = 1;
                loaded->sourcePath = requestedPaths[i];
                loaded->renderBlocks.push_back({0, 0, 0, reinterpret_cast<Block const*>(0x1000)});
                file.loaded = loaded;
                cache[requestedPaths[i]] = loaded;
            }
            requested->composition = structure::composeTaskProjection(requested->files,
                [](Block const* b, int, int) { return b; });
        } else {
            requested->error = "Missing file";
            requested->errorFileId = requested->files.back().id;
        }
        inFlight = false;
        completed = std::move(requested);
        processTaskActions();
    };
    command(TaskAction::Create, "城堡"); finish(true);
    command(TaskAction::AddFile, "body.mcstructure"); finish(true);
    auto fileId = getTaskSnapshot().catalog.tasks[0].files[0].id;
    auto original = session.loaded();
    command(TaskAction::Path, "missing.mcstructure", fileId); finish(false);
    check(session.loaded() == original && session.lastPath() == "body.mcstructure",
          "failed path change preserves projection and binds the last successful editor");
    check(getTaskSnapshot().catalog.tasks[0].files[0].path == "missing.mcstructure",
          "failed path remains available for user repair");
    session.adjustOffsets(3, 0, 0);
    processTaskActions();
    check(inFlight && requestedPaths == std::vector<std::string>{"body.mcstructure"},
          "hotkey after failure recomposes the actual active files, not the invalid catalog");
    check(requested && requested->files[0].x == 3, "live offset carried to composition");
    finish(true);
    check(session.transform().offsetX == 3 && session.lastPath() == "body.mcstructure",
          "successful live edit keeps original source binding");
    check(getTaskSnapshot().catalog.tasks[0].files[0].x == 3 &&
          getTaskSnapshot().catalog.tasks[0].files[0].path == "missing.mcstructure",
          "persist transform by stable ID without replacing the user's missing path");
    command(TaskAction::Path, "repaired.mcstructure", fileId); finish(true);
    check(session.lastPath() == "repaired.mcstructure" && getTaskStatus().error.empty(),
          "repair publishes catalog and clears loading error");
    session.adjustOffsets(100, 0, 0); processTaskActions(); finish(false);
    check(session.transform().offsetX == 3,
          "failed live transform restores the last successful transform, not the failed request");
    command(TaskAction::AddFile, "missing-two.mcstructure"); finish(false);
    check(session.selectedSource() < 0, "failed new file does not edit the previously selected file");
    command(TaskAction::SelectFile, {}, fileId);
    session.adjustOffsets(0, 2, 0); processTaskActions();
    check(inFlight && requested->files.size() == 1 && requested->files[0].y == 2,
          "existing file remains editable after another file fails to attach");
    finish(true);
    // Compare actual file/world positions, not just the normalized composite
    // origin: reopening also loads a catalog file absent from the old result.
    auto fileWorldPosition = [&](std::string const& id) -> std::optional<std::array<long long, 3>> {
        auto const activation = session.activation();
        if (!activation.loaded || !activation.origin) return {};
        auto const& loaded = *activation.loaded;
        auto source = std::find(loaded.sourceFiles.begin(), loaded.sourceFiles.end(), id);
        if (source == loaded.sourceFiles.end()) return {};
        auto index = static_cast<int>(source - loaded.sourceFiles.begin());
        if (auto const* file=structure::independentTaskFile(loaded,index); file && file->loaded && file->visible) {
            return std::array<long long,3>{static_cast<long long>(activation.origin->x)+file->x,
                static_cast<long long>(activation.origin->y)+file->y,
                static_cast<long long>(activation.origin->z)+file->z};
        }
        for (auto const& block : loaded.renderBlocks) if (block.sourceIndex == index)
            return std::array<long long, 3>{static_cast<long long>(activation.origin->x) + block.x,
                static_cast<long long>(activation.origin->y) + block.y,
                static_cast<long long>(activation.origin->z) + block.z};
        return {};
    };
    auto const originalTask = getTaskSnapshot().catalog.selectedTaskId;
    auto const originalPosition = fileWorldPosition(fileId);
    command(TaskAction::Close);
    playerOrigin = {100, 70, -200, 0};
    command(TaskAction::Activate);
    check(requested->request.origin == structure::detail::StructureLoadOrigin{10, 20, 30, 0},
          "reopen after walking uses the task's original anchor");
    finish(true);
    check(originalPosition && fileWorldPosition(fileId) == originalPosition,
          "reopen preserves file world position even when normalization changes");
    command(TaskAction::Close);
    ++worldRevision;
    playerOrigin = {200, 80, 300, 0};
    command(TaskAction::Activate);
    check(requested->request.origin == playerOrigin, "new world clears anchors even while tasks are closed");
    finish(true);
    auto const newWorldPosition = fileWorldPosition(fileId);
    command(TaskAction::Close);
    playerOrigin = {500, 90, 600, 1};
    command(TaskAction::Activate);
    check(requested->request.origin == playerOrigin, "different dimension gets its own anchor");
    finish(true);
    command(TaskAction::Close);
    playerOrigin = {700, 100, 800, 0};
    command(TaskAction::Activate);
    check(requested->request.origin == structure::detail::StructureLoadOrigin{200, 80, 300, 0},
          "returning dimension reuses that dimension's anchor");
    finish(true);
    check(fileWorldPosition(fileId) == newWorldPosition, "dimension-scoped reopen preserves file world position");
    command(TaskAction::Create, "第二个任务"); finish(true);
    auto const otherTask = getTaskSnapshot().catalog.selectedTaskId;
    command(TaskAction::Select, {}, {}, originalTask);
    check(requested->request.origin == structure::detail::StructureLoadOrigin{200, 80, 300, 0},
          "task switching does not re-anchor the original task");
    finish(true);
    command(TaskAction::Select, {}, {}, otherTask);
    check(requested->request.origin == playerOrigin, "second task keeps its independent anchor");
    finish(true);
    command(TaskAction::Select, {}, {}, originalTask); finish(true);
    command(TaskAction::Activate);
    command(TaskAction::Close);
    auto closed = session.loaded();
    finish(true);
    check(!closed && !session.loaded() && getTaskStatus().activeTaskId.empty(),
          "late composition cannot reopen a closed task");
    command(TaskAction::Activate);
    session.clearLoaded({}); world = false;
    finish(true);
    check(!session.loaded() && getTaskStatus().activeTaskId.empty(),
          "world exit rejects pending first activation");
    auto expected = getTaskSnapshot().catalog;
    shutdownTasks();
    check(tasks::readCatalog(taskPath) == expected, "mod shutdown flushes accepted task edits");
    std::filesystem::remove(taskPath);
    std::filesystem::remove(directory);
    requested.reset(); completed.reset(); cache.clear(); inFlight = false;
    std::printf("TaskActionsTests: %d failures\n", failures);
    return failures;
}
