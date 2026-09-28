#include <algorithm>
#include <utility>

#include "application/StemhubSession.hpp"
#include "application/Log.hpp"
#include "application/RestoreFolders.hpp"
#include "application/RestoreHandoff.hpp"

namespace usecases = stemhub::usecases;

namespace
{
// How the log names an operation, and what the user reads when it is cancelled.
struct OperationWords
{
    const char* name;
    const char* cancelled;
};

OperationWords wordsFor(const OperationState operation)
{
    switch (operation)
    {
        case OperationState::signingIn:       return { "Signing in", "Sign-in cancelled." };
        case OperationState::loadingProjects: return { "Opening the project", "Cancelled." };
        case OperationState::pulling:         return { "Loading the history", "Cancelled." };
        case OperationState::committing:      return { "Saving a version", "Save cancelled." };
        case OperationState::restoring:       return { "Restoring a version", "Restore cancelled." };
        case OperationState::idle:            break;
    }

    return { "Working", "Cancelled." };
}

// Hands a project file to the DAW, through the system's "open with" association.
bool openInSystem(const juce::File& file)
{
    if (!file.existsAsFile())
        return false;

    if (file.startAsProcess())
        return true;

   #if JUCE_MAC
    // Arguments go as they are: no quoting to get wrong with spaces or quotes in the path.
    juce::ChildProcess openProcess;
    return openProcess.start(juce::StringArray { "open", file.getFullPathName() });
   #else
    return false;
   #endif
}
}

StemhubSession::StemhubSession(std::shared_ptr<const IProjectApi> apiToUse, SessionStorage storageToUse)
    : api(std::move(apiToUse)),
      storage(std::move(storageToUse)),
      openFileHandler(openInSystem)
{
    jassert(api != nullptr && storage.credentials != nullptr);
}

StemhubSession::~StemhubSession()
{
    shutdown();
}

void StemhubSession::shutdown()
{
    jobs.shutdown();
    cancelPendingUpdate();
}

//==============================================================================
// Jobs and results

void StemhubSession::start(const OperationState operation, const juce::String& progress, Job job)
{
    state.operationState = operation;
    statusOfCurrentScreen() = Status::progress(progress);
    changed();

    // The job holds its own reference to the API, so it never reaches into this session.
    const auto epoch = beginRequest();
    jobs.enqueue([sharedApi = api, epoch, task = std::move(job)](const Jobs::Post& post)
    {
        const ReportProgress report = [&post, epoch](const juce::String& text)
        {
            post({ epoch, ProgressReport { text } });
        };

        return TaggedPayload { epoch, task(*sharedApi, report) };
    });
}

StemhubSession::Job StemhubSession::asSignedIn(SignedInJob job) const
{
    return [token = state.accessToken, signedInJob = std::move(job)](const IProjectApi& backend, const ReportProgress& report)
    {
        return signedInJob(SignedInApi(backend, token), report);
    };
}

bool StemhubSession::finish(const JobOutcome& outcome)
{
    auto& screenStatus = statusOfCurrentScreen();
    const auto operation = std::exchange(state.operationState, OperationState::idle);

    if (outcome.sessionExpired)
    {
        expireSession();
        return false;
    }

    if (!outcome.failed())
        return true;

    // A cancel is no error: the job stopped as asked.
    if (cancelledMessage.isNotEmpty())
    {
        screenStatus = Status::warning(cancelledMessage);
    }
    else
    {
        stemhub::log::warning(juce::String(wordsFor(operation).name) + " failed: " + outcome.errorMessage);
        screenStatus = Status::error(outcome.errorMessage);
    }

    return false;
}

void StemhubSession::refuse(Status reason)
{
    state.sessionStatus = std::move(reason);
    changed();
}

void StemhubSession::handleAsyncUpdate()
{
    applyFinishedJobs();
}

int StemhubSession::flushPendingResultsForTesting()
{
    return applyFinishedJobs();
}

int StemhubSession::applyFinishedJobs()
{
    bool didApply = false;
    int finishedJobs = 0;
    for (auto& tagged : jobs.takeResults())
    {
        if (!std::holds_alternative<ProgressReport>(tagged.payload))
            ++finishedJobs;

        didApply = applyResult(std::move(tagged)) || didApply;
    }

    if (didApply)
        changed();

    return finishedJobs;
}

