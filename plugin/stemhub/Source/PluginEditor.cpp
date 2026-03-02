#include <map>

#include "PluginProcessor.hpp"
#include "PluginEditor.hpp"

template<typename Map, typename Key>
juce::String findMappedMessage(const Map& messageMap, const Key& key, const juce::String& defaultMessage = "")
{
    auto it = messageMap.find(key);
    return (it != messageMap.end()) ? it->second : defaultMessage;
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

void StemhubAudioProcessorEditor::refreshSessionUi()
{
    refreshAuthStateLabel();
    refreshComponentVisibility();
    resized();
    repaint();
}

void StemhubAudioProcessorEditor::refreshComponentVisibility()
{
    const bool isSignedIn = audioProcessor.getAuthState() == AuthState::signedIn;

    usernameInput.setVisible(!isSignedIn);
    passwordInput.setVisible(!isSignedIn);
    signInButton.setVisible(!isSignedIn);

    signOutButton.setVisible(isSignedIn);
    saveChanges.setVisible(isSignedIn);
    syncButton.setVisible(isSignedIn);
    changeBranch.setVisible(isSignedIn);
}

void StemhubAudioProcessorEditor::handleSignInClick()
{
    const auto username = usernameInput.getText().trim();
    const auto password = passwordInput.getText();

    if (username.isEmpty() || password.isEmpty())
    {
        audioProcessor.setAuthState(AuthState::authError);
        refreshSessionUi();
        return;
    }

    User user;
    user.id = "local-user";
    user.email = "";
    user.username = username;

    audioProcessor.signIn(std::move(user));
    passwordInput.clear();
    refreshSessionUi();
}

void StemhubAudioProcessorEditor::handleSignOutClick()
{
    audioProcessor.signOut();
    usernameInput.clear();
    passwordInput.clear();
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

StemhubAudioProcessorEditor::StemhubAudioProcessorEditor(StemhubAudioProcessor& processorToEdit)
    : AudioProcessorEditor(&processorToEdit), audioProcessor(processorToEdit)
{
    setSize(600, 400);

    addAndMakeVisible(authStateLabel);
    authStateLabel.setJustificationType(juce::Justification::centred);
    authStateLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    authStateLabel.setFont(juce::FontOptions(20.0f, juce::Font::bold));

    addAndMakeVisible(usernameInput);
    usernameInput.setMultiLine(false);
    usernameInput.setTextToShowWhenEmpty("Username", juce::Colours::grey);

    addAndMakeVisible(passwordInput);
    passwordInput.setTextToShowWhenEmpty("Password", juce::Colours::grey);
    passwordInput.setPasswordCharacter('*');

    addAndMakeVisible(signInButton);
    addAndMakeVisible(signOutButton);
    addAndMakeVisible(saveChanges);
    addAndMakeVisible(syncButton);
    addAndMakeVisible(changeBranch);

    signInButton.onClick = [this] { handleSignInClick(); };
    signOutButton.onClick = [this] { handleSignOutClick(); };
    saveChanges.onClick = [this] { handleSaveChangesClick(); };
    syncButton.onClick = [this] { handleSyncClick(); };
    changeBranch.onClick = [this] { handleChangeBranchClick(); };

    refreshSessionUi();
}

void StemhubAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));
}

void StemhubAudioProcessorEditor::resized()
{
    const bool showLoginControls = audioProcessor.getAuthState() != AuthState::signedIn;
    auto area = getLocalBounds().reduced(20);
    const int fieldWidth = 220;
    const int x = (getWidth() - fieldWidth) / 2;

    if (showLoginControls)
    {
        area.removeFromTop(64);

        auto usernameRow = area.removeFromTop(32);
        usernameInput.setBounds(x, usernameRow.getY(), fieldWidth, usernameRow.getHeight());

        area.removeFromTop(8);

        auto passwordRow = area.removeFromTop(32);
        passwordInput.setBounds(x, passwordRow.getY(), fieldWidth, passwordRow.getHeight());

        area.removeFromTop(12);

        auto buttonRow = area.removeFromTop(32);
        signInButton.setBounds(x, buttonRow.getY(), fieldWidth, buttonRow.getHeight());

        area.removeFromTop(24);

        auto labelRow = area.removeFromTop(56);
        authStateLabel.setBounds(labelRow);
    }
    else
    {
        auto labelRow = area.removeFromTop(56);
        authStateLabel.setBounds(labelRow);

        area.removeFromTop(24);

        auto saveRow = area.removeFromTop(32);
        saveChanges.setBounds(x, saveRow.getY(), fieldWidth, saveRow.getHeight());

        area.removeFromTop(8);

        auto syncRow = area.removeFromTop(32);
        syncButton.setBounds(x, syncRow.getY(), fieldWidth, syncRow.getHeight());

        area.removeFromTop(8);

        auto branchRow = area.removeFromTop(32);
        changeBranch.setBounds(x, branchRow.getY(), fieldWidth, branchRow.getHeight());

        area.removeFromTop(24);

        auto signOutRow = area.removeFromTop(32);
        signOutButton.setBounds(x, signOutRow.getY(), fieldWidth, signOutRow.getHeight());
    }
}

void StemhubAudioProcessorEditor::refreshAuthStateLabel()
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

    authStateLabel.setText(message, juce::dontSendNotification);
}
