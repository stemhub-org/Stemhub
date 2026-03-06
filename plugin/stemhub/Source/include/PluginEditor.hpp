#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.hpp"
#include "Views.hpp"

class StemhubAudioProcessorEditor : public juce::AudioProcessorEditor,
                                    private juce::ChangeListener,
                                    private juce::KeyListener
{
public:
    explicit StemhubAudioProcessorEditor(StemhubAudioProcessor& processorToEdit);
    ~StemhubAudioProcessorEditor() override;

    using juce::Component::keyPressed;
    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void changeListenerCallback(juce::ChangeBroadcaster* source) override;
    bool keyPressed(const juce::KeyPress& key, juce::Component* originatingComponent) override;
    void refreshSessionUi();
    void refreshProjectSelectionUi();
    void refreshDashboardUi();
    void handleChooseProjectFileClick();
    void handleOpenProjectClick();
    void handleCreateProjectClick();
    void handleSignInClick();
    void handleSignOutClick();
    void handleSaveChangesClick();
    void handleSyncClick();
    void handleChangeBranchClick();
    void handleVersionSelectionChanged();
    void handleBackToProjectsClick();
    void handleRestoreClick();

    void launchProjectFileChooser(const juce::String& title,
                                  std::function<void(const juce::File&)> onFileChosen);
    void launchProjectFolderChooser(const juce::String& title,
                                   std::function<void(const juce::File&)> onFolderChosen);
    void triggerPushVersion(const juce::String& commitMessage);
    bool hasActiveProjectSelection() const;
    juce::File getEffectiveProjectFile() const;
    void showCommitMessagePopupForSave();
    void requestSaveWithCommitMessage(juce::String commitMessage);
    LoginView loginView;
    ProjectSelectionView projectSelectionView;
    DashboardView dashboardView;
    StemhubAudioProcessor& audioProcessor;
    std::unique_ptr<juce::FileChooser> projectFileChooser;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StemhubAudioProcessorEditor)
};
