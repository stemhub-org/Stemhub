#include "application/Log.hpp"

namespace stemhub::log
{
namespace
{
void write(const char* level, const juce::String& message)
{
    juce::Logger::writeToLog(juce::Time::getCurrentTime().toISO8601(true) + " " + level + " " + message);
}
}

void info(const juce::String& message)
{
    write("INFO", message);
}

void warning(const juce::String& message)
{
    write("WARNING", message);
}

void error(const juce::String& message)
{
    write("ERROR", message);
}

LogFile::LogFile()
    : fileLogger(juce::FileLogger::createDefaultAppLogger("Stemhub", "plugin.log", "Stemhub plugin log", 1024 * 1024)),
      previousLogger(juce::Logger::getCurrentLogger())
{
    if (fileLogger != nullptr)
        juce::Logger::setCurrentLogger(fileLogger.get());
}

LogFile::~LogFile()
{
    if (fileLogger != nullptr)
        juce::Logger::setCurrentLogger(previousLogger);
}
}
