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
    void refreshSessionUi();
    void refreshAuthStateLabel();
    void refreshComponentVisibility();
    void handleSignInClick();
    void handleSignOutClick();
    void handleSaveChangesClick();
    void handleSyncClick();
    void handleChangeBranchClick();

    juce::TextEditor usernameInput;
    juce::TextEditor passwordInput;
    juce::Label authStateLabel;

    juce::TextButton saveChanges { "Save" }; // Commit button
    juce::TextButton syncButton { "Sync" }; // pull / merge button
    juce::TextButton changeBranch { "view other branches" }; // branch management button

    juce::TextButton signInButton { "Sign In" };
    juce::TextButton signOutButton { "Sign Out" };
    StemhubAudioProcessor& audioProcessor;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StemhubAudioProcessorEditor)
};
