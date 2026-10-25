#include "ui/StructureFilePicker.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <mutex>
#include <thread>

namespace {
std::atomic_bool blockDialog{}, dialogEntered{};
std::mutex waitMutex;
std::condition_variable_any wake;
lholo::ui::StructureFileDialogResult nextResult;

lholo::ui::StructureFileDialogResult fakeDialog(std::filesystem::path const&, std::stop_token stop) {
    dialogEntered = true;
    if (blockDialog.load()) {
        std::unique_lock lock(waitMutex);
        wake.wait(lock, stop, [] { return !blockDialog.load(); });
    }
    return nextResult;
}

bool waitFor(auto predicate) {
    auto const deadline = std::chrono::steady_clock::now() + std::chrono::seconds{5};
    while (!predicate()) {
        if (std::chrono::steady_clock::now() >= deadline) return false;
        std::this_thread::sleep_for(std::chrono::milliseconds{1});
    }
    return true;
}
}

int runStructureFilePickerTests() {
    int failures{};
    auto check = [&](bool value, char const* message) {
        if (!value) { ++failures; std::fprintf(stderr, "StructureFilePickerTests: %s\n", message); }
    };
    lholo::ui::StructureFilePicker savePicker{fakeDialog, lholo::ui::StructureFileTypes::Mcstructure};
    nextResult = {std::filesystem::path{L"D:\\wrong.litematic"}, 0};
    check(savePicker.start({}), "save picker starts");
    check(waitFor([&] { return !savePicker.busy(); }), "save picker completes");
    auto saveResult = savePicker.takeResult();
    check(saveResult && !saveResult->path && saveResult->errorCode, "save mode rejects litematic output");
    nextResult = {std::filesystem::path{L"D:\\save.MCSTRUCTURE"}, 0};
    check(savePicker.start({}), "mcstructure save starts");
    check(waitFor([&] { return !savePicker.busy(); }), "mcstructure save completes");
    saveResult = savePicker.takeResult();
    check(saveResult && saveResult->path && !saveResult->errorCode, "save mode accepts mcstructure");
    lholo::ui::StructureFilePicker browser{fakeDialog};
    nextResult = {std::filesystem::path{L"D:\\蓝图 文件夹\\房屋.MCSTRUCTURE"}, 0};
    blockDialog = true;
    check(browser.start({}), "first request starts");
    check(!savePicker.start({}), "open/save instances share the modal-dialog gate");
    check(waitFor([] { return dialogEntered.load(); }), "dialog enters worker");
    check(browser.busy() && !browser.start({}), "duplicate request rejected");
    blockDialog = false;
    wake.notify_all();
    check(waitFor([&] { return !browser.busy(); }), "selection completes");
    auto result = browser.takeResult();
    check(result && result->path == nextResult.path && !result->errorCode, "Unicode path and upper-case extension retained");
    check(!browser.takeResult(), "completion consumed once");

    nextResult = {};
    check(browser.start({}), "cancel request starts");
    check(waitFor([&] { return !browser.busy(); }), "cancel completes");
    result = browser.takeResult();
    check(result && !result->path && !result->errorCode, "cancel is distinct from failure");

    nextResult = {std::filesystem::path{L"D:\\not-a-structure.txt"}, 0};
    check(browser.start({}), "unsupported selection starts");
    check(waitFor([&] { return !browser.busy(); }), "unsupported selection completes");
    result = browser.takeResult();
    check(result && !result->path && result->errorCode, "unsupported extension rejected");

    nextResult = {std::filesystem::path{L"D:\\late.litematic"}, 0};
    dialogEntered = false;
    blockDialog = true;
    check(browser.start({}), "cancellable request starts");
    check(waitFor([] { return dialogEntered.load(); }), "cancellable dialog enters");
    browser.cancel();
    browser.shutdown();
    check(!browser.busy() && !browser.takeResult(), "shutdown joins worker and discards late selection");
    blockDialog = false;
    check(browser.start({}), "browser reusable after cancellation");
    check(waitFor([&] { return !browser.busy(); }), "reopened selection completes");
    result = browser.takeResult();
    check(result && result->path == nextResult.path, "litematic selection returned");
    std::printf("StructureFilePickerTests: %d failures\n", failures);
    return failures;
}
