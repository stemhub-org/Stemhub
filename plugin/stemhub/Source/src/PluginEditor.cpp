#include <map>

#include "../include/PluginEditor.hpp"

namespace
{
template<typename Map, typename Key>
juce::String findMappedMessage(const Map& messageMap,
                               const Key& key,
                               const juce::String& defaultMessage = {})
{
    const auto it = messageMap.find(key);
    return it != messageMap.end() ? it->second : defaultMessage;
}

const std::map<AuthState, juce::String> authMessages {
    { AuthState::signedOut, "Please sign in to your Stemhub account to access your projects." },
    { AuthState::authError, "An error occurred during authentication. Please try again." },
};

const std::map<UIState, juce::String> signedInMessages {
    { UIState::login, "Please sign in to your Stemhub account to access your projects." },
    { UIState::commit, "Commit view" },
    { UIState::history, "History and sync view" },
    { UIState::settings, "Branch management view" },
};

const std::map<OperationState, juce::String> operationMessages {
    { OperationState::loadingProjects, "Loading projects..." },
    { OperationState::committing, "Committing..." },
    { OperationState::pulling, "Syncing..." },
    { OperationState::error, "An operation error occurred." },
};
}

StemhubAudioProcessorEditor::StemhubAudioProcessorEditor(StemhubAudioProcessor& processorToEdit)
    : AudioProcessorEditor(&processorToEdit), audioProcessor(processorToEdit)
{
    setSize(600, 400);

    addAndMakeVisible(loginView);
    addAndMakeVisible(dashboardView);

    loginView.onSignIn = [this] { handleSignInClick(); };
    dashboardView.onSave = [this] { handleSaveChangesClick(); };
    dashboardView.onSync = [this] { handleSyncClick(); };
    dashboardView.onBranchChange = [this] { handleChangeBranchClick(); };
    dashboardView.onSignOut = [this] { handleSignOutClick(); };

    refreshSessionUi();
}

void StemhubAudioProcessorEditor::refreshSessionUi()
{
    const bool isSignedIn = audioProcessor.getAuthState() == AuthState::signedIn;
    const auto message = buildStatusMessage();

    loginView.setVisible(!isSignedIn);
    dashboardView.setVisible(isSignedIn);

    if (isSignedIn)
        dashboardView.setMessage(message);
    else
        loginView.setMessage(message);

    resized();
    repaint();
}

void StemhubAudioProcessorEditor::handleSignInClick()
{
    const auto email = loginView.getEmail().trim();
    const auto password = loginView.getPassword();

    if (email.isEmpty() || password.isEmpty())
    {
        audioProcessor.setAuthState(AuthState::authError);
        refreshSessionUi();
        return;
    }

    User user;
    user.id = "local-user";
    user.email = email;
    user.username = email;

    audioProcessor.signIn(std::move(user));
    loginView.clearPassword();
    refreshSessionUi();
}

void StemhubAudioProcessorEditor::handleSignOutClick()
{
    audioProcessor.signOut();
    loginView.clearInputs();
    refreshSessionUi();
}

void StemhubAudioProcessorEditor::handleSaveChangesClick()
{
    audioProcessor.setUIState(UIState::commit);
    audioProcessor.setOperationState(OperationState::idle);
    refreshSessionUi();
}

void StemhubAudioProcessorEditor::handleSyncClick()
{
    audioProcessor.setUIState(UIState::history);
    audioProcessor.setOperationState(OperationState::idle);
    refreshSessionUi();
}

void StemhubAudioProcessorEditor::handleChangeBranchClick()
{
    audioProcessor.setUIState(UIState::settings);
    audioProcessor.setOperationState(OperationState::idle);
    refreshSessionUi();
}

juce::String StemhubAudioProcessorEditor::buildStatusMessage() const
{
    const auto authState = audioProcessor.getAuthState();
    const auto uiState = audioProcessor.getUIState();
    const auto operationState = audioProcessor.getOperationState();

    juce::String message;

    if (authState == AuthState::signedIn && uiState == UIState::dashboard)
        message = "Welcome back " + audioProcessor.getUsername() + "!";
    else if (authState == AuthState::signedIn)
        message = findMappedMessage(signedInMessages, uiState, "Welcome back " + audioProcessor.getUsername() + "!");
    else
        message = findMappedMessage(authMessages, authState);

    const auto operationSuffix = findMappedMessage(operationMessages, operationState);
    if (!operationSuffix.isEmpty())
        message << "\n" << operationSuffix;

    return message;
}

void StemhubAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));
}

void StemhubAudioProcessorEditor::resized()
{
    const auto area = getLocalBounds();
    loginView.setBounds(area);
    dashboardView.setBounds(area);
}
