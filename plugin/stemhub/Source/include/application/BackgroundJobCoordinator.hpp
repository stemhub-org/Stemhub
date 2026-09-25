#pragma once

#include <atomic>
#include <functional>
#include <mutex>
#include <utility>
#include <vector>

#include <JuceHeader.h>

// Runs jobs on a small thread pool and hands their results to the message thread. Which results
// still matter is the owner's call (StemhubSession tags each one with its request epoch); this
// class only moves them between threads and makes sure no job outlives it.
template <typename Result>
class BackgroundJobCoordinator
{
public:
    // onResultReady is called on the worker thread each time a result is queued; it should
    // only schedule a takeResults() on the message thread.
    BackgroundJobCoordinator(int workerCount, std::function<void()> onResultReady)
        : resultReadyCallback(std::move(onResultReady)),
          pool(juce::ThreadPoolOptions {}
                   .withThreadName("Stemhub jobs")
                   .withNumberOfThreads(workerCount))
    {
    }

    ~BackgroundJobCoordinator()
    {
        shutdown();
    }

    // Stops accepting work, waits for the running jobs to return and drops their results.
    // Owners call this before tearing down anything a job can reach, so no job outlives it.
    void shutdown()
    {
        isClosed = true;

        // Running jobs are not interruptible yet; they end within their network timeouts.
        pool.removeAllJobs(true, -1);

        const std::lock_guard<std::mutex> lock(resultMutex);
        pendingResults.clear();
    }

    void enqueue(std::function<Result()> task)
    {
        if (isClosed)
            return;

        pool.addJob([this, task = std::move(task)]
        {
            auto result = task();

            {
                const std::lock_guard<std::mutex> lock(resultMutex);
                if (isClosed)
                    return;

                pendingResults.push_back(std::move(result));
            }

            resultReadyCallback();
        });
    }

    // The results queued since the last call, oldest first.
    std::vector<Result> takeResults()
    {
        const std::lock_guard<std::mutex> lock(resultMutex);
        return std::exchange(pendingResults, {});
    }

private:
    std::function<void()> resultReadyCallback;
    std::mutex resultMutex;
    std::vector<Result> pendingResults;
    std::atomic<bool> isClosed { false };
    juce::ThreadPool pool;
};
