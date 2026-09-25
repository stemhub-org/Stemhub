#include <algorithm>

#include "application/StemhubSession.hpp"
#include "application/SessionHelpers.hpp"
#include "application/ProjectFileService.hpp"
#include "application/SessionCache.hpp"

using namespace stemhub::sessionhelpers;

namespace usecases = stemhub::usecases;

StemhubSession::StemhubSession(std::shared_ptr<const IProjectApi> apiToUse)
    : api(std::move(apiToUse)),
      openFileHandler([](const juce::File& file) { return stemhub::projectfiles::openInSystem(file); }),
      managedWorkingCopyFolder(stemhub::projectfiles::getDefaultManagedWorkingCopyFolder())
{
    jassert(api != nullptr);
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

void StemhubSession::enqueue(std::function<JobPayload(const IProjectApi&)> run)
{
    // The job holds its own reference to the API, so it never reaches into this session.
    jobs.enqueue([sharedApi = api, run = std::move(run)]
    {
        return run(*sharedApi);
    });
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
    auto results = jobs.takeResults();

    bool didApply = false;
    for (auto& payload : results)
        didApply = applyResult(std::move(payload)) || didApply;

    if (didApply)
        changed();

    return static_cast<int>(results.size());
}

bool StemhubSession::applyResult(JobPayload payload)
{
    return std::visit([this](auto&& result)
    {
        // The one staleness rule: a result counts only if nothing was requested after it.
        // Checked per result, as applying one can start a request or end the session.
        if (!isCurrent(result.requestEpoch))
            return false;

        apply(std::move(result));
        return true;
    }, std::move(payload));
}

//==============================================================================
// Signing in and out

void StemhubSession::requestSignIn(const juce::String& email, const juce::String& password)
{
    JUCE_ASSERT_MESSAGE_THREAD
    if (state.authState == AuthState::signingIn)
        return;

    state = {};
    state.authState = AuthState::signingIn;
    state.authStatus = Status::progress("Signing in to your StemHub account...");
    changed();

    enqueue([input = usecases::SignInInput { email, password }, epoch = beginRequest()](const IProjectApi& backend)
                -> JobPayload
    {
        auto result = usecases::signIn(backend, input);
        result.requestEpoch = epoch;
        return result;
    });
}

void StemhubSession::requestRestoreCachedSession()
{
    JUCE_ASSERT_MESSAGE_THREAD
    if (didAttemptCachedSessionRestore)
        return;

    didAttemptCachedSessionRestore = true;
    if (state.authState == AuthState::signedIn || state.authState == AuthState::signingIn)
        return;

    const auto cachedToken = stemhub::sessioncache::loadAccessToken().trim();
    if (cachedToken.isEmpty())
        return;

    state.authState = AuthState::signingIn;
    state.authStatus = Status::progress("Restoring your session...");
    changed();

    enqueue([input = usecases::RestoreSessionInput { cachedToken }, epoch = beginRequest()](const IProjectApi& backend)
                -> JobPayload
    {
        auto result = usecases::restoreSession(backend, input);
        result.requestEpoch = epoch;
        return result;
    });
}

void StemhubSession::apply(AuthRequestResult result)
{
    if (result.authErrorMessage.isNotEmpty())
    {
        if (result.fromCachedSession && result.sessionExpired)
        {
            expireSession("Saved session expired. Please sign in again.");
            return;
        }

        // Signing in failed, or the saved session couldn't be checked (offline, server down).
        // In that second case the token stays saved, and reopening the plugin tries again.
        if (result.fromCachedSession)
            didAttemptCachedSessionRestore = false;

        state.authState = AuthState::authError;
        state.authStatus = Status::error(result.authErrorMessage);
        return;
    }

    state.authState = AuthState::signedIn;
    state.uiState = UIState::projectSelection;
    state.authStatus = {};
    state.accessToken = std::move(result.token);
    state.currentUser = std::move(result.user);
    state.projects = std::move(result.projects);
    state.projectsStatus = std::move(result.projectsStatus);
    stemhub::sessioncache::saveAccessToken(state.accessToken);

    if (result.fromCachedSession)
        requestRestoreCachedProjectContext();
}

void StemhubSession::signOut()
{
    JUCE_ASSERT_MESSAGE_THREAD
    // Whatever is still running belongs to the old session: its result will be dropped.
    beginRequest();
    stemhub::sessioncache::clear();
    state = {};
    changed();
}

void StemhubSession::expireSession(const juce::String& message)
{
    signOut();
    state.authState = AuthState::authError;
    state.authStatus = Status::warning(message);
}

void StemhubSession::requestRestoreCachedProjectContext()
{
    if (state.selectedProject.has_value() || state.accessToken.isEmpty() || state.projects.empty())
        return;

    const auto cachedProjectId = stemhub::sessioncache::loadProjectId().trim();
    if (cachedProjectId.isEmpty())
        return;

    const auto isListed = std::any_of(state.projects.begin(), state.projects.end(), [&cachedProjectId](const Project& project)
    {
        return project.id == cachedProjectId;
    });
    if (!isListed)
    {
        stemhub::sessioncache::clearProjectContext();
        state.projectsStatus = Status::warning("Last opened project is no longer available. Choose another project.");
        return;
    }

    const auto cachedProjectFilePath = stemhub::sessioncache::loadLastOpenedProjectFilePath().trim();
    const auto cachedProjectFile = cachedProjectFilePath.isNotEmpty() ? juce::File(cachedProjectFilePath) : juce::File();
    const auto hasUsableCachedProjectFile = cachedProjectFile.existsAsFile();
    if (cachedProjectFilePath.isNotEmpty() && !hasUsableCachedProjectFile)
        stemhub::sessioncache::clearLastOpenedProjectFilePath();

    state.operationState = OperationState::loadingProjects;
    state.projectsStatus = Status::progress("Restoring last opened project...");

    usecases::OpenProjectInput input;
    input.projectId = cachedProjectId;
    input.localProjectFile = hasUsableCachedProjectFile ? cachedProjectFile : juce::File();
    input.availableProjects = state.projects;
    input.token = state.accessToken;

    enqueue([input, epoch = beginRequest()](const IProjectApi& backend) -> JobPayload
    {
        auto result = usecases::openProject(backend, input);
        result.requestEpoch = epoch;
        result.fromCachedProjectRestore = true;
        return result;
    });
}

//==============================================================================
// Projects and branches

void StemhubSession::requestOpenProject(juce::String projectId, juce::File localProjectFile, const bool restoreLatestIfSafe)
{
    JUCE_ASSERT_MESSAGE_THREAD
    if (isBusy())
        return;

    // The working file of the project open until now is not a copy of another project.
    if (state.selectedProject.has_value() && state.selectedProject->id != projectId
        && localProjectFile == state.selectedProjectFile)
        localProjectFile = juce::File();

    state.operationState = OperationState::loadingProjects;
    state.projectsStatus = Status::progress("Opening project...");
    changed();

    usecases::OpenProjectInput input;
    input.projectId = std::move(projectId);
    input.localProjectFile = std::move(localProjectFile);
    input.availableProjects = state.projects;
    input.token = state.accessToken;
    input.restoreLatestIfSafe = restoreLatestIfSafe;
    input.localCopy = state.workingCopy;
    input.managedWorkingCopyFolder = managedWorkingCopyFolder;

    enqueue([input, epoch = beginRequest()](const IProjectApi& backend) -> JobPayload
    {
        auto result = usecases::openProject(backend, input);
        result.requestEpoch = epoch;
        return result;
    });
}

void StemhubSession::requestCreateProject(juce::File localProjectFile)
{
    JUCE_ASSERT_MESSAGE_THREAD
    if (isBusy())
        return;

    state.operationState = OperationState::loadingProjects;
    state.projectsStatus = Status::progress("Creating project...");
    changed();

    enqueue([input = usecases::CreateProjectInput { std::move(localProjectFile), state.accessToken },
             epoch = beginRequest()](const IProjectApi& backend) -> JobPayload
    {
        auto result = usecases::createProject(backend, input);
        result.requestEpoch = epoch;
        return result;
    });
}

void StemhubSession::apply(ProjectActivationJobResult result)
{
    state.operationState = OperationState::idle;

    if (result.sessionExpired)
    {
        expireSession();
        return;
    }

    if (hasError(result))
    {
        if (result.fromCachedProjectRestore)
            stemhub::sessioncache::clearProjectContext();

        state.projectsStatus = Status::error(result.errorMessage);
        return;
    }

    if (result.refreshProjects)
        state.projects = std::move(result.projects);

    // The project grid's progress message is done with; the dashboard reports the outcome.
    state.projectsStatus = {};
    state.branches = std::move(result.branches);
    state.versionHistory = std::move(result.versions);
    state.selectedVersionId = chooseSelectedVersionId(state.versionHistory, result.selectedVersionId);
    enterProject(*result.selectedProject, result.branchId, result.branchName, std::move(result.projectFile));
    state.workingCopy = std::move(result.workingCopy);
    state.openedVersionId.clear();
    state.sessionStatus = std::move(result.status);

    if (state.selectedProjectFile.existsAsFile())
        stemhub::sessioncache::saveLastOpenedProjectFilePath(state.selectedProjectFile.getFullPathName());

    // Only a copy that was just restored is opened in the DAW. A file the user linked is usually
    // the project the DAW already has open, and reopening it would reload the session.
    if (result.didRestoreLatest)
    {
        if (openFileHandler(state.selectedProjectFile))
            state.openedVersionId = state.workingCopy.versionId;
        else
            state.sessionStatus = Status::warning("Latest version restored to " + state.selectedProjectFile.getFullPathName()
                                                  + ", but it could not be opened automatically. Open it from your DAW.");
    }
}

void StemhubSession::requestSelectBranch(juce::String branchId)
{
    JUCE_ASSERT_MESSAGE_THREAD
    if (isBusy())
        return;

    if (!state.selectedProject.has_value())
    {
        state.sessionStatus = Status::error("Choose a project before selecting a workspace.");
        changed();
        return;
    }

    const auto branchIt = std::find_if(state.branches.begin(), state.branches.end(), [&branchId](const Branch& branch)
    {
        return branch.id == branchId;
    });

    if (branchIt == state.branches.end())
    {
        state.sessionStatus = Status::error("Selected workspace is no longer available.");
        changed();
        return;
    }

    state.operationState = OperationState::pulling;
    state.sessionStatus = Status::progress("Loading workspace history...");
    changed();

    usecases::FetchHistoryInput input;
    input.branchId = std::move(branchId);
    input.branchName = branchIt->name;
    input.token = state.accessToken;
    input.localProjectFile = getEffectiveProjectFile();

    enqueue([input, epoch = beginRequest()](const IProjectApi& backend) -> JobPayload
    {
        auto result = usecases::fetchHistory(backend, input);
        result.requestEpoch = epoch;
        return result;
    });
}

void StemhubSession::requestRefreshVersionHistory()
{
    JUCE_ASSERT_MESSAGE_THREAD
    if (isBusy())
        return;

    if (!hasProjectAndBranchSelected(state.selectedProject, state.selectedBranchId))
    {
        state.sessionStatus = Status::error("Choose a project and workspace before refreshing history.");
        changed();
        return;
    }

    const auto branchIt = std::find_if(state.branches.begin(), state.branches.end(), [this](const Branch& branch)
    {
        return branch.id == state.selectedBranchId;
    });

    state.operationState = OperationState::pulling;
    state.sessionStatus = Status::progress("Refreshing version history...");
    changed();

    usecases::FetchHistoryInput input;
    input.branchId = state.selectedBranchId;
    input.branchName = branchIt != state.branches.end() ? branchIt->name : state.selectedBranchName;
    input.preferredVersionId = state.selectedVersionId;
    input.token = state.accessToken;
    input.localProjectFile = getEffectiveProjectFile();

    enqueue([input, epoch = beginRequest()](const IProjectApi& backend) -> JobPayload
    {
        auto result = usecases::fetchHistory(backend, input);
        result.requestEpoch = epoch;
        return result;
    });
}

void StemhubSession::apply(BranchHistoryJobResult result)
{
    state.operationState = OperationState::idle;

    if (result.sessionExpired)
    {
        expireSession();
        return;
    }

    if (hasError(result))
    {
        state.sessionStatus = Status::error(result.errorMessage);
        return;
    }

    // The backend only accepts a parent from the same branch. Refreshing the same branch leaves
    // the files on disk, and so the working copy, as they are.
    if (result.branchId != state.selectedBranchId)
        clearWorkingCopy();

    state.selectedBranchId = std::move(result.branchId);
    state.selectedBranchName = std::move(result.branchName);
    state.versionHistory = std::move(result.versions);
    state.selectedVersionId = chooseSelectedVersionId(state.versionHistory, result.selectedVersionId);
    state.sessionStatus = std::move(result.status);
}

//==============================================================================
// Saving and restoring

void StemhubSession::requestPushVersion(juce::String commitMessage, juce::String dawName)
{
    JUCE_ASSERT_MESSAGE_THREAD
    if (isBusy())
        return;

    const auto projectFile = getEffectiveProjectFile();
    if (hasCleanWorkingCopy(projectFile))
    {
        state.sessionStatus = Status::warning("No changes to save: the project file is the same as the last saved "
                                              "or restored version. Save the project in your DAW first.");
        changed();
        return;
    }

    state.operationState = OperationState::committing;
    state.sessionStatus = Status::progress("Saving version...");
    changed();

    usecases::PushInput input;
    input.projectFile = projectFile;
    input.projectId = state.selectedProject.has_value() ? state.selectedProject->id : juce::String();
    input.branchId = state.selectedBranchId;
    input.parentVersionId = getParentVersionForNextSave(projectFile);
    input.commitMessage = std::move(commitMessage);
    input.dawName = std::move(dawName);
    input.token = state.accessToken;

    enqueue([input, epoch = beginRequest()](const IProjectApi& backend) -> JobPayload
    {
        auto result = usecases::pushVersion(backend, input);
        result.requestEpoch = epoch;
        return result;
    });
}

void StemhubSession::apply(PushVersionJobResult result)
{
    state.operationState = OperationState::idle;

    if (result.sessionExpired)
    {
        expireSession();
        return;
    }

    if (hasError(result))
    {
        state.sessionStatus = Status::error(result.errorMessage);
        return;
    }

    if (result.refreshedVersions.has_value())
        state.versionHistory = std::move(*result.refreshedVersions);

    if (result.pushedCopy.isSet())
    {
        // The pushed file now holds the new version: it is the next save's parent, and what the
        // "no changes" check compares against.
        state.workingCopy = result.pushedCopy;
        state.selectedVersionId = state.workingCopy.versionId;
        state.openedVersionId = state.workingCopy.versionId;
        stemhub::sessioncache::saveLastOpenedProjectFilePath(state.workingCopy.file.getFullPathName());
    }

    state.sessionStatus = std::move(result.status);
}

void StemhubSession::requestRestoreVersion(const juce::String& versionId, const juce::File& projectFolder)
{
    JUCE_ASSERT_MESSAGE_THREAD
    if (isBusy())
        return;

    if (!hasProjectAndBranchSelected(state.selectedProject, state.selectedBranchId))
    {
        state.sessionStatus = Status::error("Choose a project before restoring.");
        changed();
        return;
    }
    if (!projectFolder.isDirectory())
    {
        state.sessionStatus = Status::error("Choose a valid restore destination folder.");
        changed();
        return;
    }
    if (versionId.isEmpty())
    {
        state.sessionStatus = Status::error("Select a version before restoring.");
        changed();
        return;
    }

    state.operationState = OperationState::restoring;
    state.sessionStatus = Status::progress("Restoring version...");
    changed();

    usecases::RestoreInput input;
    input.projectId = state.selectedProject->id;
    input.versionId = versionId;
    input.destinationFolder = stemhub::projectfiles::chooseRestoreFolder(
        projectFolder,
        stemhub::projectfiles::resolveRestoreProjectName(state.versionHistory, versionId, state.selectedProject->name),
        versionId);
    input.token = state.accessToken;

    enqueue([input, epoch = beginRequest()](const IProjectApi& backend) -> JobPayload
    {
        auto result = usecases::restoreVersion(backend, input);
        result.requestEpoch = epoch;
        return result;
    });
}

void StemhubSession::apply(RestoreVersionJobResult result)
{
    state.operationState = OperationState::idle;

    if (result.sessionExpired)
    {
        expireSession();
        return;
    }

    if (hasError(result))
    {
        juce::Logger::writeToLog("[Restore] restore failed: " + result.errorMessage);
        state.sessionStatus = Status::error(result.errorMessage);
        return;
    }

    // The restored copy becomes the working file even if the DAW can't be asked to open it:
    // it is on disk, and the user can open it by hand.
    state.selectedProjectFile = result.restoredProjectFile;
    state.pendingProjectFile = state.selectedProjectFile;
    state.selectedVersionId = result.restoredVersionId;
    state.workingCopy = result.restoredCopy;
    state.sessionStatus = std::move(result.status);
    stemhub::sessioncache::saveLastOpenedProjectFilePath(state.selectedProjectFile.getFullPathName());

    if (openFileHandler(state.selectedProjectFile))
        state.openedVersionId = result.restoredVersionId;
    else
        state.sessionStatus = Status::warning("Version restored to " + state.selectedProjectFile.getFullPathName()
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

void StemhubSession::setPendingProjectFile(const juce::File& file)
{
    JUCE_ASSERT_MESSAGE_THREAD
    state.pendingProjectFile = file;
    changed();
}

void StemhubSession::showProjectSelection()
{
    JUCE_ASSERT_MESSAGE_THREAD
    // A save or restore in flight belongs to this project; leaving would mix its result into another.
    if (isWriteOperationInProgress() || state.authState != AuthState::signedIn)
        return;

    state.uiState = UIState::projectSelection;
    changed();
}

juce::File StemhubSession::getEffectiveProjectFile() const
{
    return stemhub::projectfiles::resolveEffectiveProjectFile(state.selectedProjectFile, state.pendingProjectFile);
}

bool StemhubSession::isWriteOperationInProgress() const noexcept
{
    return state.operationState == OperationState::committing
        || state.operationState == OperationState::restoring;
}

void StemhubSession::enterProject(Project project, juce::String branchId, juce::String branchName, juce::File projectFile)
{
    state.selectedProject = std::move(project);
    state.selectedBranchId = std::move(branchId);
    state.selectedBranchName = std::move(branchName);
    state.selectedProjectFile = std::move(projectFile);
    state.pendingProjectFile = state.selectedProjectFile;
    state.uiState = UIState::dashboard;
    stemhub::sessioncache::saveProjectId(state.selectedProject->id);
}

void StemhubSession::clearWorkingCopy()
{
    state.workingCopy = {};
    state.openedVersionId.clear();
}

bool StemhubSession::hasCleanWorkingCopy(const juce::File& workingFile) const
{
    return state.workingCopy.describes(workingFile) && state.workingCopy.isUnchanged();
}

juce::String StemhubSession::getParentVersionForNextSave(const juce::File& projectFile) const
{
    if (state.workingCopy.describes(projectFile))
        return state.workingCopy.versionId;

    return state.versionHistory.empty() ? juce::String() : state.versionHistory.front().id;
}
