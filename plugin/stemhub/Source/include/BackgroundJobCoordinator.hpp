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

    explicit BackgroundJobCoordinator(size_t workerCount)
        : backgroundJobs(static_cast<int>(workerCount))
    {
    }

    ~BackgroundJobCoordinator()
    {
        backgroundJobs.removeAllJobs(true, 2000);
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

    void enqueue(std::function<Payload()> task, std::function<void()> completionCallback)
    {
        const auto requestId = ++requestCounter;
        const auto currentGeneration = requestGeneration.load();

        backgroundJobs.addJob([this,
                              requestId,
                              currentGeneration,
                              taskFn = std::move(task),
                              completionFn = std::move(completionCallback)]() mutable
        {
            if (currentGeneration != requestGeneration.load())
                return;

            JobResult result { currentGeneration, requestId, taskFn() };

            {
                const std::lock_guard<std::mutex> lock(resultMutex);
                pendingResults.push_back(std::move(result));
            }

            completionFn();
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
    std::deque<JobResult> pendingResults;
    mutable std::mutex resultMutex;
    std::atomic<uint64_t> requestGeneration { 0 };
    std::atomic<uint64_t> requestCounter { 0 };
    juce::ThreadPool backgroundJobs;
};
