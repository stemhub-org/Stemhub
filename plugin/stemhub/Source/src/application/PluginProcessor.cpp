#include "application/PluginProcessor.hpp"
#include "application/PluginState.hpp"
#include "ui/PluginEditor.hpp"

namespace
{
std::unique_ptr<juce::FileLogger> sharedFileLogger;
juce::Logger* previousLogger = nullptr;
int fileLoggerUsers = 0;
}

StemhubAudioProcessor::FileLoggerScope::FileLoggerScope()
{
    if (++fileLoggerUsers > 1)
        return;

    previousLogger = juce::Logger::getCurrentLogger();
    sharedFileLogger.reset(juce::FileLogger::createDefaultAppLogger("Stemhub", "plugin.log", "Stemhub plugin log", 1024 * 1024));
    juce::Logger::setCurrentLogger(sharedFileLogger != nullptr ? sharedFileLogger.get() : previousLogger);
}

StemhubAudioProcessor::FileLoggerScope::~FileLoggerScope()
{
    if (--fileLoggerUsers > 0)
        return;

    juce::Logger::setCurrentLogger(previousLogger);
    previousLogger = nullptr;
    sharedFileLogger.reset();
}

StemhubAudioProcessor::StemhubAudioProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      session(std::make_shared<ApiClient>(), SessionStorage::forCurrentUser())
{
    juce::Logger::writeToLog("StemhubAudioProcessor constructor");
    session.addChangeListener(this);
}

StemhubAudioProcessor::~StemhubAudioProcessor()
{
    juce::Logger::writeToLog("StemhubAudioProcessor destructor");
    cancelPendingUpdate();
    session.removeChangeListener(this);
}

const juce::String StemhubAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool StemhubAudioProcessor::acceptsMidi() const
{
#if JucePlugin_WantsMidiInput
    return true;
#else
    return false;
#endif
}

bool StemhubAudioProcessor::producesMidi() const
{
#if JucePlugin_ProducesMidiOutput
    return true;
#else
    return false;
#endif
}

bool StemhubAudioProcessor::isMidiEffect() const
{
#if JucePlugin_IsMidiEffect
    return true;
#else
    return false;
#endif
}

double StemhubAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int StemhubAudioProcessor::getNumPrograms()
{
    return 1;
}

int StemhubAudioProcessor::getCurrentProgram()
{
    return 0;
}

void StemhubAudioProcessor::setCurrentProgram(int index)
{
    juce::ignoreUnused(index);
}

const juce::String StemhubAudioProcessor::getProgramName(int index)
{
    juce::ignoreUnused(index);
    return {};
}

void StemhubAudioProcessor::changeProgramName(int index, const juce::String& newName)
{
    juce::ignoreUnused(index, newName);
}

void StemhubAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused(sampleRate, samplesPerBlock);
}

void StemhubAudioProcessor::releaseResources()
{
}

// A pass-through effect: mono or stereo, with as many outputs as inputs.
bool StemhubAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto output = layouts.getMainOutputChannelSet();
    return (output == juce::AudioChannelSet::mono() || output == juce::AudioChannelSet::stereo())
        && output == layouts.getMainInputChannelSet();
}

template <typename SampleType>
static void clearExtraOutputChannels(juce::AudioProcessor& processor, juce::AudioBuffer<SampleType>& buffer)
{
    const auto totalNumInputChannels = processor.getTotalNumInputChannels();
    const auto totalNumOutputChannels = processor.getTotalNumOutputChannels();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, buffer.getNumSamples());
}

void StemhubAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    juce::ignoreUnused(midiMessages);
    clearExtraOutputChannels(*this, buffer);
}

void StemhubAudioProcessor::processBlock(juce::AudioBuffer<double>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    juce::ignoreUnused(midiMessages);
    clearExtraOutputChannels(*this, buffer);
}

bool StemhubAudioProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor* StemhubAudioProcessor::createEditor()
{
    return new StemhubAudioProcessorEditor(*this, session);
}

void StemhubAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    ProjectLink link;
    {
        const juce::SpinLock::ScopedLockType lock(linkLock);
        link = linkForHost;
    }

    destData = stemhub::pluginstate::encode(link);
}

void StemhubAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    auto link = stemhub::pluginstate::decode(data, static_cast<size_t>(juce::jmax(0, sizeInBytes)));
    {
        const juce::SpinLock::ScopedLockType lock(linkLock);
        linkForHost = link;
        linkFromHost = std::move(link);
    }

    triggerAsyncUpdate();
}

void StemhubAudioProcessor::handleAsyncUpdate()
{
    std::optional<ProjectLink> link;
    {
        const juce::SpinLock::ScopedLockType lock(linkLock);
        std::swap(link, linkFromHost);
    }

    if (link.has_value())
        session.restoreLink(std::move(*link));

    // The session keeps a project that is already open, or takes a restore hand-off instead.
    updateLinkForHost();
}

void StemhubAudioProcessor::changeListenerCallback(juce::ChangeBroadcaster* source)
{
    juce::ignoreUnused(source);
    updateLinkForHost();
}

void StemhubAudioProcessor::updateLinkForHost()
{
    const auto& link = session.getState().link;
    bool didChange = false;
    {
        const juce::SpinLock::ScopedLockType lock(linkLock);
        didChange = linkForHost != link;
        if (didChange)
            linkForHost = link;
    }

    // Marks the DAW project as modified, so the new link is saved with it.
    if (didChange)
        updateHostDisplay(ChangeDetails().withNonParameterStateChanged(true));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new StemhubAudioProcessor();
}