bool StemhubSession::applyResult(TaggedPayload tagged)
{
    // The one staleness rule: a result counts only if nothing was requested after it. Checked
    // per result, as applying one can start a request or end the session.
    if (!isCurrent(tagged.requestEpoch))
        return false;

    std::visit([this](auto&& payload) { apply(std::move(payload)); }, std::move(tagged.payload));
    return true;
}

void StemhubSession::apply(ProgressReport report)
{
    // A report never replaces "Cancelling...", nor the outcome of a job that has ended.
    if (isBusy() && cancelledMessage.isEmpty())
        statusOfCurrentScreen() = Status::progress(std::move(report.text));
}

void StemhubSession::cancelRequest()
{
    JUCE_ASSERT_MESSAGE_THREAD
    if (!isBusy() || cancelledMessage.isNotEmpty())
        return;

    cancelledMessage = wordsFor(state.operationState).cancelled;
    jobs.stopRunningJobs();
    statusOfCurrentScreen() = Status::progress("Cancelling...");
    changed();
}

Status& StemhubSession::statusOfCurrentScreen() noexcept
{
    if (state.operationState == OperationState::signingIn)
        return state.authStatus;

    if (state.operationState == OperationState::loadingProjects)
        return state.projectsStatus;

    return state.sessionStatus;
}

//==============================================================================
// Signing in and out

void StemhubSession::requestSignIn(const juce::String& email, const juce::String& password)
{
    JUCE_ASSERT_MESSAGE_THREAD
    if (isBusy())
        return;

    resetState();
    start(OperationState::signingIn, "Signing in to your StemHub account...",
          [input = usecases::SignInInput { email, password }](const IProjectApi& backend, const auto&) -> JobPayload
          {
              return usecases::signIn(backend, input);
          });
}

void StemhubSession::requestRestoreSavedSession()
{
    JUCE_ASSERT_MESSAGE_THREAD
    if (didAttemptSavedSessionRestore)
        return;

    didAttemptSavedSessionRestore = true;

    const auto savedToken = storage.credentials->loadToken();
    if (savedToken.isEmpty() || state.isSignedIn() || isBusy())
        return;

    start(OperationState::signingIn, "Restoring your session...",
          [input = usecases::RestoreSessionInput { savedToken }](const IProjectApi& backend, const auto&) -> JobPayload
          {
              return usecases::restoreSession(backend, input);
          });
}

void StemhubSession::apply(AuthRequestResult result)
{
    // Offline, or with the server down, the saved token stays, and reopening the plugin tries again.
    if (result.fromSavedSession && result.failed() && !result.sessionExpired)
        didAttemptSavedSessionRestore = false;

    if (!finish(result))
        return;

    state.uiState = UIState::projectSelection;
    state.authStatus = {};
    state.accessToken = std::move(result.token);
    state.currentUser = std::move(result.user);
    state.projects = std::move(result.projects);
    state.projectsStatus = std::move(result.status);
    storage.credentials->saveToken(state.accessToken);

    // A DAW project saved without a link (never linked, or by an earlier version): if the DAW has
    // just opened a restored copy, this instance is the one it was restored for. Only taken now:
    // the plugin window can open before the host hands back the saved link, and a restored copy's
    // own link names its project.
    if (!state.link.isSet() && !state.selectedProject.has_value())
        takeRestoreHandoff({});

    openLinkedProject();
}

void StemhubSession::signOut()
{
    JUCE_ASSERT_MESSAGE_THREAD
    // Whatever is still running belongs to the old session: it is asked to stop, and its result
    // will be dropped.
    jobs.stopRunningJobs();
    beginRequest();
    storage.credentials->clear();
    resetState();
    changed();
}

void StemhubSession::resetState()
{
    auto link = std::move(state.link);
    state = {};
    state.link = std::move(link);
}

void StemhubSession::expireSession()
{
    stemhub::log::info("StemHub refused the token: signed out.");
    signOut();
    state.authStatus = Status::warning("Your session expired. Sign in again.");
}

void StemhubSession::restoreLink(ProjectLink savedLink)
{
    JUCE_ASSERT_MESSAGE_THREAD
    // Hosts may hand the saved state back later (undo, presets); the project open here wins.
    if (state.selectedProject.has_value())
        return;

    // An empty state doesn't undo a link taken from a restore hand-off.
    if (savedLink.isSet())
        state.link = std::move(savedLink);

    takeRestoreHandoff(state.link.projectId);
    openLinkedProject();
    changed();
}

