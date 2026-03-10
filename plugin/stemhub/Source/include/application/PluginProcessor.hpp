#pragma once

#include <functional>
#include <memory>
#include <JuceHeader.h>
#include <optional>
#include <vector>
#include <variant>
#include "application/BackgroundJobCoordinator.hpp"
#include "domain/Branch.hpp"
#include "domain/User.hpp"
#include "domain/Project.hpp"
#include "domain/States.hpp"
#include "network/ApiUtils.hpp"
#include "network/ApiClient.hpp"
#include "application/VersionControlService.hpp"

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
    [[nodiscard]] const SessionState& getSessionState() const noexcept { return sessionState; }
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
    [[nodiscard]] juce::String getCurrentOpenedVersionLabel() const;
    [[nodiscard]] const juce::String& getAccessToken() const noexcept { return access_tkn; }
    [[nodiscard]] const juce::String& getAuthErrorMessage() const noexcept { return authErrorMessage; }
    [[nodiscard]] const juce::String& getProjectSelectionStatusMessage() const noexcept { return projectSelectionStatusMessage; }
    [[nodiscard]] const juce::String& getActiveProjectStatusMessage() const noexcept { return activeProjectStatusMessage; }
    [[nodiscard]] const juce::File& getPendingProjectFile() const noexcept { return pendingProjectFile; }
    [[nodiscard]] const juce::File& getSelectedProjectFile() const noexcept { return selectedProjectFile; }

    void setCurrentUser(std::optional<User> newUser) noexcept { currentUser = std::move(newUser); }
    void signIn(User newUser) noexcept;
    void signOut() noexcept;
    
    [[nodiscard]] juce::String getUsername() const noexcept { return currentUser ? currentUser->username : juce::String();}
    
    void setAuthState(AuthState newAuthState) noexcept;
    void setUIState(UIState newUIState) noexcept;
    void setOperationState(OperationState newOperationState) noexcept;
    void setProjectSelectionStatusMessage(juce::String message);
    void setActiveProjectStatusMessage(juce::String message);
    void setPendingProjectFile(const juce::File& file);
    void selectProject(Project project, juce::String branchId, juce::String branchName, juce::File projectFile);
    void clearSelectedProject() noexcept;
    
    void requestSignIn(const juce::String& email, const juce::String& password);
    void requestRestoreCachedSession();
    void requestOpenProject(juce::String projectId, juce::File localProjectFile, bool preferRemoteLatest = false);
    void requestCreateProject(juce::File localProjectFile);
    void requestSelectBranch(juce::String branchId);
    void requestRefreshVersionHistory();
    void requestPushVersion(juce::String commitMessage, juce::String dawName);
    void requestRestoreVersion(const juce::String& versionId, const juce::File& destinationFolder);

    void setSelectedVersionId(juce::String versionId);
    VersionControlService& getVersionControlService() noexcept { return versionControlService; }
    IProjectApi& getApiClient() noexcept { return *apiClient; }

private:
    void handleAsyncUpdate() override;

    struct AuthRequestResult
    {
        uint64_t requestId {};
        std::optional<User> user;
        std::vector<Project> projects;
        juce::String token;
        juce::String authErrorMessage;
        juce::String projectSelectionStatusMessage;
        bool fromCachedSession { false };
    };

    struct ProjectActivationJobResult
    {
        std::optional<Project> selectedProject;
        std::vector<Project> projects;
        std::vector<Branch> branches;
        std::vector<VersionSummary> versions;
        juce::String branchId;
        juce::String branchName;
        juce::String selectedVersionId;
        juce::String workingVersionId;
        juce::File projectFile;
        juce::String errorMessage;
        juce::String projectSelectionStatusMessage;
        juce::String activeProjectStatusMessage;
        bool refreshProjects { false };
        bool shouldAutoOpenLocalFile { true };
    };

    struct BranchHistoryJobResult
    {
        std::vector<VersionSummary> versions;
        juce::String branchId;
        juce::String branchName;
        juce::String selectedVersionId;
        juce::String workingVersionId;
        juce::File projectFile;
        juce::String errorMessage;
        juce::String activeProjectStatusMessage;
    };

    struct PushVersionJobResult
    {
        juce::String pushedVersionId;
        juce::String errorMessage;
        juce::String activeProjectStatusMessage;
    };

    struct RestoreVersionJobResult
    {
        juce::File restoredProjectFile;
        juce::String restoredVersionId;
        juce::String errorMessage;
        juce::String activeProjectStatusMessage;
    };

    using BackgroundJobPayload = std::variant<AuthRequestResult, ProjectActivationJobResult, BranchHistoryJobResult, PushVersionJobResult, RestoreVersionJobResult>;

    using BackgroundJobResult = BackgroundJobCoordinator<BackgroundJobPayload>::JobResult;

    void enqueueBackgroundTask(std::function<BackgroundJobPayload()> job);

    RestoreVersionJobResult performRestoreVersionRequest(const juce::String& versionId, const juce::File& destinationFile) const;
    AuthRequestResult performSignInRequest(const juce::String& email, const juce::String& password) const;
    AuthRequestResult performRestoreCachedSessionRequest(const juce::String& token) const;
    ProjectActivationJobResult performOpenProjectRequest(const juce::String& projectId,
                                                         const juce::File& localProjectFile,
                                                         const std::vector<Project>& availableProjects,
                                                         const juce::String& accessToken,
                                                         bool preferRemoteLatest) const;
    ProjectActivationJobResult performCreateProjectRequest(const juce::File& localProjectFile,
                                                           const juce::String& accessToken) const;
    BranchHistoryJobResult performFetchBranchHistoryRequest(const juce::String& branchId,
                                                            const juce::String& branchName,
                                                            const juce::String& preferredVersionId,
                                                            const juce::String& accessToken) const;
    PushVersionJobResult performPushVersionRequest(const juce::File& projectFile,
                                                   const juce::File& projectRootDirectory,
                                                   const std::optional<Project>& project,
                                                   const juce::String& branchId,
                                                   const juce::String& commitMessage,
                                                   const juce::String& dawName);
    void applyBackgroundResult(BackgroundJobResult result);
    void applyAuthRequestResult(AuthRequestResult result);
    void applyProjectActivationResult(ProjectActivationJobResult result);
    void applyBranchHistoryResult(BranchHistoryJobResult result);
    void applyPushVersionResult(PushVersionJobResult result);
    void applyRestoreVersionResult(RestoreVersionJobResult result);
    void requestRestoreCachedProjectContext();
    void setWorkingCopyContext(const juce::File& workingFile, const juce::String& versionId);
    void clearWorkingCopyContext();
    [[nodiscard]] bool hasCleanWorkingCopy(const juce::File& workingFile) const;
    void setCurrentOpenedVersionId(juce::String versionId);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StemhubAudioProcessor)

    std::unique_ptr<IProjectApi> apiClient;
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
    BackgroundJobCoordinator<BackgroundJobPayload> backgroundJobs { 2 };
    VersionControlService versionControlService;
    juce::File pendingProjectFile;
    juce::File selectedProjectFile;
    juce::String currentOpenedVersionId;
    juce::File workingCopyProjectFile;
    juce::String workingCopyVersionId;
    int64 workingCopyFileSize { 0 };
    int64 workingCopyFileModTime { 0 };
    bool didAttemptCachedSessionRestore { false };
};
