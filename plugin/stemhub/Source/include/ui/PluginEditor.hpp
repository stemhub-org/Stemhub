#pragma once

#include <JuceHeader.h>
#include "application/BackgroundJobCoordinator.hpp"
#include "application/SnapshotFiles.hpp"
#include "application/StemhubSession.hpp"
#include "ui/Views.hpp"
#include "ui/PluginTheme.hpp"

// Shows the session's state and turns clicks and shortcuts into its intents.
class StemhubAudioProcessorEditor : public juce::AudioProcessorEditor,
                                    private juce::ChangeListener,
                                    private juce::KeyListener,
                                    private juce::AsyncUpdater
{
public:
    StemhubAudioProcessorEditor(juce::AudioProcessor& ownerProcessor, StemhubSession& sessionToShow);
    ~StemhubAudioProcessorEditor() override;

    using juce::Component::keyPressed;
    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void changeListenerCallback(juce::ChangeBroadcaster* source) override;
    bool keyPressed(const juce::KeyPress& key, juce::Component* originatingComponent) override;
    // A snapshot count finished.
    void handleAsyncUpdate() override;
    void refreshSessionUi();
    void refreshProjectSelectionUi();
    void refreshDashboardUi();
    // Counts in the background what a save of the working file takes, when that file isn't the
    // one counted last. Clearing countedProjectFile asks for a recount (after a save, on Sync).
    void refreshSnapshotSize();
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
    bool hasActiveProjectSelection() const;
    void showCommitMessagePopupForSave();
    void requestSaveWithCommitMessage(juce::String commitMessage);

    StemhubSession& session;
    // Declared before the views: it owns the embedded brand typefaces they build fonts from.
    stemhub::plugin::theme::StemhubPluginLookAndFeel pluginLookAndFeel;
    LoginView loginView;
    ProjectSelectionView projectSelectionView;
    DashboardView dashboardView;
    juce::TooltipWindow tooltipWindow { this, 600 };
    std::unique_ptr<juce::AlertWindow> commitPopup;
    OperationState lastObservedOperationState { OperationState::idle };
    std::unique_ptr<juce::FileChooser> projectFileChooser;
    // The file whose snapshot size the dashboard shows, or is counting.
    juce::File countedProjectFile;

    // Declared last so it is destroyed first: its worker stops before anything else goes away.
    BackgroundJobCoordinator<stemhub::snapshotfiles::Summary> snapshotCounter { 1, [this] { triggerAsyncUpdate(); } };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StemhubAudioProcessorEditor)
};
