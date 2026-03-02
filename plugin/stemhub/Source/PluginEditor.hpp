#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.hpp"

class StemhubAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit StemhubAudioProcessorEditor(StemhubAudioProcessor& processorToEdit);
    ~StemhubAudioProcessorEditor() override = default;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void refreshAuthStateLabel();
    void refreshComponentVisibility();

    juce::TextEditor usernameInput;
    juce::TextEditor passwordInput;
    juce::Label authStateLabel;

    juce::TextButton signInButton { "Sign In" };
    StemhubAudioProcessor& audioProcessor;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StemhubAudioProcessorEditor)
};
