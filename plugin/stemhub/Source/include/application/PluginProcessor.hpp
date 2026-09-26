#pragma once

#include <optional>

#include <JuceHeader.h>

#include "application/Log.hpp"
#include "application/StemhubSession.hpp"

// The plugin as the host sees it. Audio passes through untouched; everything StemHub does lives
// in the session, which the editor drives. The processor saves the session's project link in the
// DAW project and hands it back when the project loads.
class StemhubAudioProcessor final : public juce::AudioProcessor,
                                    private juce::ChangeListener,
                                    private juce::AsyncUpdater
{
public:
    StemhubAudioProcessor();
    ~StemhubAudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

    using AudioProcessor::processBlock;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlock(juce::AudioBuffer<double>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

private:
    void changeListenerCallback(juce::ChangeBroadcaster* source) override;
    // Hands the link the host restored to the session, on the message thread.
    void handleAsyncUpdate() override;
    // Makes what the host saves follow the session's link, and tells the host when it changed.
    void updateLinkForHost();

    // Declared before the session, so the log outlives the session's jobs.
    juce::SharedResourcePointer<stemhub::log::LogFile> logFile;

    // Hosts save and restore state from any thread, so these are only used under linkLock; the
    // session itself stays on the message thread.
    juce::SpinLock linkLock;
    ProjectLink linkForHost;
    std::optional<ProjectLink> linkFromHost;

    StemhubSession session;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StemhubAudioProcessor)
};
