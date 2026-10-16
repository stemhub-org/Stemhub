#include <csignal>
#include <iostream>

#include <JuceHeader.h>

// The suites register themselves: see *Tests.cpp.
namespace
{
// JUCE's default logger writes to the debugger on Windows; CI reads the console.
class ConsoleLogger final : public juce::Logger
{
    void logMessage(const juce::String& message) override
    {
        std::cout << message << std::endl;
    }
};
}

int main()
{
   #if ! JUCE_WINDOWS
    // The local test server writes with send(); a client hanging up must not kill the run.
    std::signal(SIGPIPE, SIG_IGN);
   #endif

    ConsoleLogger consoleLogger;
    juce::Logger::setCurrentLogger(&consoleLogger);
    const juce::ScopeGuard restoreLogger { [] { juce::Logger::setCurrentLogger(nullptr); } };

    juce::ScopedJuceInitialiser_GUI scopedJuce;
    juce::UnitTestRunner runner;
    runner.setAssertOnFailure(false);
    runner.runAllTests();
    int passCount = 0;
    int failureCount = 0;
    for (int i = 0; i < runner.getNumResults(); ++i)
    {
        if (auto* result = runner.getResult(i); result != nullptr)
        {
            passCount += result->passes;
            failureCount += result->failures;
            for (const auto& message : result->messages)
                juce::Logger::writeToLog(message);
        }
    }

    juce::Logger::writeToLog(juce::String(passCount) + " checks passed, " + juce::String(failureCount) + " failed.");
    return failureCount == 0 ? 0 : 1;
}
