#pragma once
#include "tasks/TaskStore.h"
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <optional>
#include <thread>

namespace lholo::tasks {
// One writer owns the temporary file. Continuous edits replace the queued catalog.
class TaskStoreWriter {
public:
    explicit TaskStoreWriter(std::filesystem::path path);
    ~TaskStoreWriter();
    void queue(Catalog catalog);
    std::optional<std::string> takeResult();
    void finish();
private:
    void run();
    std::filesystem::path mPath;
    std::mutex mMutex;
    std::condition_variable mWake;
    std::optional<Catalog> mPending;
    std::optional<std::string> mResult;
    std::chrono::steady_clock::time_point mDeadline;
    bool mStopping{};
    std::thread mWorker;
};
}