void StemhubSession::takeRestoreHandoff(const juce::String& projectId)
{
    const auto handoff = stemhub::handoff::take(storage.restoreHandoffFile, projectId);
    if (!handoff.has_value())
        return;

    // The DAW has just opened this restored copy as the project this instance lives in. Which
    // version it holds was recorded when it was restored.
    state.link = { handoff->projectId, handoff->branchId, handoff->file };
}

void StemhubSession::openLinkedProject()
{
    if (!state.link.isSet() || !state.isSignedIn() || state.selectedProject.has_value() || isBusy())
        return;

    const auto isListed = std::any_of(state.projects.begin(), state.projects.end(), [this](const Project& project)
    {
        return project.id == state.link.projectId;
    });
    if (!isListed)
    {
        state.projectsStatus = Status::warning("This DAW project is linked to a StemHub project you can no longer open. "
                                               "Choose another project.");
        return;
    }

    usecases::OpenProjectInput input;
    input.projectId = state.link.projectId;
    input.preferredBranchId = state.link.branchId;
    // Passed even when it is missing (a drive not plugged in), so the link keeps the path.
    input.localProjectFile = state.link.workingFile;
    input.availableProjects = state.projects;
    input.workingCopies = storage.workingCopies;
    input.restoredProjectsFolder = storage.restoredProjectsFolder;

    start(OperationState::loadingProjects, "Opening the linked project...",
          asSignedIn([input](const SignedInApi& backend, const ReportProgress& report) -> JobPayload
          {
              return usecases::openProject(backend, input, report);
          }));
}

//==============================================================================
// Projects and branches

void StemhubSession::chooseProjectFile(const juce::File& file)
{
    JUCE_ASSERT_MESSAGE_THREAD
    state.chosenProjectFile = file;
    changed();
}

void StemhubSession::requestOpenProject(juce::String projectId, const bool restoreLatestIfSafe)
{
    JUCE_ASSERT_MESSAGE_THREAD
    if (isBusy())
        return;

    // A project keeps its branch and file only when it is the one open here or the one this DAW
    // project is linked to: another project's file is not a copy of this one.
    const auto isOpenHere = state.selectedProject.has_value() && state.selectedProject->id == projectId;
    const auto isLinked = state.link.projectId == projectId;

    usecases::OpenProjectInput input;
    input.projectId = std::move(projectId);
    input.preferredBranchId = isOpenHere ? state.selectedBranchId
                            : isLinked   ? state.link.branchId
                                         : juce::String();
    input.localProjectFile = state.chosenProjectFile != juce::File() ? state.chosenProjectFile
                           : isOpenHere                              ? state.workingFile
                           : isLinked                                ? state.link.workingFile
                                                                     : juce::File();
    input.availableProjects = state.projects;
    input.restoreLatestIfSafe = restoreLatestIfSafe;
    input.workingCopies = storage.workingCopies;
    input.restoredProjectsFolder = storage.restoredProjectsFolder;

    start(OperationState::loadingProjects, "Opening project...",
          asSignedIn([input](const SignedInApi& backend, const ReportProgress& report) -> JobPayload
          {
              return usecases::openProject(backend, input, report);
          }));
}

void StemhubSession::requestCreateProject()
{
    JUCE_ASSERT_MESSAGE_THREAD
    if (isBusy())
        return;

    start(OperationState::loadingProjects, "Creating project...",
          asSignedIn([input = usecases::CreateProjectInput { getProjectFileForGrid() }](const SignedInApi& backend, const auto&)
                         -> JobPayload
          {
              return usecases::createProject(backend, input);
          }));
}

void StemhubSession::apply(ProjectActivationJobResult result)
{
    // A project created before a later step failed still belongs in the grid, or it would be
    // created again.
    if (result.refreshedProjects.has_value())
    {
        state.projects = std::move(*result.refreshedProjects);
    }
    else if (result.selectedProject.has_value())
    {
        const auto& project = *result.selectedProject;
        if (std::none_of(state.projects.begin(), state.projects.end(), [&project](const Project& listed) { return listed.id == project.id; }))
            state.projects.push_back(project);
    }

    if (!finish(result))
        return;

    // The project grid's progress message is done with; the dashboard reports the outcome.
    state.projectsStatus = {};
    state.branches = std::move(result.branches);
    state.versionHistory = std::move(result.versions);
    state.selectedVersionId = std::move(result.selectedVersionId);
    enterProject(*result.selectedProject, std::move(result.branchId), std::move(result.projectFile));
    state.workingCopy = std::move(result.workingCopy);
    // The DAW has the working file open; it holds a known version only while the file is unchanged.
    state.openedVersionId = state.workingCopy.isUnchanged() ? state.workingCopy.versionId : juce::String();
    state.sessionStatus = std::move(result.status);

    // Only a copy that was just restored is opened in the DAW. A file the user linked is usually
    // the project the DAW already has open, and reopening it would reload the session.
    if (result.restoredCopy.isSet())
        handOverRestoredCopy(result.restoredCopy);
}

