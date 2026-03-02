#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.hpp"

class StemhubAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit StemhubAudioProcessorEditor (StemhubAudioProcessor&);
    ~StemhubAudioProcessorEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    StemhubAudioProcessor& processor;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StemhubAudioProcessorEditor)
};
