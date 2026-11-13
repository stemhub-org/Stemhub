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
    // Hands the owner an extra result while the job runs, such as a progress report.
    using Post = std::function<void(Result)>;

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

    // Stops accepting work, asks the running jobs to stop, waits for them and drops their
    // results. Owners call this before tearing down anything a job can reach, so no job
    // outlives it. Jobs stop at their next check of isJobCancelled(), or when a request ends.
    void shutdown()
    {
        isClosed = true;
        pool.removeAllJobs(true, -1);

        const std::lock_guard<std::mutex> lock(resultMutex);
        pendingResults.clear();
    }

    // Asks the running jobs to stop, without waiting; each still hands back a result. Jobs
    // waiting for a thread are left alone.
    void stopRunningJobs()
    {
        RunningJobs running;
        pool.removeAllJobs(true, 0, &running);
    }

    void enqueue(std::function<Result(const Post& post)> task)
    {
        if (isClosed)
            return;

        pool.addJob([this, job = std::move(task)]
        {
            const Post post = [this](Result result) { queue(std::move(result)); };
            queue(job(post));
        });
    }

    // The results queued since the last call, oldest first.
    std::vector<Result> takeResults()
    {
        const std::lock_guard<std::mutex> lock(resultMutex);
        return std::exchange(pendingResults, {});
    }

private:
    struct RunningJobs final : juce::ThreadPool::JobSelector
    {
        bool isJobSuitable(juce::ThreadPoolJob* job) override { return job->isRunning(); }
    };

    void queue(Result result)
    {
        {
            const std::lock_guard<std::mutex> lock(resultMutex);
            if (isClosed)
                return;

            pendingResults.push_back(std::move(result));
        }

        resultReadyCallback();
    }

    std::function<void()> resultReadyCallback;
    std::mutex resultMutex;
    std::vector<Result> pendingResults;
    std::atomic<bool> isClosed { false };
    juce::ThreadPool pool;
};
