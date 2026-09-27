#pragma once

#include <JuceHeader.h>

#include "network/ApiTypes.hpp"

namespace stemhub::test
{
// How long a test waits for something before it fails. Waits end as soon as their condition
// holds, so this only bounds how long a failing test takes.
constexpr int kWaitTimeoutMs = 5000;

// Blocks a fake API call until the test releases it; once released, it stays open for later
// calls. A gate that honours cancels also lets go when the job running the call is asked to
// stop, as a real transfer does.
//
// A gate that is never released lets go after kFallbackMs, so a forgotten release can't hang the
// run. That is longer than any wait: the test waiting on the blocked call fails before it.
class BlockingGate
{
public:
    static constexpr int kFallbackMs = 2 * kWaitTimeoutMs;

    explicit BlockingGate(bool honoursCancelToUse = false)
        : honoursCancel(honoursCancelToUse)
    {
    }

    // False when no call reached the gate in time.
    [[nodiscard]] bool waitUntilEntered()
    {
        return entered.wait(kWaitTimeoutMs);
    }

    void release()
    {
        allowContinue.signal();
    }

    // False when the call that entered hasn't gone through in time.
    [[nodiscard]] bool waitUntilFinished()
    {
        return finished.wait(kWaitTimeoutMs);
    }

    // Called by the fake API, on the worker thread making the call.
    void block()
    {
        entered.signal();

        const auto giveUpAt = juce::Time::getMillisecondCounter() + static_cast<juce::uint32>(kFallbackMs);
        while (!allowContinue.wait(10) && juce::Time::getMillisecondCounter() < giveUpAt)
            if (honoursCancel && isJobCancelled())
                break;

        finished.signal();
    }

private:
    const bool honoursCancel;
    juce::WaitableEvent entered;
    juce::WaitableEvent allowContinue { true }; // manual reset: release() opens it for good
    juce::WaitableEvent finished;
};
}
