#pragma once

#include <JuceHeader.h>

#include "application/StemhubSession.hpp"

// The plugin as the host sees it. Audio passes through untouched; everything StemHub does lives
// in the session, which the editor drives.
class StemhubAudioProcessor final : public juce::AudioProcessor
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
    // Installs the shared plugin.log while at least one instance is alive.
    struct FileLoggerScope
    {
        FileLoggerScope();
        ~FileLoggerScope();
    };

    // Declared before the session, so the log outlives the session's jobs.
    FileLoggerScope fileLogger;
    StemhubSession session;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StemhubAudioProcessor)
};
