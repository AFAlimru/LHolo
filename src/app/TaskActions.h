#pragma once
#include "tasks/TaskCatalog.h"
#include <cstdint>
namespace lholo::app {
using TaskAction = tasks::Action;
using TaskIntent = tasks::Intent;
struct TaskStatus {
    std::uint64_t revision{}, ack{};
    bool accepted{}, loading{}, worldAvailable{}, writable{};
    std::string activeTaskId, error, errorFileId;
    bool operator==(TaskStatus const&) const = default;
};
struct TaskSnapshot : TaskStatus {
    tasks::Catalog catalog;
};
void loadTasks();
void shutdownTasks();
bool submitTaskIntent(TaskIntent);
TaskSnapshot getTaskSnapshot();
TaskStatus getTaskStatus();
void processTaskActions();
} // namespace lholo::app
