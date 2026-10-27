#include "app/TaskActions.h"
#include "app/TaskContext.h"
#include "tasks/TaskStore.h"
#include "tasks/TaskStoreWriter.h"
#include "structure/StructureFileLoader.h"
#include "structure/StructureSession.h"
#include "structure/MaterialTracker.h"
#include <algorithm>
#include <deque>
#include <mutex>

namespace lholo::app {
namespace {
std::mutex mutex;
TaskSnapshot state;
std::deque<TaskIntent> pending;
std::optional<structure::detail::StructureLoadOrigin> anchor;
std::optional<structure::detail::StructureLoadOrigin> activeAnchor;
std::uint64_t activeGeneration{};
bool dirty{}, loading{};
std::optional<tasks::Task> activeDefinition, pendingDefinition;
bool reloadFiles{};
std::string storageError;
std::unique_ptr<tasks::TaskStoreWriter> writer;
auto& session() {
    return structure::detail::StructureSession::getInstance();
}
void save() {
    writer->queue(state.catalog);
}
structure::detail::StructureTransformSnapshot transform(tasks::File const& f) {
    return {f.rotation / 90,
            f.mirror == "x" ? 1 : f.mirror == "z" ? 2 : 0,
            f.x,
            f.y,
            f.z,
            f.slice.mode,
            f.slice.layer,
            f.slice.axis};
}
void selectEditor(tasks::Task& task) {
    if (!activeDefinition) return;
    structure::detail::invalidateMaterialList();
    activeDefinition->selectedFileId = task.selectedFileId;
    auto* file = tasks::findFile(*activeDefinition, task.selectedFileId);
    if (!file) {
        session().selectCompositionFile({}, {}, -2, {});
        return;
    }
    auto loaded = structure::detail::cachedTaskFile(file->path);
    auto projection = session().loaded();
    auto index = projection ? static_cast<int>(std::find(projection->sourceFiles.begin(),
                                                         projection->sourceFiles.end(), file->id) -
                                               projection->sourceFiles.begin())
                            : -2;
    if (projection && index == static_cast<int>(projection->sourceFiles.size()))
        index = -2;
    session().selectCompositionFile(loaded, transform(*file), index, file->path);
}
void clearRuntime() {
    state.activeTaskId.clear();
    anchor.reset();
    activeAnchor.reset();
    activeGeneration = 0;
    dirty = false;
    activeDefinition.reset();
    pendingDefinition.reset();
    reloadFiles = false;
}
bool activate(tasks::Task const* task) {
    auto origin = detail::taskPlayerOrigin();
    if (!task || !origin)
        return false;
    if (task->id != state.activeTaskId || !anchor)
        anchor = origin;
    pendingDefinition = *task;
    dirty = true;
    reloadFiles = true;
    return true;
}
bool mutate(TaskIntent const& i) {
    auto* task = tasks::findTask(state.catalog, i.taskId);
    if (i.action == TaskAction::Activate)
        return activate(task);
    if (i.action == TaskAction::Close) {
        session().cancelPendingLoad();
        structure::clear();
        clearRuntime();
        return true;
    }
    if (!tasks::applyCatalogEdit(state.catalog, i))
        return false;
    if (i.action == TaskAction::Create || i.action == TaskAction::Select) {
        auto* selected = tasks::findTask(state.catalog, state.catalog.selectedTaskId);
        if (detail::taskPlayerOrigin())
            activate(selected);
        return true;
    }
    if (i.action == TaskAction::Delete && i.taskId == state.activeTaskId) {
        structure::clear();
        clearRuntime();
        return true;
    }
    task = tasks::findTask(state.catalog, i.taskId);
    if (task && task->id == state.activeTaskId) {
        if (i.action == TaskAction::SelectFile)
            selectEditor(*task);
        else if (i.action != TaskAction::Rename && i.action != TaskAction::Select) {
            pendingDefinition = *task;
            dirty = true;
        }
    }
    return true;
}
} // namespace
void loadTasks() {
    std::lock_guard lock(mutex);
    state.writable = false;
    try {
        auto path = detail::taskStorePath();
        state.catalog = tasks::readCatalog(path);
        writer = std::make_unique<tasks::TaskStoreWriter>(std::move(path));
        state.error.clear();
        state.writable = true;
    } catch (std::exception const& e) {
        state.error = e.what();
    }
    ++state.revision;
}
bool submitTaskIntent(TaskIntent i) {
    std::lock_guard lock(mutex);
    if (!i.id || i.id <= state.ack || !pending.empty())
        return false;
    pending.push_back(std::move(i));
    return true;
}
TaskSnapshot getTaskSnapshot() {
    std::lock_guard lock(mutex);
    auto copy = state;
    if (!storageError.empty()) {
        copy.error = storageError;
        copy.errorFileId.clear();
    }
    copy.loading = loading || dirty || !pending.empty() || structure::detail::isStructureFileLoading();
    copy.worldAvailable = detail::taskPlayerOrigin().has_value();
    return copy;
}
TaskStatus getTaskStatus() {
    std::lock_guard lock(mutex);
    auto status = static_cast<TaskStatus const&>(state);
    if (!storageError.empty()) {
        status.error = storageError;
        status.errorFileId.clear();
    }
    status.loading = loading || dirty || !pending.empty() || structure::detail::isStructureFileLoading();
    status.worldAvailable = detail::taskPlayerOrigin().has_value();
    return status;
}
void shutdownTasks() {
    std::lock_guard lock(mutex);
    pending.clear();
    if (writer) {
        writer->finish();
        if (auto result = writer->takeResult()) storageError = std::move(*result);
        writer.reset();
    }
    loading = false;
    state.writable = false;
    clearRuntime();
}
void processTaskActions() {
    std::lock_guard lock(mutex);
    auto current = session().loaded();
    if (writer)
        if (auto result = writer->takeResult()) storageError = std::move(*result);
    if (!current && !detail::taskPlayerOrigin())
        clearRuntime();
    if (!state.activeTaskId.empty() &&
        (!current || !current->composed || current->generation != activeGeneration))
        clearRuntime();
    if (auto result = structure::detail::takeTaskCompositionResult()) {
        loading = false;
        if (result->revision == state.revision && pendingDefinition && result->taskId == pendingDefinition->id) {
            if (!result->error.empty()) {
                state.error = result->error;
                state.errorFileId = result->errorFileId;
                session().failFileLoad(result->request,
                                       i18n::Message{i18n::TextKey::StatusLoadFailed, {result->error}});
                anchor = activeAnchor;
                if (auto* task = tasks::findTask(state.catalog, state.activeTaskId))
                    selectEditor(*task);
            } else if (anchor) {
                auto origin = *anchor;
                origin.x += result->composition.origin[0];
                origin.y += result->composition.origin[1];
                origin.z += result->composition.origin[2];
                auto generation = result->composition.loaded->generation;
                if (session().completeCompositionLoad(result->request, std::move(result->composition.loaded),
                                                      origin)) {
                    state.activeTaskId = result->taskId;
                    activeGeneration = generation;
                    activeAnchor = anchor;
                    activeDefinition = std::move(pendingDefinition);
                    auto* task = tasks::findTask(state.catalog, state.activeTaskId);
                    if (task && task->files == activeDefinition->files) {
                        state.error.clear();
                        state.errorFileId.clear();
                    }
                    if (task)
                        selectEditor(*task);
                    structure::detail::invalidateMaterialList();
                }
            }
        }
        pendingDefinition.reset();
    } else if (loading && !structure::detail::isStructureFileLoading())
        loading = false;
    // Existing editors/hotkeys own their live tuple. Persist it into the selected file, then recompose.
    if (!loading && !dirty && activeDefinition) {
        auto* task = tasks::findTask(state.catalog, state.activeTaskId);
        auto* file = tasks::findFile(*activeDefinition, activeDefinition->selectedFileId);
        if (file && session().selectedSource() >= 0) {
            auto t = session().transform();
            bool changed = file->x != t.offsetX || file->y != t.offsetY || file->z != t.offsetZ ||
                file->rotation != t.rotation * 90 ||
                file->mirror != (t.mirror == 1 ? "x" : t.mirror == 2 ? "z" : "none") ||
                file->slice != structure::LayerSettings{t.layerAxis, t.layerDisplayMode, t.displayLayer};
            if (changed) {
                pendingDefinition = *activeDefinition;
                file = tasks::findFile(*pendingDefinition, file->id);
                file->x = t.offsetX;
                file->y = t.offsetY;
                file->z = t.offsetZ;
                file->rotation = t.rotation * 90;
                file->mirror = t.mirror == 1 ? "x" : t.mirror == 2 ? "z" : "none";
                file->slice = {t.layerAxis, t.layerDisplayMode, t.displayLayer};
                if (auto* saved = task ? tasks::findFile(*task, file->id) : nullptr) {
                    saved->x = file->x; saved->y = file->y; saved->z = file->z;
                    saved->rotation = file->rotation; saved->mirror = file->mirror; saved->slice = file->slice;
                }
                ++state.revision;
                dirty = true;
                save();
            }
        }
    }
    if (!pending.empty()) {
        auto i = std::move(pending.front());
        pending.pop_front();
        state.ack = i.id;
        state.accepted = false;
        if (i.action == TaskAction::Acknowledge) {
            state.accepted = true;
            if (!i.value.empty()) {
                state.error = i.value;
                state.errorFileId = i.fileId;
            }
        } else if (state.writable && i.revision == state.revision &&
                   (!loading || i.action == TaskAction::Close)) {
            state.accepted = mutate(i);
            if (state.accepted) {
                ++state.revision;
                state.error.clear();
                state.errorFileId.clear();
                if (i.action != TaskAction::Activate && i.action != TaskAction::Close)
                    save();
            }
        }
    }
    if (!dirty || loading || !anchor || !detail::taskPlayerOrigin())
        return;
    auto const& task = *pendingDefinition;
    std::vector<structure::CompositionFile> files;
    std::vector<std::string> paths;
    for (auto const& file : task.files) {
        files.push_back({file.id,
                         {},
                         file.visible,
                         file.x,
                         file.y,
                         file.z,
                         file.rotation / 90,
                         file.mirror == "x"   ? 1
                         : file.mirror == "z" ? 2
                                              : 0,
                         file.slice});
        paths.push_back(file.path);
    }
    if (structure::detail::startTaskComposition(task.id, state.revision, *anchor, std::move(files),
                                                std::move(paths), reloadFiles)) {
        loading = true;
        dirty = false;
        reloadFiles = false;
    }
}
} // namespace lholo::app
