#include "ui/FileDialog.h"

#include <algorithm>
#include <iterator>
#include <vector>

#include <Windows.h>
#include <commdlg.h>

namespace lholo::ui {

std::optional<std::filesystem::path> openStructureFile(std::filesystem::path const& current) {
    std::vector<wchar_t> buffer(32768, L'\0');
    if (!current.empty()) {
        auto const value = current.native();
        std::copy_n(value.data(), std::min(value.size(), buffer.size() - 1), buffer.data());
    }

    wchar_t const filter[] =
        L"投影结构 (*.mcstructure;*.litematic)\0*.mcstructure;*.litematic\0"
        L"Bedrock 结构 (*.mcstructure)\0*.mcstructure\0"
        L"Litematica 结构 (*.litematic)\0*.litematic\0"
        L"所有文件 (*.*)\0*.*\0\0";
    OPENFILENAMEW dialog{};
    dialog.lStructSize = sizeof(dialog);
    dialog.lpstrFile = buffer.data();
    dialog.nMaxFile = static_cast<DWORD>(buffer.size());
    dialog.lpstrFilter = filter;
    dialog.nFilterIndex = 1;
    dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR | OFN_EXPLORER;
    dialog.lpstrDefExt = L"mcstructure";
    if (!GetOpenFileNameW(&dialog)) return std::nullopt;
    return std::filesystem::path{buffer.data()};
}

std::optional<std::filesystem::path> saveMcstructureFile() {
    std::vector<wchar_t> buffer(32768, L'\0');
    constexpr wchar_t defaultName[] = L"structure.mcstructure";
    std::copy_n(defaultName, std::size(defaultName), buffer.data());

    wchar_t const filter[] =
        L"Bedrock 结构 (*.mcstructure)\0*.mcstructure\0"
        L"所有文件 (*.*)\0*.*\0\0";
    OPENFILENAMEW dialog{};
    dialog.lStructSize = sizeof(dialog);
    dialog.lpstrFile = buffer.data();
    dialog.nMaxFile = static_cast<DWORD>(buffer.size());
    dialog.lpstrFilter = filter;
    dialog.nFilterIndex = 1;
    dialog.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR | OFN_EXPLORER;
    dialog.lpstrDefExt = L"mcstructure";
    if (!GetSaveFileNameW(&dialog)) return std::nullopt;
    return std::filesystem::path{buffer.data()};
}

} // namespace lholo::ui