void StemhubSession::requestSelectBranch(juce::String branchId)
{
    JUCE_ASSERT_MESSAGE_THREAD
    if (isBusy())
        return;

    if (!state.selectedProject.has_value())
    {
        refuse(Status::error("Choose a project before selecting a workspace."));
        return;
    }

    const auto branchIt = std::find_if(state.branches.begin(), state.branches.end(), [&branchId](const Branch& branch)
    {
        return branch.id == branchId;
    });

    if (branchIt == state.branches.end())
    {
        refuse(Status::error("Selected workspace is no longer available."));
        return;
    }

    usecases::FetchHistoryInput input;
    input.branchId = std::move(branchId);
    input.branchName = branchIt->name;

    start(OperationState::pulling, "Loading workspace history...",
          asSignedIn([input](const SignedInApi& backend, const auto&) -> JobPayload
          {
              return usecases::fetchHistory(backend, input);
          }));
}

void StemhubSession::requestRefreshVersionHistory()
{
    JUCE_ASSERT_MESSAGE_THREAD
    if (isBusy())
        return;

    if (!state.hasOpenProject())
    {
        refuse(Status::error("Choose a project and workspace before refreshing history."));
        return;
    }

    const auto* branch = state.selectedBranch();

    usecases::FetchHistoryInput input;
    input.branchId = state.selectedBranchId;
    input.branchName = branch != nullptr ? branch->name : juce::String();
    input.preferredVersionId = state.selectedVersionId;

    start(OperationState::pulling, "Refreshing version history...",
          asSignedIn([input](const SignedInApi& backend, const auto&) -> JobPayload
          {
              return usecases::fetchHistory(backend, input);
          }));
}

void StemhubSession::apply(BranchHistoryJobResult result)
{
    if (!finish(result))
        return;

    // The backend only accepts a parent from the same branch. Refreshing the same branch leaves
    // the files on disk, and so the working copy, as they are.
    if (result.branchId != state.selectedBranchId)
        clearWorkingCopy();

    state.selectedBranchId = std::move(result.branchId);
    state.versionHistory = std::move(result.versions);
    state.selectedVersionId = std::move(result.selectedVersionId);
    state.sessionStatus = std::move(result.status);
}

//==============================================================================
// Saving and restoring

void StemhubSession::requestPushVersion(juce::String commitMessage)
{
    JUCE_ASSERT_MESSAGE_THREAD
    if (isBusy())
        return;

    if (hasCleanWorkingCopy())
    {
        refuse(Status::warning("No changes to save: the project file is the same as the last saved "
                               "or restored version. Save the project in your DAW first."));
        return;
    }

    usecases::PushInput input;
    input.projectFile = state.workingFile;
    input.projectId = state.selectedProject.has_value() ? state.selectedProject->id : juce::String();
    input.branchId = state.selectedBranchId;
    input.parentVersionId = getParentVersionForNextSave();
    input.commitMessage = std::move(commitMessage);
    input.workingCopies = storage.workingCopies;

    start(OperationState::committing, "Saving version...",
          asSignedIn([input](const SignedInApi& backend, const ReportProgress& report) -> JobPayload
          {
              return usecases::pushVersion(backend, input, report);
          }));
}

void StemhubSession::apply(PushVersionJobResult result)
{
    if (!finish(result))
        return;

    if (result.refreshedVersions.has_value())
        state.versionHistory = std::move(*result.refreshedVersions);

    if (result.pushedCopy.isSet())
    {
        // The pushed file now holds the new version: it is the next save's parent, and what the
        // "no changes" check compares against.
        state.workingCopy = result.pushedCopy;
        state.selectedVersionId = state.workingCopy.versionId;
        state.openedVersionId = state.workingCopy.versionId;
        state.lastSavedVersionId = state.workingCopy.versionId;
    }

    state.sessionStatus = std::move(result.status);
}

