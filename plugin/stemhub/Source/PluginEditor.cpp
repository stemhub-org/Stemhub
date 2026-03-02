#include "PluginProcessor.hpp"
#include "PluginEditor.hpp"

void StemhubAudioProcessorEditor::refreshComponentVisibility()
{
    const bool showLoginControls = audioProcessor.getAuthState() != AuthState::signedIn;

    usernameInput.setVisible(showLoginControls);
    passwordInput.setVisible(showLoginControls);
    signInButton.setVisible(showLoginControls);
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

    signInButton.onClick = [this]
    {
        const auto username = usernameInput.getText().trim();
        const auto password = passwordInput.getText();

        if (username.isEmpty() || password.isEmpty())
        {
            audioProcessor.setAuthState(AuthState::authError);
            refreshAuthStateLabel();
            refreshComponentVisibility();
            resized();
            repaint();
            return;
        }

        User user;
        user.id = "local-user";
        user.email = "";
        user.username = username;

        audioProcessor.setCurrentUser(user);
        audioProcessor.setAuthState(AuthState::signedIn);
        refreshAuthStateLabel();
        refreshComponentVisibility();
        resized();
        repaint();
    };

    refreshAuthStateLabel();
    refreshComponentVisibility();
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
    } else {
        auto labelArea = getLocalBounds().reduced(40).withSizeKeepingCentre(400, 60);
        authStateLabel.setBounds(labelArea);
    }
}

void StemhubAudioProcessorEditor::refreshAuthStateLabel()
{
    juce::String message;

    switch (audioProcessor.getAuthState())
    {
        case AuthState::signedOut:
            message = "Please sign in to your Stemhub account to access your projects.";
            break;

        case AuthState::signingIn:
            message = "Connecting to Stemhub...";
            break;

        case AuthState::signedIn:
            message = "Welcome back " + audioProcessor.getUsername() + "!";
            break;

        case AuthState::authError:
            message = "An error occurred during authentication. Please try again.";
            break;
    }

    authStateLabel.setText(message, juce::dontSendNotification);
}
