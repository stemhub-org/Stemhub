#include <algorithm>

#include "application/PluginProcessor.hpp"
#include "application/SessionCache.hpp"

void StemhubAudioProcessor::applyAuthRequestResult(AuthRequestResult result)
{
    const auto fromCachedSession = result.fromCachedSession;

    if (result.authErrorMessage.isNotEmpty())
    {
        if (fromCachedSession && result.sessionExpired)
        {
            expireSession("Saved session expired. Please sign in again.");
            return;
        }

        // Signing in failed, or the saved session couldn't be checked (offline, server down).
        // In that second case the token stays saved, and reopening the plugin tries again.
        if (fromCachedSession)
            didAttemptCachedSessionRestore = false;

        authErrorMessage = result.authErrorMessage;
        setAuthState(AuthState::authError);
        sendChangeMessage();
        return;
    }

    authErrorMessage.clear();
    projectSelectionStatusMessage = std::move(result.projectSelectionStatusMessage);
    activeProjectStatusMessage.clear();
    projects = std::move(result.projects);
    access_tkn = std::move(result.token);
    stemhub::sessioncache::saveAccessToken(access_tkn);
    signIn(std::move(*result.user));

    if (fromCachedSession)
        requestRestoreCachedProjectContext();
}

void StemhubAudioProcessor::expireSession(const juce::String& message)
{
    // The backend refused the token: everything tied to it goes, and the user signs in again.
    signOut();
    authErrorMessage = message;
    setAuthState(AuthState::authError);
    sendChangeMessage();
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

    enqueueBackgroundTask([input = stemhub::usecases::SignInInput { email, password }](const IProjectApi& api)
                              -> BackgroundJobPayload
    {
        return stemhub::usecases::signIn(api, input);
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

    enqueueBackgroundTask([input = stemhub::usecases::RestoreSessionInput { cachedToken }](const IProjectApi& api)
                              -> BackgroundJobPayload
    {
        return stemhub::usecases::restoreSession(api, input);
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
    {
        stemhub::sessioncache::clearProjectContext();
        setOperationState(OperationState::idle);
        projectSelectionStatusMessage = "Last opened project is no longer available. Choose another project.";
        sendChangeMessage();
        return;
    }

    const auto cachedProjectFilePath = stemhub::sessioncache::loadLastOpenedProjectFilePath().trim();
    const auto cachedProjectFile = juce::File(cachedProjectFilePath);
    const auto hasCachedProjectFilePath = cachedProjectFilePath.isNotEmpty();
    const auto hasUsableCachedProjectFile = cachedProjectFile.existsAsFile();
    if (hasCachedProjectFilePath && !hasUsableCachedProjectFile)
        stemhub::sessioncache::clearLastOpenedProjectFilePath();

    const auto localProjectFile = hasUsableCachedProjectFile ? cachedProjectFile : juce::File();
    juce::Logger::writeToLog("[Restore] CachedProjectContext -> projectId="
                             + cachedProjectId
                             + ", cachedProjectFilePath="
                             + cachedProjectFilePath
                             + ", exists="
                             + (cachedProjectFile.existsAsFile() ? "true" : "false"));

    setOperationState(OperationState::loadingProjects);
    projectSelectionStatusMessage = "Restoring last opened project...";
    sendChangeMessage();

    stemhub::usecases::OpenProjectInput input;
    input.projectId = cachedProjectId;
    input.localProjectFile = localProjectFile;
    input.availableProjects = projects;
    input.token = access_tkn;

    enqueueBackgroundTask([input, selectionRequestId = beginSelectionRequest()](const IProjectApi& api)
                              -> BackgroundJobPayload
    {
        auto result = stemhub::usecases::openProject(api, input);
        result.selectionRequestId = selectionRequestId;
        result.fromCachedProjectRestore = true;
        return result;
    });
}
