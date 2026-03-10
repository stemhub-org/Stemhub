#include <algorithm>
#include <type_traits>

#include "application/PluginProcessor.hpp"
#include "application/PluginProcessorHelpers.hpp"
#include "application/ProjectFileService.hpp"
#include "application/SessionCache.hpp"

using namespace stemhub::processorhelpers;

void StemhubAudioProcessor::applyAuthRequestResult(AuthRequestResult result)
{
    const auto fromCachedSession = result.fromCachedSession;

    if (result.authErrorMessage.isNotEmpty())
    {
        if (fromCachedSession)
        {
            stemhub::sessioncache::clear();
            currentUser.reset();
            access_tkn.clear();
            versionControlService.clearAccessToken();
            authErrorMessage.clear();
            sessionState = {};
            sendChangeMessage();
            return;
        }

        authErrorMessage = result.authErrorMessage;
        setAuthState(AuthState::authError);
        return;
    }

    authErrorMessage.clear();
    projectSelectionStatusMessage = std::move(result.projectSelectionStatusMessage);
    activeProjectStatusMessage.clear();
    projects = std::move(result.projects);
    access_tkn = std::move(result.token);
    stemhub::sessioncache::saveAccessToken(access_tkn);
    versionControlService.setAccessToken(access_tkn);
    signIn(std::move(*result.user));

    if (fromCachedSession)
        requestRestoreCachedProjectContext();
}

void StemhubAudioProcessor::applyProjectActivationResult(ProjectActivationJobResult result)
{
    if (hasError(result))
    {
        setOperationState(OperationState::error);
        projectSelectionStatusMessage = result.errorMessage;
        return;
    }

    if (result.refreshProjects)
    {
        const auto count = static_cast<int>(result.projects.size());
        projects = std::move(result.projects);
        projectSelectionStatusMessage = count == 0 ? "No projects found."
                                                   : "Loaded " + juce::String(count) + " project(s).";
    }

    branches = std::move(result.branches);
    versionHistory = std::move(result.versions);
    selectedVersionId = chooseSelectedVersionId(versionHistory, result.selectedVersionId);
    selectProject(*result.selectedProject, result.branchId, result.branchName,
            std::move(result.projectFile));

    if (result.shouldAutoOpenLocalFile
        && selectedProjectFile.existsAsFile()
        && !stemhub::projectfiles::openInSystem(selectedProjectFile))
    {
        setOperationState(OperationState::error);
        activeProjectStatusMessage = "Project loaded, but failed to open local project file: "
            + selectedProjectFile.getFullPathName();
        return;
    }

    setOperationState(OperationState::idle);
    activeProjectStatusMessage = std::move(result.activeProjectStatusMessage);
}

void StemhubAudioProcessor::applyBranchHistoryResult(BranchHistoryJobResult result)
{
    if (hasError(result))
    {
        setOperationState(OperationState::error);
        activeProjectStatusMessage = result.errorMessage;
        return;
    }

    selectedBranchId = std::move(result.branchId);
    selectedBranchName = std::move(result.branchName);
    versionHistory = std::move(result.versions);
    selectedVersionId = chooseSelectedVersionId(versionHistory, result.selectedVersionId);
    versionControlService.setCurrentProjectContext(makeProjectVersionContext(selectedProject,
                                                                             selectedBranchId,
                                                                             versionHistory));
    if (result.projectFile.existsAsFile())
    {
        selectedProjectFile = result.projectFile;
        pendingProjectFile = selectedProjectFile;

        if (!stemhub::projectfiles::openInSystem(selectedProjectFile))
        {
            setOperationState(OperationState::error);
            activeProjectStatusMessage = "Branch loaded, but failed to open local project file: "
                + selectedProjectFile.getFullPathName();
            return;
        }
    }

    setOperationState(OperationState::idle);
    activeProjectStatusMessage = std::move(result.activeProjectStatusMessage);
}

void StemhubAudioProcessor::applyPushVersionResult(PushVersionJobResult result)
{
    if (hasError(result))
    {
        setOperationState(OperationState::error);
        activeProjectStatusMessage = result.errorMessage;
        return;
    }

    versionControlService.setLastVersionId(std::move(result.pushedVersionId));
    setOperationState(OperationState::idle);
    activeProjectStatusMessage = result.activeProjectStatusMessage.isNotEmpty()
        ? result.activeProjectStatusMessage
        : "Version pushed successfully.";
}

