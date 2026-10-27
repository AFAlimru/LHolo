#include "tasks/TaskStore.h"
#include "nlohmann/json.hpp"
#include <Windows.h>
#include <fstream>
#include <set>
#include <stdexcept>

namespace lholo::tasks {
namespace {
using nlohmann::json;
constexpr char const* axes[]{"y", "x", "material"};
constexpr char const* modes[]{"all", "single", "upToCurrent", "fromCurrent"};
template <std::size_t N> int decode(std::string const& value, char const* const (&names)[N]) {
    for (std::size_t i = 0; i < N; ++i)
        if (value == names[i])
            return static_cast<int>(i);
    throw std::runtime_error("Unknown task slice value: " + value);
}
} // namespace
Catalog readCatalog(std::filesystem::path const& path) {
    if (!std::filesystem::exists(path))
        return {};
    std::ifstream stream(path);
    auto data = json::parse(stream);
    if (data.at("version").get<int>() != 1)
        throw std::runtime_error("Unsupported tasks version");
    Catalog catalog{data.at("selectedTaskId").get<std::string>(), {}};
    std::set<std::string> ids;
    auto acceptId = [&](std::string const& id) {
        if (id.empty() || !ids.insert(id).second)
            throw std::runtime_error("Duplicate or empty task/file ID");
    };
    for (auto const& item : data.at("tasks")) {
        Task task{item.at("id"), item.at("name"), item.at("selectedFileId"), {}};
        acceptId(task.id);
        for (auto const& entry : item.at("files")) {
            auto const& offset = entry.at("offset");
            auto const& slice = entry.at("slice");
            File file;
            file.id = entry.at("id");
            file.path = entry.at("path");
            file.visible = entry.at("visible");
            file.x = offset.at("x");
            file.y = offset.at("y");
            file.z = offset.at("z");
            file.rotation = entry.at("rotation");
            file.mirror = entry.at("mirror");
            if (file.rotation != 0 && file.rotation != 90 && file.rotation != 180 && file.rotation != 270)
                file.rotation = 0;
            if (file.mirror != "none" && file.mirror != "x" && file.mirror != "z")
                file.mirror = "none";
            file.slice = {structure::layerAxisFromInt(decode(slice.at("axis"), axes)),
                          structure::layerDisplayModeFromInt(decode(slice.at("mode"), modes)),
                          slice.at("index").get<int>()};
            if (file.slice.layer < 0)
                throw std::runtime_error("Negative task slice index");
            acceptId(file.id);
            task.files.push_back(std::move(file));
        }
        if (!task.selectedFileId.empty() && !findFile(task, task.selectedFileId))
            throw std::runtime_error("Unknown selected file");
        catalog.tasks.push_back(std::move(task));
    }
    if (!catalog.selectedTaskId.empty() && !findTask(catalog, catalog.selectedTaskId))
        throw std::runtime_error("Unknown selected task");
    return catalog;
}
void writeCatalog(std::filesystem::path const& path, Catalog const& catalog) {
    json data{{"version", 1}, {"selectedTaskId", catalog.selectedTaskId}, {"tasks", json::array()}};
    for (auto const& task : catalog.tasks) {
        json item{{"id", task.id},
                  {"name", task.name},
                  {"selectedFileId", task.selectedFileId},
                  {"files", json::array()}};
        for (auto const& file : task.files)
            item["files"].push_back({{"id", file.id},
                                     {"path", file.path},
                                     {"visible", file.visible},
                                     {"offset", {{"x", file.x}, {"y", file.y}, {"z", file.z}}},
                                     {"rotation", file.rotation},
                                     {"mirror", file.mirror},
                                     {"slice",
                                      {{"axis", axes[structure::toInt(file.slice.axis)]},
                                       {"mode", modes[structure::toInt(file.slice.mode)]},
                                       {"index", file.slice.layer}}}});
        data["tasks"].push_back(std::move(item));
    }
    std::filesystem::create_directories(path.parent_path());
    auto temporary = path;
    temporary += L".tmp";
    {
        std::ofstream stream(temporary, std::ios::binary | std::ios::trunc);
        stream.exceptions(std::ios::failbit | std::ios::badbit);
        stream << data.dump(2) << '\n';
        stream.close();
    }
    if (!MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
        throw std::system_error(static_cast<int>(GetLastError()), std::system_category(), "Saving tasks");
}
} // namespace lholo::tasks
