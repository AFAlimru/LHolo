#include "tasks/TaskStoreWriter.h"
#include <utility>

namespace lholo::tasks {
TaskStoreWriter::TaskStoreWriter(std::filesystem::path path)
    : mPath(std::move(path)), mWorker([this] { run(); }) {}
TaskStoreWriter::~TaskStoreWriter() { finish(); }
void TaskStoreWriter::queue(Catalog catalog) {
    std::lock_guard lock(mMutex);
    mPending = std::move(catalog);
    mDeadline = std::chrono::steady_clock::now() + std::chrono::milliseconds{300};
    mWake.notify_one();
}
std::optional<std::string> TaskStoreWriter::takeResult() {
    std::lock_guard lock(mMutex);
    return std::exchange(mResult, std::nullopt);
}
void TaskStoreWriter::finish() {
    {
        std::lock_guard lock(mMutex);
        mStopping = true;
        mWake.notify_one();
    }
    if (mWorker.joinable()) mWorker.join();
}
void TaskStoreWriter::run() {
    std::unique_lock lock(mMutex);
    for (;;) {
        mWake.wait(lock, [&] { return mStopping || mPending.has_value(); });
        if (!mPending) return;
        while (!mStopping && std::chrono::steady_clock::now() < mDeadline) {
            auto deadline = mDeadline;
            mWake.wait_until(lock, deadline);
        }
        auto catalog = std::move(*mPending);
        mPending.reset();
        lock.unlock();
        std::string error;
        try { writeCatalog(mPath, catalog); }
        catch (std::exception const& e) { error = e.what(); }
        lock.lock();
        mResult = std::move(error);
    }
}
}
