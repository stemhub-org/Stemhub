#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.hpp"
#include "Views.hpp"

class StemhubAudioProcessorEditor : public juce::AudioProcessorEditor,
                                    private juce::ChangeListener
{
public:
    explicit StemhubAudioProcessorEditor(StemhubAudioProcessor& processorToEdit);
    ~StemhubAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void changeListenerCallback(juce::ChangeBroadcaster* source) override;
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
