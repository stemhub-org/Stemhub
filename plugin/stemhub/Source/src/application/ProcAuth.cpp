#include <algorithm>

#include "application/PluginProcessor.hpp"
#include "application/SessionCache.hpp"

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

    const auto cachedProjectFilePath = stemhub::sessioncache::loadLastOpenedProjectFilePath().trim();
    const auto cachedProjectFile = juce::File(cachedProjectFilePath);
    const auto localProjectFile = cachedProjectFile.existsAsFile() ? cachedProjectFile : juce::File();
    juce::Logger::writeToLog("[Restore] CachedProjectContext -> projectId="
                             + cachedProjectId
                             + ", cachedProjectFilePath="
                             + cachedProjectFilePath
                             + ", exists="
                             + (cachedProjectFile.existsAsFile() ? "true" : "false"));

    setOperationState(OperationState::loadingProjects);
    projectSelectionStatusMessage = "Restoring last opened project...";
    sendChangeMessage();

    const auto projectsSnapshot = projects;
    const auto token = access_tkn;
    enqueueBackgroundTask([this, cachedProjectId, projectsSnapshot, token, localProjectFile]() -> BackgroundJobPayload
    {
        auto result = performOpenProjectRequest(cachedProjectId, localProjectFile, projectsSnapshot, token, false);
        result.shouldAutoOpenLocalFile = false;
        return result;
    });
}
