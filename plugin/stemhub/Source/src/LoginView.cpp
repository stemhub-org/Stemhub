#include "../include/Views.hpp"

LoginView::LoginView()
{
    addAndMakeVisible(authStateLabel);
    authStateLabel.setJustificationType(juce::Justification::centred);
    authStateLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    authStateLabel.setFont(juce::FontOptions(20.0f, juce::Font::bold));

    addAndMakeVisible(usernameInput);
    usernameInput.setMultiLine(false);
    usernameInput.setTextToShowWhenEmpty("Username", juce::Colours::grey);

    addAndMakeVisible(passwordInput);
    passwordInput.setMultiLine(false);
    passwordInput.setTextToShowWhenEmpty("Password", juce::Colours::grey);
    passwordInput.setPasswordCharacter('*');

    addAndMakeVisible(signInButton);
    signInButton.onClick = [this]
    {
        if (onSignIn != nullptr)
            onSignIn();
    };
}

void LoginView::resized()
{
    auto area = getLocalBounds().reduced(20);
    const int fieldWidth = 220;
    const int x = (getWidth() - fieldWidth) / 2;

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
