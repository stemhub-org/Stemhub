#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.hpp"
#include "Views.hpp"

class StemhubAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit StemhubAudioProcessorEditor(StemhubAudioProcessor& processorToEdit);
    ~StemhubAudioProcessorEditor() override = default;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void refreshSessionUi();
    void handleSignInClick();
    void handleSignOutClick();
    void handleSaveChangesClick();
    void handleSyncClick();
    void handleChangeBranchClick();

    juce::String buildStatusMessage() const;
    LoginView loginView;
    DashboardView dashboardView;
    StemhubAudioProcessor& audioProcessor;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StemhubAudioProcessorEditor)
};
