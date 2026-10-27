#include "tasks/TaskCatalog.h"
#include <algorithm>
#include <array>
#include <random>

namespace lholo::tasks {
std::string newId() {
    std::random_device random;
    std::array<unsigned char, 16> bytes;
    for (auto& b : bytes)
        b = static_cast<unsigned char>(random());
    bytes[6] = (bytes[6] & 15) | 64;
    bytes[8] = (bytes[8] & 63) | 128;
    constexpr char hex[] = "0123456789abcdef";
    std::string id;
    for (std::size_t i = 0; i < bytes.size(); ++i) {
        if (i == 4 || i == 6 || i == 8 || i == 10)
            id += '-';
        id += hex[bytes[i] >> 4];
        id += hex[bytes[i] & 15];
    }
    return id;
}
Task* findTask(Catalog& catalog, std::string const& id) {
    auto it =
        std::find_if(catalog.tasks.begin(), catalog.tasks.end(), [&](auto const& t) { return t.id == id; });
    return it == catalog.tasks.end() ? nullptr : &*it;
}
File* findFile(Task& task, std::string const& id) {
    auto it = std::find_if(task.files.begin(), task.files.end(), [&](auto const& f) { return f.id == id; });
    return it == task.files.end() ? nullptr : &*it;
}
bool applyCatalogEdit(Catalog& catalog, Intent const& i) {
    auto* task = findTask(catalog, i.taskId);
    auto* file = task ? findFile(*task, i.fileId) : nullptr;
    switch (i.action) {
    case Action::Create: {
        if (i.value.empty())
            return false;
        Task created{newId(), i.value, {}, {}};
        catalog.selectedTaskId = created.id;
        catalog.tasks.push_back(std::move(created));
        return true;
    }
    case Action::Select:
        if (!task)
            return false;
        catalog.selectedTaskId = task->id;
        return true;
    case Action::Rename:
        if (!task || i.value.empty())
            return false;
        task->name = i.value;
        return true;
    case Action::Delete:
        if (!task)
            return false;
        std::erase_if(catalog.tasks, [&](auto const& t) { return t.id == i.taskId; });
        if (catalog.selectedTaskId == i.taskId)
            catalog.selectedTaskId = catalog.tasks.empty() ? "" : catalog.tasks.front().id;
        return true;
    case Action::AddFile:
        if (!task || i.value.empty())
            return false;
        task->files.push_back({newId(), i.value});
        task->selectedFileId = task->files.back().id;
        return true;
    case Action::Path:
        if (!file)
            return false;
        file->path = i.value;
        return true;
    case Action::Visible:
        if (!file)
            return false;
        file->visible = !file->visible;
        return true;
    case Action::Move: {
        if (!file || (i.delta != -1 && i.delta != 1))
            return false;
        auto a = file - task->files.data(), b = a + i.delta;
        if (b < 0 || b >= static_cast<std::ptrdiff_t>(task->files.size()))
            return false;
        std::swap(task->files[a], task->files[b]);
        return true;
    }
    case Action::RemoveFile:
        if (!file)
            return false;
        std::erase_if(task->files, [&](auto const& f) { return f.id == i.fileId; });
        if (task->selectedFileId == i.fileId)
            task->selectedFileId = task->files.empty() ? "" : task->files.front().id;
        return true;
    case Action::SelectFile:
        if (!file)
            return false;
        task->selectedFileId = file->id;
        return true;
    case Action::Activate:
    case Action::Close:
    case Action::Acknowledge:
        return false;
    }
    return false;
}
} // namespace lholo::tasks
