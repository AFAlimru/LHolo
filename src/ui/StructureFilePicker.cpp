#include "ui/StructureFilePicker.h"

#include <algorithm>
#include <atomic>
#include <exception>
#include <utility>

#include <Windows.h>

namespace lholo::ui {
namespace {
// All LHolo async open/save pickers share the native modal-dialog gate.
std::atomic_bool dialogInUse{};
}

StructureFilePicker::~StructureFilePicker() { shutdown(); }

bool StructureFilePicker::start(std::filesystem::path current) {
    std::lock_guard lock(mMutex);
    if (mBusy) return false;
    // The previous worker sets mBusy=false as its final operation under the
    // mutex, so joining it here cannot wait for another lock acquisition.
    if (mWorker.joinable()) mWorker.join();
    bool expected{};
    if (!dialogInUse.compare_exchange_strong(expected, true)) return false;
    mResult.reset();
    mBusy = true;
    try {
        mWorker = std::jthread([this, current = std::move(current)](std::stop_token stop) {
            StructureFileDialogResult result;
            try {
                result = mDialog(current, stop);
                if (result.path) {
                    auto extension = result.path->extension().native();
                    std::transform(extension.begin(), extension.end(), extension.begin(), [](wchar_t c) {
                        return c >= L'A' && c <= L'Z' ? static_cast<wchar_t>(c + L'a' - L'A') : c;
                    });
                    if (extension != L".mcstructure" && (mTypes != StructureFileTypes::Projection || extension != L".litematic")) {
                        result = {std::nullopt, ERROR_INVALID_DATA};
                    }
                }
            } catch (std::exception const&) {
                result = {std::nullopt, ERROR_GEN_FAILURE};
            }
            std::lock_guard resultLock(mMutex);
            if (!stop.stop_requested()) mResult = std::move(result);
            mBusy = false;
            dialogInUse = false;
        });
    } catch (...) {
        dialogInUse = false;
        mBusy = false;
        throw;
    }
    return true;
}

bool StructureFilePicker::busy() const {
    std::lock_guard lock(mMutex);
    return mBusy;
}

std::optional<StructureFileDialogResult> StructureFilePicker::takeResult() {
    std::lock_guard lock(mMutex);
    return std::exchange(mResult, std::nullopt);
}

void StructureFilePicker::cancel() {
    std::lock_guard lock(mMutex);
    mWorker.request_stop();
    mResult.reset();
}

void StructureFilePicker::shutdown() {
    cancel();
    // Never join while holding the result mutex.
    if (mWorker.joinable()) {
        // Closing a modal dialog can synchronously notify its owner window.
        // If shutdown runs on that owner's thread, service sent messages while
        // waiting; queued user input is deliberately left alone.
        HANDLE const thread = mWorker.native_handle();
        while (MsgWaitForMultipleObjectsEx(1, &thread, INFINITE, QS_SENDMESSAGE, MWMO_INPUTAVAILABLE)
               == WAIT_OBJECT_0 + 1) {
            MSG message{};
            PeekMessageW(&message, nullptr, WM_NULL, WM_NULL, PM_NOREMOVE);
        }
        mWorker.join();
    }
}
}
