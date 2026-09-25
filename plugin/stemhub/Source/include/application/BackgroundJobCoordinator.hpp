#pragma once

#include <atomic>
#include <deque>
#include <functional>
#include <JuceHeader.h>
#include <mutex>

#include <cstdint>

template <typename Payload>
class BackgroundJobCoordinator
{
public:
    struct JobResult
    {
        uint64_t requestGeneration {};
        uint64_t requestId {};
        Payload payload;
    };

    // onResultReady is called on the worker thread each time a result is queued; it should
    // only schedule a flushResults() on the message thread.
    BackgroundJobCoordinator(size_t workerCount, std::function<void()> onResultReady)
        : resultReadyCallback(std::move(onResultReady)),
          backgroundJobs(juce::ThreadPoolOptions{}
                             .withThreadName("Stemhub jobs")
                             .withNumberOfThreads(static_cast<int>(workerCount)))
    {
    }

    ~BackgroundJobCoordinator()
    {
        shutdown();
    }

    // Stops accepting work, drops pending results and waits for the running jobs to return.
    // Owners call this before tearing down anything a job can reach, so no job outlives it.
    void shutdown()
    {
        isClosed = true;
        ++requestGeneration;

        // Running jobs are not interruptible yet; they end within their network timeouts.
        backgroundJobs.removeAllJobs(true, -1);

        const std::lock_guard<std::mutex> lock(resultMutex);
        pendingResults.clear();
    }

    void invalidateSession()
    {
        ++requestGeneration;

        const std::lock_guard<std::mutex> lock(resultMutex);
        pendingResults.clear();
    }

    uint64_t getCurrentGeneration() const noexcept
    {
        return requestGeneration.load();
    }

    void enqueue(std::function<Payload()> task)
    {
        if (isClosed)
            return;

        const auto requestId = ++requestCounter;
        const auto currentGeneration = requestGeneration.load();

        backgroundJobs.addJob([this,
                              requestId,
                              currentGeneration,
                              taskFn = std::move(task)]() mutable
        {
            if (currentGeneration != requestGeneration.load())
                return;

            auto payload = taskFn();

            if (currentGeneration != requestGeneration.load())
                return;

            JobResult result { currentGeneration, requestId, std::move(payload) };

            {
                const std::lock_guard<std::mutex> lock(resultMutex);
                pendingResults.push_back(std::move(result));
            }

            resultReadyCallback();
        });
    }

    template <typename ApplyResult>
    bool flushResults(ApplyResult&& applyResult)
    {
        std::deque<JobResult> results;

        {
            const std::lock_guard<std::mutex> lock(resultMutex);
            std::swap(results, pendingResults);
        }

        if (results.empty())
            return false;

        const auto activeGeneration = requestGeneration.load();
        bool didApply = false;

        while (!results.empty())
        {
            auto result = std::move(results.front());
            results.pop_front();

            if (result.requestGeneration != activeGeneration)
                continue;

            applyResult(std::move(result));
            didApply = true;
        }

        return didApply;
    }

private:
    std::function<void()> resultReadyCallback;
    std::deque<JobResult> pendingResults;
    mutable std::mutex resultMutex;
    std::atomic<bool> isClosed { false };
    std::atomic<uint64_t> requestGeneration { 0 };
    std::atomic<uint64_t> requestCounter { 0 };
    juce::ThreadPool backgroundJobs;
};
