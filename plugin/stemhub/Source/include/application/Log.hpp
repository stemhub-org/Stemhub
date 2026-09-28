#pragma once

#include <memory>

#include <JuceHeader.h>

// The plugin's log. Lines go to JUCE's current logger: plugin.log while a LogFile is held, the
// console otherwise (tests). Never log tokens or passwords.
namespace stemhub::log
{
void info(const juce::String& message);
void warning(const juce::String& message);
void error(const juce::String& message);

// plugin.log in the user's log folder, installed as JUCE's logger. Every plugin instance holds it
// through a juce::SharedResourcePointer<LogFile>, so one file serves them all and outlives the
// last instance's jobs.
class LogFile
{
public:
    LogFile();
    ~LogFile();

private:
    std::unique_ptr<juce::FileLogger> fileLogger;
    juce::Logger* previousLogger { nullptr };

    JUCE_DECLARE_NON_COPYABLE(LogFile)
};
}
