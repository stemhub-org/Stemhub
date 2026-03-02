#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.hpp"

class StemhubAudioProcessorEditor : public juce::AudioProcessorEditor
{
    public:
        explicit StemhubAudioProcessorEditor (StemhubAudioProcessor& processorToEdit);
        ~StemhubAudioProcessorEditor() override = default;

        void paint (juce::Graphics&) override;
        void resized() override;

    private:
        juce::TextButton signedOutButton { "Signed Out" };
        juce::TextButton signingInButton { "Signing In" };
        juce::TextButton signedInButton { "Signed In" };
        juce::TextButton authErrorButton { "Auth Error" };
        StemhubAudioProcessor& audioProcessor;
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StemhubAudioProcessorEditor)
};