void StemhubAudioProcessor::applyRestoreVersionResult(RestoreVersionJobResult result)
{
    if (hasError(result))
    {
        setOperationState(OperationState::error);
        activeProjectStatusMessage = result.errorMessage;
        return;
    }

    if (result.restoredProjectFile.existsAsFile())
    {
        selectedProjectFile = result.restoredProjectFile;
        pendingProjectFile = selectedProjectFile;
        if (!stemhub::projectfiles::openInSystem(selectedProjectFile))
        {
            setOperationState(OperationState::error);
            activeProjectStatusMessage = "Version restored, but failed to open project file: "
                + selectedProjectFile.getFullPathName();
            return;
        }
    }

    setOperationState(OperationState::idle);
    activeProjectStatusMessage = result.activeProjectStatusMessage;
}

void StemhubAudioProcessor::applyBackgroundResult(BackgroundJobResult result)
{
    std::visit([this](auto&& payload)
    {
        using Payload = std::decay_t<decltype(payload)>;

        if constexpr (std::is_same_v<Payload, AuthRequestResult>)
            applyAuthRequestResult(std::move(payload));
        else if constexpr (std::is_same_v<Payload, ProjectActivationJobResult>)
            applyProjectActivationResult(std::move(payload));
        else if constexpr (std::is_same_v<Payload, BranchHistoryJobResult>)
            applyBranchHistoryResult(std::move(payload));
        else if constexpr (std::is_same_v<Payload, PushVersionJobResult>)
            applyPushVersionResult(std::move(payload));
        else if constexpr (std::is_same_v<Payload, RestoreVersionJobResult>)
            applyRestoreVersionResult(std::move(payload));
    }, std::move(result.payload));
}

void StemhubAudioProcessor::signIn(User newUser) noexcept
{
    currentUser = std::move(newUser);
    sessionState.authState = AuthState::signedIn;
    sessionState.uiState = UIState::projectSelection;
    sessionState.operationState = OperationState::idle;
}

void StemhubAudioProcessor::signOut() noexcept
{
    backgroundJobs.invalidateSession();
    currentUser.reset();
    access_tkn.clear();
    versionControlService.clearAccessToken();
    stemhub::sessioncache::clear();
    authErrorMessage.clear();
    projectSelectionStatusMessage.clear();
    activeProjectStatusMessage.clear();
    projects.clear();
    branches.clear();
    versionHistory.clear();
    selectedVersionId.clear();
    clearSelectedProject();
    pendingProjectFile = juce::File();
    selectedProjectFile = juce::File();
    sessionState = {};
    sendChangeMessage();
}

void StemhubAudioProcessor::setAuthState(AuthState newAuthState) noexcept
{
    sessionState.authState = newAuthState;

    if (newAuthState != AuthState::signedIn)
    {
        sessionState.uiState = UIState::login;
        sessionState.operationState = OperationState::idle;
    }
}

void StemhubAudioProcessor::setUIState(UIState newUIState) noexcept
{
    sessionState.uiState = sessionState.authState == AuthState::signedIn ? newUIState : UIState::login;
}

void StemhubAudioProcessor::setOperationState(OperationState newOperationState) noexcept
{
    sessionState.operationState = sessionState.authState == AuthState::signedIn ? newOperationState
                                                                                : OperationState::idle;
}

void StemhubAudioProcessor::setProjectSelectionStatusMessage(juce::String message)
{
    projectSelectionStatusMessage = std::move(message);
    sendChangeMessage();
}

void StemhubAudioProcessor::setActiveProjectStatusMessage(juce::String message)
{
    activeProjectStatusMessage = std::move(message);
    sendChangeMessage();
}

void StemhubAudioProcessor::setPendingProjectFile(const juce::File& file)
{
    pendingProjectFile = file;
    sendChangeMessage();
}

void StemhubAudioProcessor::selectProject(Project project, juce::String branchId, juce::String branchName, juce::File projectFile)
{
    versionControlService.clearProjectContext();
    selectedProject = std::move(project);
    selectedBranchId = std::move(branchId);
    selectedBranchName = std::move(branchName);
    selectedProjectFile = std::move(projectFile);
    pendingProjectFile = selectedProjectFile;
    if (selectedProject.has_value())
        stemhub::sessioncache::saveProjectId(selectedProject->id);
    versionControlService.setCurrentProjectContext(makeProjectVersionContext(selectedProject,
                                                                             selectedBranchId,
                                                                             versionHistory));
    sessionState.uiState = UIState::dashboard;
    sendChangeMessage();
}

