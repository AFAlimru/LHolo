// LHolo - Client-side projection renderer for Minecraft Bedrock Windows
// Copyright (C) 2026  MarmieQi
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#include "structure/java_to_bedrock/JavaBlockEntityToBedrock.h"

#include <nlohmann/json.hpp>

namespace lholo::structure::detail {
namespace {

void appendJavaText(nlohmann::json const& value, std::string& output) {
    if (value.is_string()) {
        output += value.get_ref<std::string const&>();
        return;
    }
    if (value.is_array()) {
        for (auto const& child : value) appendJavaText(child, output);
        return;
    }
    if (!value.is_object()) return;

    if (auto const text = value.find("text"); text != value.end() && text->is_string()) {
        output += text->get_ref<std::string const&>();
    } else if (auto const fallback = value.find("fallback");
               fallback != value.end() && fallback->is_string()) {
        output += fallback->get_ref<std::string const&>();
    } else if (auto const translate = value.find("translate");
               translate != value.end() && translate->is_string()) {
        // Client language tables are not available in this format converter.
        // Keeping the translation key is deterministic and preferable to
        // silently dropping the component.
        output += translate->get_ref<std::string const&>();
    }

    if (auto const extra = value.find("extra"); extra != value.end()) appendJavaText(*extra, output);
}

} // namespace

std::string javaTextComponentToPlainText(std::string const& component) {
    if (component.empty()) return {};
    auto const parsed = nlohmann::json::parse(component, nullptr, false, true);
    if (parsed.is_discarded()) return component;
    std::string result;
    appendJavaText(parsed, result);
    return result;
}

} // namespace lholo::structure::detail
