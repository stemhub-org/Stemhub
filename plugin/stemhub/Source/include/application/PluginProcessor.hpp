#pragma once

#include <functional>
#include <memory>
#include <JuceHeader.h>
#include <optional>
#include <atomic>
#include <vector>
#include <variant>
#include "application/BackgroundJobCoordinator.hpp"
#include "application/UseCases.hpp"
#include "domain/Branch.hpp"
#include "domain/User.hpp"
#include "domain/Project.hpp"
#include "domain/States.hpp"
#include "domain/WorkingCopyBaseline.hpp"
#include "network/ApiClient.hpp"

class StemhubAudioProcessor : public juce::AudioProcessor,
                              public juce::ChangeBroadcaster,
                              private juce::AsyncUpdater
{
public:
    StemhubAudioProcessor();
    explicit StemhubAudioProcessor(std::unique_ptr<IProjectApi> apiClient);
    ~StemhubAudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

#ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
#endif

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

    // CUSTOM METHODS
    [[nodiscard]] AuthState getAuthState() const noexcept { return sessionState.authState; }
    [[nodiscard]] UIState getUIState() const noexcept { return sessionState.uiState; }
    [[nodiscard]] OperationState getOperationState() const noexcept { return sessionState.operationState; }
    [[nodiscard]] const std::optional<User>& getCurrentUser() const noexcept { return currentUser; }
    [[nodiscard]] const std::vector<Project>& getProjects() const noexcept { return projects; }
    [[nodiscard]] const std::optional<Project>& getSelectedProject() const noexcept { return selectedProject; }
    [[nodiscard]] const std::vector<Branch>& getBranches() const noexcept { return branches; }
    [[nodiscard]] const std::vector<VersionSummary>& getVersionHistory() const noexcept { return versionHistory; }
    [[nodiscard]] const juce::String& getSelectedBranchId() const noexcept { return selectedBranchId; }
    [[nodiscard]] const juce::String& getSelectedBranchName() const noexcept { return selectedBranchName; }
    [[nodiscard]] const juce::String& getSelectedVersionId() const noexcept { return selectedVersionId; }
    [[nodiscard]] const juce::String& getCurrentOpenedVersionId() const noexcept { return currentOpenedVersionId; }
    [[nodiscard]] const juce::String& getAuthErrorMessage() const noexcept { return authErrorMessage; }
    [[nodiscard]] const juce::String& getProjectSelectionStatusMessage() const noexcept { return projectSelectionStatusMessage; }
    [[nodiscard]] const juce::String& getActiveProjectStatusMessage() const noexcept { return activeProjectStatusMessage; }
    [[nodiscard]] const juce::File& getPendingProjectFile() const noexcept { return pendingProjectFile; }
    [[nodiscard]] const juce::File& getSelectedProjectFile() const noexcept { return selectedProjectFile; }
    // Save and restore write files and versions; nothing else may start while one runs.
    [[nodiscard]] bool isWriteOperationInProgress() const noexcept;

    void signIn(User newUser) noexcept;
    void signOut() noexcept;

    void setAuthState(AuthState newAuthState) noexcept;
    void setUIState(UIState newUIState) noexcept;
    void setOperationState(OperationState newOperationState) noexcept;
    void setActiveProjectStatusMessage(juce::String message);
    void setPendingProjectFile(const juce::File& file);
    void selectProject(Project project, juce::String branchId, juce::String branchName, juce::File projectFile);
    void clearSelectedProject() noexcept;
    
    void requestSignIn(const juce::String& email, const juce::String& password);
    void requestRestoreCachedSession();
    // restoreLatestIfSafe: an explicit open from the project grid. When this instance has no
    // local copy of the project, or a clean one behind the branch head, the latest version is
    // restored into a new folder and opened in the DAW. Unsaved local changes are never replaced.
    void requestOpenProject(juce::String projectId, juce::File localProjectFile, bool restoreLatestIfSafe = false);
    void requestCreateProject(juce::File localProjectFile);
    void requestSelectBranch(juce::String branchId);
    void requestRefreshVersionHistory();
    // Uploads only the files the server doesn't have yet. See docs/content-addressed-storage.md.
    void requestPushVersion(juce::String commitMessage, juce::String dawName);
    // Downloads the version's files into a new folder inside destinationFolder, verifying SHA-256.
    void requestRestoreVersion(const juce::String& versionId, const juce::File& destinationFolder);

    void setSelectedVersionId(juce::String versionId);
    // Opening a project file hands it to the DAW / OS. Tests replace this so nothing is launched.
    void setOpenFileHandler(std::function<bool(const juce::File&)> handler) { openFileHandler = std::move(handler); }
    // Where opening a project restores its latest version (default: the app data folder).
    void setManagedWorkingCopyFolder(const juce::File& folder) { managedWorkingCopyFolder = folder; }
    void flushPendingBackgroundResultsForTesting();

private:
    void handleAsyncUpdate() override;

    using AuthRequestResult = stemhub::usecases::AuthRequestResult;
    using ProjectActivationJobResult = stemhub::usecases::ProjectActivationJobResult;
    using BranchHistoryJobResult = stemhub::usecases::BranchHistoryJobResult;
    using PushVersionJobResult = stemhub::usecases::PushVersionJobResult;
    using RestoreVersionJobResult = stemhub::usecases::RestoreVersionJobResult;

    using BackgroundJobPayload = std::variant<AuthRequestResult, ProjectActivationJobResult, BranchHistoryJobResult, PushVersionJobResult, RestoreVersionJobResult>;

    using BackgroundJobResult = BackgroundJobCoordinator<BackgroundJobPayload>::JobResult;

    // Runs `run` with the API on a worker thread. `run` gets everything else by value, built on
    // the message thread, and must not capture this processor: the result comes back through
    // handleAsyncUpdate().
    void enqueueBackgroundTask(std::function<BackgroundJobPayload(const IProjectApi&)> run);

    void applyBackgroundResult(BackgroundJobResult result);
    void applyAuthRequestResult(AuthRequestResult result);
    void applyProjectActivationResult(ProjectActivationJobResult result);
    void applyBranchHistoryResult(BranchHistoryJobResult result);
    void applyPushVersionResult(PushVersionJobResult result);
    void applyRestoreVersionResult(RestoreVersionJobResult result);
    void requestRestoreCachedProjectContext();
    // Called when the backend refuses the token: signs out and shows message on the login screen.
    void expireSession(const juce::String& message = "Your session expired. Sign in again.");
    void clearWorkingCopy();
    uint64_t beginSelectionRequest() noexcept;
    [[nodiscard]] bool isCurrentSelectionRequest(uint64_t requestId) const noexcept;
    [[nodiscard]] bool hasCleanWorkingCopy(const juce::File& workingFile) const;
    // The version the next save of projectFile builds on: the one it holds, else the branch head.
    [[nodiscard]] juce::String getParentVersionForNextSave(const juce::File& projectFile) const;
    void setCurrentOpenedVersionId(juce::String versionId);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StemhubAudioProcessor)

    // Shared with the jobs that are running, which may still hold it for a moment after a request.
    std::shared_ptr<const IProjectApi> apiClient;
    juce::String access_tkn;
    juce::String authErrorMessage;
    juce::String projectSelectionStatusMessage;
    juce::String activeProjectStatusMessage;
    std::optional<User> currentUser;
    std::vector<Project> projects;
    std::vector<Branch> branches;
    std::vector<VersionSummary> versionHistory;
    std::optional<Project> selectedProject;
    juce::String selectedBranchId;
    juce::String selectedBranchName;
    juce::String selectedVersionId;
    SessionState sessionState;
    juce::File pendingProjectFile;
    juce::File selectedProjectFile;
    juce::String currentOpenedVersionId;
    // What the local project file holds; set by saves, restores and restore-folder names.
    WorkingCopyBaseline workingCopy;
    bool didAttemptCachedSessionRestore { false };
    std::atomic<uint64_t> activeSelectionRequestId { 0 };
    std::function<bool(const juce::File&)> openFileHandler;
    juce::File managedWorkingCopyFolder;

    // Declared last so it is destroyed first: its workers must stop before anything they use goes away.
    BackgroundJobCoordinator<BackgroundJobPayload> backgroundJobs { 2, [this] { triggerAsyncUpdate(); } };
};
