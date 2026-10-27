#include "oreui/TaskProtocol.h"
#include "nlohmann/json.hpp"
#include <map>
namespace lholo::oreui {
std::optional<app::TaskIntent> parseTaskIntent(std::string_view payload) {
    if (payload.size() > kTaskIntentMaxBytes) return {};
    try {
        auto j = nlohmann::json::parse(payload);
        static std::map<std::string, app::TaskAction> const actions{
            {"create", app::TaskAction::Create},         {"select", app::TaskAction::Select},
            {"rename", app::TaskAction::Rename},         {"delete", app::TaskAction::Delete},
            {"addFile", app::TaskAction::AddFile},       {"path", app::TaskAction::Path},
            {"visible", app::TaskAction::Visible},       {"move", app::TaskAction::Move},
            {"removeFile", app::TaskAction::RemoveFile}, {"selectFile", app::TaskAction::SelectFile},
            {"activate", app::TaskAction::Activate},     {"close", app::TaskAction::Close}};
        auto action = actions.find(j.at("action").get<std::string>());
        if (action == actions.end())
            return {};
        if (!j.at("id").is_number_unsigned() || !j.at("revision").is_number_unsigned())
            return {};
        app::TaskIntent i{j.at("id"),
                          j.at("revision"),
                          action->second,
                          j.value("taskId", ""),
                          j.value("fileId", ""),
                          j.value("value", ""),
                          j.value("delta", 0)};
        if (!i.id || i.id > 9007199254740991ULL || i.revision > 9007199254740991ULL || i.value.size() > kTaskValueMaxBytes)
            return {};
        return i;
    } catch (nlohmann::json::exception const&) {
        return {};
    }
}
} // namespace lholo::oreui
