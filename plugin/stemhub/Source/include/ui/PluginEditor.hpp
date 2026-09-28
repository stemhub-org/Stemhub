#pragma once

#include <functional>
#include <memory>

#include <JuceHeader.h>

#include "application/BackgroundJobCoordinator.hpp"
#include "application/SnapshotFiles.hpp"
#include "application/StemhubSession.hpp"
#include "ui/DashboardView.hpp"
#include "ui/LoginView.hpp"
#include "ui/PluginTheme.hpp"
#include "ui/ProjectSelectionView.hpp"
#include "ui/SessionPresenter.hpp"

// Shows the session's state through SessionPresenter's models, and turns clicks into the
// session's intents. Keys it doesn't use, Cmd/Ctrl+S among them, go to the DAW.
class StemhubAudioProcessorEditor : public juce::AudioProcessorEditor,
                                    private juce::ChangeListener,
                                    private juce::AsyncUpdater
{
public:
    StemhubAudioProcessorEditor(juce::AudioProcessor& ownerProcessor, StemhubSession& sessionToShow);
    ~StemhubAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void changeListenerCallback(juce::ChangeBroadcaster* source) override;
    // A snapshot count finished.
    void handleAsyncUpdate() override;
    void refreshSessionUi();
    // Counts in the background what a save of the working file takes, when that file isn't the
    // one counted last. Clearing countedProjectFile asks for a recount (after a save, on Sync).
    void countSnapshot(const juce::String& workingFilePath);

    void signIn();
    void signOut();
    void chooseProjectFile();
    void createProject();
    void save();
    void sync();
    void restore(const juce::String& versionId);

    void launchProjectFileChooser(const juce::String& title,
                                  const juce::File& initialFile,
                                  std::function<void(const juce::File&)> onFileChosen);
    void launchProjectFolderChooser(const juce::String& title,
                                    std::function<void(const juce::File&)> onFolderChosen);

    StemhubSession& session;
    SessionPresenter presenter;
    // Declared before the views: it owns the embedded brand typefaces they build fonts from.
    stemhub::plugin::theme::StemhubPluginLookAndFeel pluginLookAndFeel;
    LoginView loginView;
    ProjectSelectionView projectSelectionView;
    DashboardView dashboardView;
    juce::TooltipWindow tooltipWindow { this, 600 };
    std::unique_ptr<juce::FileChooser> fileChooser;
    // The file whose snapshot size the dashboard shows, or is counting.
    juce::File countedProjectFile;

    // Declared last so it is destroyed first: its worker stops before anything else goes away.
    BackgroundJobCoordinator<stemhub::snapshotfiles::Summary> snapshotCounter { 1, [this] { triggerAsyncUpdate(); } };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StemhubAudioProcessorEditor)
};
