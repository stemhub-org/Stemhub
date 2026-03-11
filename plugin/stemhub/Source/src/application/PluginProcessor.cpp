#include "application/PluginProcessor.hpp"

namespace
{
std::unique_ptr<juce::FileLogger> globalFileLogger;
juce::Logger* previousLogger = nullptr;
int activeLoggerUsers = 0;

void installStemhubFileLogger()
{
    if (++activeLoggerUsers > 1)
        return;

    previousLogger = juce::Logger::getCurrentLogger();

    globalFileLogger.reset(juce::FileLogger::createDefaultAppLogger(
        "Stemhub",
        "plugin.log",
        "Stemhub plugin log",
        1024 * 1024));

    if (globalFileLogger)
        juce::Logger::setCurrentLogger(globalFileLogger.get());
    else
        juce::Logger::setCurrentLogger(previousLogger);
}

void uninstallStemhubFileLogger()
{
    if (activeLoggerUsers == 0)
        return;

    if (--activeLoggerUsers == 0)
    {
        juce::Logger::setCurrentLogger(previousLogger);
        previousLogger = nullptr;
        globalFileLogger.reset();
    }
}
}

StemhubAudioProcessor::StemhubAudioProcessor(std::unique_ptr<IProjectApi> apiClientProvider)
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor(BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput("Input", juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       )
#endif
{
    installStemhubFileLogger();
    juce::Logger::writeToLog("StemhubAudioProcessor constructor");

    apiClient = std::move(apiClientProvider);
    if (apiClient == nullptr)
        apiClient = std::make_unique<ApiClient>();

    versionControlService.setApiClient(*apiClient);
}

StemhubAudioProcessor::StemhubAudioProcessor()
    : StemhubAudioProcessor(std::make_unique<ApiClient>())
{
}

StemhubAudioProcessor::~StemhubAudioProcessor()
{
    juce::Logger::writeToLog("StemhubAudioProcessor destructor");
    cancelPendingUpdate();
    uninstallStemhubFileLogger();
}

void StemhubAudioProcessor::enqueueBackgroundTask(std::function<BackgroundJobPayload()> taskFactory)
{
    backgroundJobs.enqueue(std::move(taskFactory), [this]()
    {
        triggerAsyncUpdate();
    });
}
