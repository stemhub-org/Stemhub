#pragma once

#include <functional>
#include <memory>
#include <variant>

#include <JuceHeader.h>

#include "application/BackgroundJobCoordinator.hpp"
#include "application/UseCases.hpp"
#include "domain/SessionState.hpp"
#include "network/ApiClient.hpp"

// The signed-in user's session: the single owner of SessionState, and the only thing the editor
// talks to. User intents start background jobs (see UseCases.hpp) whose results come back on the
// message thread. Listeners hear about every change through the ChangeBroadcaster, which merges
// bursts into one callback.
//
// One job at a time: an intent that would start a job is ignored while the session is busy, and
// only the latest request's result is applied, so signing out drops whatever was still running.
//
// Everything public runs on the message thread.
class StemhubSession : public juce::ChangeBroadcaster,
                       private juce::AsyncUpdater
{
public:
    explicit StemhubSession(std::shared_ptr<const IProjectApi> api);
    ~StemhubSession() override;

    // Stops accepting work and waits for running jobs. Call it before tearing down anything a job
    // might use; the destructor calls it too.
    void shutdown();

    // ── State ──
    [[nodiscard]] const SessionState& getState() const noexcept { return state; }
    [[nodiscard]] bool isBusy() const noexcept { return state.operationState != OperationState::idle; }
    // Save and restore write files and versions; the user can't leave the project while one runs.
    [[nodiscard]] bool isWriteOperationInProgress() const noexcept;
    // The file a save would push: the one the user just picked, else the project's working file.
    [[nodiscard]] juce::File getEffectiveProjectFile() const;

    // ── Intents ──
    void requestSignIn(const juce::String& email, const juce::String& password);
    // Signs in with the saved token, once per session unless the server couldn't be reached.
    void requestRestoreCachedSession();
    void signOut();
    // restoreLatestIfSafe: an explicit open from the project grid. When this instance has no
    // local copy of the project, or an unchanged one behind the branch head, the latest version
    // is restored into a new folder and opened in the DAW. Unsaved local changes are never replaced.
    void requestOpenProject(juce::String projectId, juce::File localProjectFile, bool restoreLatestIfSafe = false);
    void requestCreateProject(juce::File localProjectFile);
    void requestSelectBranch(juce::String branchId);
    void requestRefreshVersionHistory();
    // Uploads only the files the server doesn't have yet. See docs/content-addressed-storage.md.
    void requestPushVersion(juce::String commitMessage, juce::String dawName);
    // Downloads the version's files into a new folder inside destinationFolder, verifying SHA-256.
    void requestRestoreVersion(const juce::String& versionId, const juce::File& destinationFolder);
    void setSelectedVersionId(juce::String versionId);
    void setPendingProjectFile(const juce::File& file);
    // Back to the project grid; ignored while a save or restore runs.
    void showProjectSelection();

    // ── Configuration ──
    // Opening a project file hands it to the DAW / OS. Tests replace this so nothing is launched.
    void setOpenFileHandler(std::function<bool(const juce::File&)> handler) { openFileHandler = std::move(handler); }
    // Where opening a project restores its latest version (default: the app data folder).
    void setManagedWorkingCopyFolder(const juce::File& folder) { managedWorkingCopyFolder = folder; }

    // Applies finished jobs now instead of on the next message loop turn. Returns how many
    // results arrived, stale ones included.
    int flushPendingResultsForTesting();

private:
    using AuthRequestResult = stemhub::usecases::AuthRequestResult;
    using ProjectActivationJobResult = stemhub::usecases::ProjectActivationJobResult;
    using BranchHistoryJobResult = stemhub::usecases::BranchHistoryJobResult;
    using PushVersionJobResult = stemhub::usecases::PushVersionJobResult;
    using RestoreVersionJobResult = stemhub::usecases::RestoreVersionJobResult;

    using JobPayload = std::variant<AuthRequestResult,
                                    ProjectActivationJobResult,
                                    BranchHistoryJobResult,
                                    PushVersionJobResult,
                                    RestoreVersionJobResult>;

    void handleAsyncUpdate() override;
    int applyFinishedJobs();

    // Every request gets a new epoch; a result is applied only if nothing was requested since.
    uint64_t beginRequest() noexcept { return ++currentRequestEpoch; }
    [[nodiscard]] bool isCurrent(uint64_t requestEpoch) const noexcept { return requestEpoch == currentRequestEpoch; }

    // Runs `run` with the API on a worker thread. `run` gets everything else by value, built on
    // the message thread, and must not capture this session.
    void enqueue(std::function<JobPayload(const IProjectApi&)> run);

    // False when the result was stale and dropped.
    bool applyResult(JobPayload payload);
    void apply(AuthRequestResult result);
    void apply(ProjectActivationJobResult result);
    void apply(BranchHistoryJobResult result);
    void apply(PushVersionJobResult result);
    void apply(RestoreVersionJobResult result);

    void requestRestoreCachedProjectContext();
    // The backend refused the token: sign out and say why on the login screen.
    void expireSession(const juce::String& message = "Your session expired. Sign in again.");
    void enterProject(Project project, juce::String branchId, juce::String branchName, juce::File projectFile);
    void clearWorkingCopy();
    [[nodiscard]] bool hasCleanWorkingCopy(const juce::File& workingFile) const;
    // The version the next save of projectFile builds on: the one it holds, else the branch head.
    [[nodiscard]] juce::String getParentVersionForNextSave(const juce::File& projectFile) const;
    void changed() { sendChangeMessage(); }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StemhubSession)

    SessionState state;
    std::shared_ptr<const IProjectApi> api;
    uint64_t currentRequestEpoch { 0 };
    bool didAttemptCachedSessionRestore { false };
    std::function<bool(const juce::File&)> openFileHandler;
    juce::File managedWorkingCopyFolder;

    // Declared last so it is destroyed first: its workers must stop before anything they use goes away.
    BackgroundJobCoordinator<JobPayload> jobs { 2, [this] { triggerAsyncUpdate(); } };
};