void StemhubAudioProcessor::clearSelectedProject() noexcept
{
    selectedProject.reset();
    selectedBranchId.clear();
    selectedBranchName.clear();
    selectedVersionId.clear();
    selectedProjectFile = juce::File();
    branches.clear();
    versionHistory.clear();
    versionControlService.clearProjectContext();
    activeProjectStatusMessage.clear();
}

void StemhubAudioProcessor::requestSignIn(const juce::String& email, const juce::String& password)
{
    if (sessionState.authState == AuthState::signingIn)
        return;

    setAuthState(AuthState::signingIn);
    authErrorMessage.clear();
    projectSelectionStatusMessage.clear();
    activeProjectStatusMessage.clear();
    projects.clear();
    branches.clear();
    versionHistory.clear();
    selectedVersionId.clear();
    sendChangeMessage();

    enqueueBackgroundTask([this, email, password]() -> BackgroundJobPayload
    {
        return performSignInRequest(email, password);
    });
}

void StemhubAudioProcessor::requestRestoreCachedSession()
{
    if (didAttemptCachedSessionRestore)
        return;

    didAttemptCachedSessionRestore = true;
    if (sessionState.authState == AuthState::signedIn || sessionState.authState == AuthState::signingIn)
        return;

    const auto cachedToken = stemhub::sessioncache::loadAccessToken().trim();
    if (cachedToken.isEmpty())
        return;

    setAuthState(AuthState::signingIn);
    authErrorMessage.clear();
    projectSelectionStatusMessage = "Restoring session...";
    activeProjectStatusMessage.clear();
    sendChangeMessage();

    enqueueBackgroundTask([this, token = cachedToken]() -> BackgroundJobPayload
    {
        return performRestoreCachedSessionRequest(token);
    });
}

void StemhubAudioProcessor::requestRestoreCachedProjectContext()
{
    if (selectedProject.has_value() || access_tkn.isEmpty() || projects.empty())
        return;

    const auto cachedProjectId = stemhub::sessioncache::loadProjectId().trim();
    if (cachedProjectId.isEmpty())
        return;

    const auto projectIt = std::find_if(projects.begin(), projects.end(), [&cachedProjectId](const Project& project)
    {
        return project.id == cachedProjectId;
    });
    if (projectIt == projects.end())
        return;

    setOperationState(OperationState::loadingProjects);
    projectSelectionStatusMessage = "Restoring last opened project...";
    sendChangeMessage();

    const auto projectsSnapshot = projects;
    const auto token = access_tkn;
    enqueueBackgroundTask([this, cachedProjectId, projectsSnapshot, token]() -> BackgroundJobPayload
    {
        auto result = performOpenProjectRequest(cachedProjectId, {}, projectsSnapshot, token);
        result.shouldAutoOpenLocalFile = false;
        return result;
    });
}

void StemhubAudioProcessor::requestOpenProject(juce::String projectId, juce::File localProjectFile)
{
    setOperationState(OperationState::loadingProjects);
    sendChangeMessage();

    const auto projectsSnapshot = projects;
    const auto token = access_tkn;
    enqueueBackgroundTask([this,
                           requestedProjectId = std::move(projectId),
                           requestedProjectFile = std::move(localProjectFile),
                           projectsSnapshot,
                           token]() -> BackgroundJobPayload
    {
        return performOpenProjectRequest(requestedProjectId, requestedProjectFile, projectsSnapshot, token);
    });
}

void StemhubAudioProcessor::requestCreateProject(juce::File localProjectFile)
{
    setOperationState(OperationState::loadingProjects);
    sendChangeMessage();

    const auto token = access_tkn;
    enqueueBackgroundTask([this,
                           requestedProjectFile = std::move(localProjectFile),
                           token]() -> BackgroundJobPayload
    {
        return performCreateProjectRequest(requestedProjectFile, token);
    });
}

void StemhubAudioProcessor::requestSelectBranch(juce::String branchId)
{
    if (!selectedProject.has_value())
    {
        setOperationState(OperationState::error);
        setActiveProjectStatusMessage("Choose a project before selecting a branch.");
        return;
    }

    const auto branchIt = std::find_if(branches.begin(), branches.end(), [&branchId](const Branch& branch)
    {
        return branch.id == branchId;
    });

    if (branchIt == branches.end())
    {
        setOperationState(OperationState::error);
        setActiveProjectStatusMessage("Selected branch is no longer available.");
        return;
    }

    setOperationState(OperationState::pulling);
    setActiveProjectStatusMessage("Loading branch history...");

    const auto token = access_tkn;
    const auto branchName = branchIt->name;
    enqueueBackgroundTask([this,
                           requestedBranchId = std::move(branchId),
                           requestedBranchName = std::move(branchName),
                           token]() -> BackgroundJobPayload
    {
        return performFetchBranchHistoryRequest(requestedBranchId, requestedBranchName, {}, token);
    });
}

