#pragma once

#include <functional>
#include <memory>
#include <variant>

#include <JuceHeader.h>

#include "application/BackgroundJobCoordinator.hpp"
#include "application/SessionStorage.hpp"
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
// Each plugin instance has its own session, linked to the StemHub project of the DAW project it
// lives in (SessionState::link). The processor saves that link in the DAW project.
//
// Everything public runs on the message thread.
class StemhubSession : public juce::ChangeBroadcaster,
                       private juce::AsyncUpdater
{
public:
    StemhubSession(std::shared_ptr<const IProjectApi> api, SessionStorage storage);
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
    // Once signed in, the linked project opens.
    void requestRestoreSavedSession();
    // Keeps the link, so signing in again reopens this DAW project's StemHub project.
    void signOut();
    // The link saved in the DAW project, as the host hands it back, ignored once a project is
    // open here. A restore hand-off waiting for that project (for any project, when there is no
    // link) is taken now: the DAW is opening that restored copy.
    void restoreLink(ProjectLink savedLink);
    // restoreLatestIfSafe: an explicit open from the project grid. When this instance has no
    // local copy of the project, or an unchanged one behind the branch head, the latest version
    // is restored into a new folder and opened in the DAW. Unsaved local changes are never replaced.
    void requestOpenProject(juce::String projectId, juce::File localProjectFile, bool restoreLatestIfSafe = false);
    void requestCreateProject(juce::File localProjectFile);
    void requestSelectBranch(juce::String branchId);
    void requestRefreshVersionHistory();
    // Uploads only the files the server doesn't have yet. See docs/content-addressed-storage.md.
    void requestPushVersion(juce::String commitMessage);
    // Downloads the version's files into a new folder inside destinationFolder, verifying SHA-256.
    void requestRestoreVersion(const juce::String& versionId, const juce::File& destinationFolder);
    // Stops the job in progress at its next step (between two files, or during a transfer). It
    // still ends with a result: a save that already created its version stays saved.
    void cancelRequest();
    void setSelectedVersionId(juce::String versionId);
    void setPendingProjectFile(const juce::File& file);
    // Back to the project grid; ignored while a save or restore runs.
    void showProjectSelection();

    // ── Configuration ──
    // Opening a project file hands it to the DAW / OS. Tests replace this so nothing is launched.
    void setOpenFileHandler(std::function<bool(const juce::File&)> handler) { openFileHandler = std::move(handler); }

    // Applies finished jobs now instead of on the next message loop turn. Returns how many jobs
    // finished, stale ones included; progress reports don't count.
    int flushPendingResultsForTesting();

private:
    using AuthRequestResult = stemhub::usecases::AuthRequestResult;
    using ProjectActivationJobResult = stemhub::usecases::ProjectActivationJobResult;
    using BranchHistoryJobResult = stemhub::usecases::BranchHistoryJobResult;
    using PushVersionJobResult = stemhub::usecases::PushVersionJobResult;
    using RestoreVersionJobResult = stemhub::usecases::RestoreVersionJobResult;
    using ProgressReport = stemhub::usecases::ProgressReport;
    using ReportProgress = stemhub::usecases::ReportProgress;

    using JobPayload = std::variant<AuthRequestResult,
                                    ProjectActivationJobResult,
                                    BranchHistoryJobResult,
                                    PushVersionJobResult,
                                    RestoreVersionJobResult,
                                    ProgressReport>;
    using Jobs = BackgroundJobCoordinator<JobPayload>;

    void handleAsyncUpdate() override;
    int applyFinishedJobs();

    // Every request gets a new epoch; a result is applied only if nothing was requested since.
    uint64_t beginRequest()
    {
        cancelledMessage.clear();
        return ++currentRequestEpoch;
    }

    [[nodiscard]] bool isCurrent(uint64_t requestEpoch) const noexcept { return requestEpoch == currentRequestEpoch; }

    // Runs `run` with the API and a progress reporter on a worker thread, and tags its result and
    // reports with epoch. `run` gets everything else by value, built on the message thread, and
    // must not capture this session.
    void enqueue(uint64_t epoch, std::function<JobPayload(const IProjectApi&, const ReportProgress&)> run);

    // False when the result was stale and dropped.
    bool applyResult(JobPayload payload);
    void apply(AuthRequestResult result);
    void apply(ProjectActivationJobResult result);
    void apply(BranchHistoryJobResult result);
    void apply(PushVersionJobResult result);
    void apply(RestoreVersionJobResult result);
    void apply(ProgressReport report);
    // Why a job failed, on the screen it belongs to; or that it was cancelled, which is no error.
    void showFailure(Status& target, const juce::String& action, const juce::String& errorMessage);
    [[nodiscard]] Status& statusOfCurrentScreen() noexcept;

    // Keeps the link: it belongs to the DAW project, not to whoever is signed in.
    void resetState();
    void openLinkedProject();
    void takeRestoreHandoff(const juce::String& projectId);
    // Leaves a restored copy for the instance the DAW opens it in, then asks the DAW to open it.
    void handOverRestoredCopy(const WorkingCopyBaseline& restoredCopy);
    // The backend refused the token: sign out and say why on the login screen.
    void expireSession(const juce::String& message = "Your session expired. Sign in again.");
    void enterProject(Project project, juce::String branchId, juce::String branchName, juce::File projectFile);
    void clearWorkingCopy();
    [[nodiscard]] bool hasCleanWorkingCopy(const juce::File& workingFile) const;
    // The version the next save of projectFile builds on: the one it holds, else the branch head.
    [[nodiscard]] juce::String getParentVersionForNextSave(const juce::File& projectFile) const;
    // Once a project is open here, it is what the DAW project is linked to.
    void refreshLink();
    void changed()
    {
        refreshLink();
        sendChangeMessage();
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StemhubSession)

    SessionState state;
    std::shared_ptr<const IProjectApi> api;
    SessionStorage storage;
    // What the linked working file holds, when a restore hand-off said so.
    WorkingCopyBaseline linkedCopy;
    uint64_t currentRequestEpoch { 0 };
    // Set by cancelRequest() until the next request: what the cancelled job's failure shows.
    juce::String cancelledMessage;
    bool didAttemptSavedSessionRestore { false };
    std::function<bool(const juce::File&)> openFileHandler;

    // Declared last so it is destroyed first: its workers must stop before anything they use goes away.
    Jobs jobs { 2, [this] { triggerAsyncUpdate(); } };
};
