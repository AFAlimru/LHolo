// LHolo - Client-side projection renderer for Minecraft Bedrock Windows
// Copyright (C) 2026  MarmieQi

#include "structure/StructurePaths.h"

#include <cstddef>
#include <string>

#include <Windows.h>

namespace lholo::structure::detail {

std::filesystem::path pathFromUtf8(std::string_view value) {
    if (value.empty()) return {};
    auto const wideSize = MultiByteToWideChar(
        CP_UTF8,
        MB_ERR_INVALID_CHARS,
        value.data(),
        static_cast<int>(value.size()),
        nullptr,
        0
    );
    if (wideSize <= 0) return std::filesystem::path{value};

    std::wstring wide(static_cast<std::size_t>(wideSize), L'\0');
    if (MultiByteToWideChar(
            CP_UTF8,
            MB_ERR_INVALID_CHARS,
            value.data(),
            static_cast<int>(value.size()),
            wide.data(),
            wideSize
        ) <= 0) {
        return std::filesystem::path{value};
    }
    return std::filesystem::path{wide};
}

std::string pathToUtf8(std::filesystem::path const& path) {
    auto const& wide = path.native();
    if (wide.empty()) return {};
    auto const size = WideCharToMultiByte(
        CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()), nullptr, 0, nullptr, nullptr
    );
    if (size <= 0) return path.string();
    std::string result(static_cast<std::size_t>(size), '\0');
    WideCharToMultiByte(
        CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()), result.data(), size, nullptr, nullptr
    );
    return result;
}

} // namespace lholo::structure::detail