void StemhubAudioProcessor::requestRefreshVersionHistory()
{
    if (!hasProjectAndBranchSelected(selectedProject, selectedBranchId))
    {
        setOperationState(OperationState::error);
        setActiveProjectStatusMessage("Choose a project and branch before refreshing history.");
        return;
    }

    const auto branchNameIt = std::find_if(branches.begin(), branches.end(), [this](const Branch& branch)
    {
        return branch.id == selectedBranchId;
    });
    const auto branchName = branchNameIt != branches.end() ? branchNameIt->name : selectedBranchName;

    setOperationState(OperationState::pulling);
    setActiveProjectStatusMessage("Refreshing version history...");

    const auto token = access_tkn;
    const auto branchId = selectedBranchId;
    const auto preferredVersionId = selectedVersionId;
    enqueueBackgroundTask([this,
                           branchId,
                           branchName,
                           preferredVersionId,
                           token]() -> BackgroundJobPayload
    {
        return performFetchBranchHistoryRequest(branchId, branchName, preferredVersionId, token);
    });
}

void StemhubAudioProcessor::requestPushVersion(juce::String commitMessage, juce::String dawName)
{
    setOperationState(OperationState::committing);
    sendChangeMessage();

    const auto projectFile = stemhub::projectfiles::resolveEffectiveProjectFile(selectedProjectFile, pendingProjectFile);
    const auto projectRootDirectory = projectFile.existsAsFile() ? projectFile.getParentDirectory() : juce::File();
    const auto project = selectedProject;
    const auto branchId = selectedBranchId;
    enqueueBackgroundTask([this,
                           selectedFile = std::move(projectFile),
                           selectedProjectRoot = std::move(projectRootDirectory),
                           project,
                           selectedBranch = std::move(branchId),
                           requestedCommitMessage = std::move(commitMessage),
                           requestedDawName = std::move(dawName)]() mutable -> BackgroundJobPayload
    {
        return performPushVersionRequest(selectedFile,
                                         selectedProjectRoot,
                                         project,
                                         selectedBranch,
                                         requestedCommitMessage,
                                         requestedDawName);
    });
}

void StemhubAudioProcessor::requestRestoreVersion(const juce::String& versionId, const juce::File& projectFolder)
{
    if (!hasProjectAndBranchSelected(selectedProject, selectedBranchId))
    {
        setOperationState(OperationState::error);
        setActiveProjectStatusMessage("Choose a project before restoring.");
        return;
    }

    if (!projectFolder.isDirectory())
    {
        setOperationState(OperationState::error);
        setActiveProjectStatusMessage("Choose a valid restore destination folder.");
        return;
    }

    if (versionId.isEmpty())
    {
        setOperationState(OperationState::error);
        setActiveProjectStatusMessage("Select a version before restoring.");
        return;
    }

    const auto projectName = selectedProject ? selectedProject->name : juce::String();
    const auto restoredProjectBase = stemhub::projectfiles::resolveRestoreProjectName(versionHistory, versionId, projectName);
    const auto fileName = restoredProjectBase + "-" + versionId.substring(0, juce::jmin(8, versionId.length())) + ".zip";
    const auto restoreFile = projectFolder.getChildFile(fileName);

    setOperationState(OperationState::pulling);
    setActiveProjectStatusMessage("Restoring selected version...");
    sendChangeMessage();

    const auto requestedVersionId = versionId;
    const auto requestedRestoreFile = restoreFile;
    enqueueBackgroundTask([this,
                           requestedVersionId,
                           requestedRestoreFile]() -> BackgroundJobPayload
    {
        return performRestoreVersionRequest(requestedVersionId, requestedRestoreFile);
    });
}

void StemhubAudioProcessor::setSelectedVersionId(juce::String versionId)
{
    selectedVersionId = std::move(versionId);
    sendChangeMessage();
}

void StemhubAudioProcessor::handleAsyncUpdate()
{
    const auto didApply = backgroundJobs.flushResults([this](auto&& result)
    {
        applyBackgroundResult(std::forward<decltype(result)>(result));
    });

    if (didApply)
        sendChangeMessage();
}

