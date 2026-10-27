#pragma once
#include "structure/LayerSettings.h"
#include <string>
#include <vector>
#include <optional>

namespace lholo::tasks {
struct File {
    std::string id, path;
    bool visible{true};
    int x{}, y{}, z{}, rotation{};
    std::string mirror{"none"};
    structure::LayerSettings slice{};
    // Bound files use absolute world coordinates. Missing dimension marks a
    // new file awaiting its first successful placement at the player's feet.
    std::optional<int> dimension;
    bool operator==(File const&) const = default;
};
struct Task {
    std::string id, name, selectedFileId;
    std::vector<File> files;
    bool operator==(Task const&) const = default;
};
struct Catalog {
    std::string selectedTaskId;
    std::vector<Task> tasks;
    bool operator==(Catalog const&) const = default;
};
std::string newId();
Task* findTask(Catalog&, std::string const& id);
File* findFile(Task&, std::string const& id);
} // namespace lholo::tasks
