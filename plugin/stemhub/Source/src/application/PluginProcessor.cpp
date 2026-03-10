#include "application/PluginProcessor.hpp"

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
    cancelPendingUpdate();
}

void StemhubAudioProcessor::enqueueBackgroundTask(std::function<BackgroundJobPayload()> taskFactory)
{
    backgroundJobs.enqueue(std::move(taskFactory), [this]()
    {
        triggerAsyncUpdate();
    });
}