void StemhubSession::requestRestoreVersion(const juce::String& versionId, const juce::File& projectFolder)
{
    JUCE_ASSERT_MESSAGE_THREAD
    if (isBusy())
        return;

    if (!state.hasOpenProject())
    {
        refuse(Status::error("Choose a project before restoring."));
        return;
    }
    if (!projectFolder.isDirectory())
    {
        refuse(Status::error("Choose a valid restore destination folder."));
        return;
    }
    if (versionId.isEmpty())
    {
        refuse(Status::error("Select a version before restoring."));
        return;
    }

    namespace restorefolders = stemhub::restorefolders;

    usecases::RestoreInput input;
    input.projectId = state.selectedProject->id;
    input.versionId = versionId;
    input.branchId = state.selectedBranchId;
    input.destinationFolder = restorefolders::newFolder(projectFolder,
                                                        restorefolders::projectName(state.versionHistory, versionId, state.selectedProject->name),
                                                        versionId);
    input.workingCopies = storage.workingCopies;

    start(OperationState::restoring, "Restoring version...",
          asSignedIn([input](const SignedInApi& backend, const ReportProgress& report) -> JobPayload
          {
              return usecases::restoreVersion(backend, input, report);
          }));
}

void StemhubSession::apply(RestoreVersionJobResult result)
{
    if (!finish(result))
        return;

    state.selectedVersionId = result.restoredCopy.versionId;
    state.sessionStatus = std::move(result.status);
    handOverRestoredCopy(result.restoredCopy);
}

void StemhubSession::handOverRestoredCopy(const WorkingCopyBaseline& restoredCopy)
{
    // The DAW opens the copy as a project of its own, and the instance loaded with it takes over
    // from there. This instance stays with the file of the DAW project it lives in.
    stemhub::handoff::write(storage.restoreHandoffFile,
                            { state.selectedProject->id, state.selectedBranchId, restoredCopy.file, juce::Time::getCurrentTime() });

    if (!openFileHandler(restoredCopy.file))
        state.sessionStatus = Status::warning("Restored to " + restoredCopy.file.getFullPathName()
                                              + ", but it could not be opened automatically. Open it from your DAW.");
}

//==============================================================================
// Selection and working copy

void StemhubSession::setSelectedVersionId(juce::String versionId)
{
    JUCE_ASSERT_MESSAGE_THREAD
    state.selectedVersionId = std::move(versionId);
    changed();
}

void StemhubSession::setWorkingFile(const juce::File& file)
{
    JUCE_ASSERT_MESSAGE_THREAD
    state.workingFile = file;
    changed();
}

void StemhubSession::showProjectSelection()
{
    JUCE_ASSERT_MESSAGE_THREAD
    // A save or restore in flight belongs to this project; leaving would mix its result into another.
    if (isWriteOperationInProgress() || !state.isSignedIn())
        return;

    state.uiState = UIState::projectSelection;
    changed();
}

juce::File StemhubSession::getProjectFileForGrid() const
{
    if (state.chosenProjectFile.existsAsFile())
        return state.chosenProjectFile;

    return state.workingFile.existsAsFile() ? state.workingFile : juce::File();
}

bool StemhubSession::isWriteOperationInProgress() const noexcept
{
    return state.operationState == OperationState::committing
        || state.operationState == OperationState::restoring;
}

void StemhubSession::enterProject(Project project, juce::String branchId, juce::File workingFile)
{
    state.selectedProject = std::move(project);
    state.selectedBranchId = std::move(branchId);
    state.workingFile = std::move(workingFile);
    // The file chosen on the grid went with this project.
    state.chosenProjectFile = juce::File();
    state.uiState = UIState::dashboard;
}

void StemhubSession::refreshLink()
{
    if (!state.selectedProject.has_value())
        return;

    state.link = { state.selectedProject->id, state.selectedBranchId, state.workingFile };
}

void StemhubSession::clearWorkingCopy()
{
    state.workingCopy = {};
    state.openedVersionId.clear();
}

bool StemhubSession::hasCleanWorkingCopy() const
{
    return state.workingCopy.describes(state.workingFile) && state.workingCopy.isUnchanged();
}

juce::String StemhubSession::getParentVersionForNextSave() const
{
    if (state.workingCopy.describes(state.workingFile))
        return state.workingCopy.versionId;

    return state.versionHistory.empty() ? juce::String() : state.versionHistory.front().id;
}
