#include "ui/Views.hpp"

namespace
{
const auto kStemhubPurple = juce::Colour::fromRGB(0x9C, 0x57, 0xDF);
const auto kStemhubDark = juce::Colour::fromRGB(0x1E, 0x1E, 0x1E);
const auto kStemhubLight = juce::Colour::fromRGB(0xF1, 0xF1, 0xF1);
const auto kStemhubSurface = juce::Colour::fromRGB(0x26, 0x26, 0x2A);

juce::Font makeSyneFont(float size, int styleFlags)
{
    return juce::Font(juce::FontOptions("Syne", size, styleFlags));
}
}

LoginView::LoginView()
{
    addAndMakeVisible(authStateLabel);
    authStateLabel.setJustificationType(juce::Justification::centred);
    authStateLabel.setColour(juce::Label::textColourId, kStemhubLight);
    authStateLabel.setFont(makeSyneFont(18.0f, juce::Font::bold));

    addAndMakeVisible(emailInput);
    emailInput.setMultiLine(false);
    emailInput.setTextToShowWhenEmpty("Email", kStemhubLight.withAlpha(0.45f));
    emailInput.setColour(juce::TextEditor::textColourId, kStemhubLight);
    emailInput.setColour(juce::TextEditor::backgroundColourId, kStemhubSurface);
    emailInput.setColour(juce::TextEditor::outlineColourId, kStemhubLight.withAlpha(0.45f));
    emailInput.setColour(juce::TextEditor::focusedOutlineColourId, kStemhubPurple);
    emailInput.setColour(juce::CaretComponent::caretColourId, kStemhubPurple);

    addAndMakeVisible(passwordInput);
    passwordInput.setMultiLine(false);
    passwordInput.setTextToShowWhenEmpty("Password", kStemhubLight.withAlpha(0.45f));
    passwordInput.setColour(juce::TextEditor::textColourId, kStemhubLight);
    passwordInput.setColour(juce::TextEditor::backgroundColourId, kStemhubSurface);
    passwordInput.setColour(juce::TextEditor::outlineColourId, kStemhubLight.withAlpha(0.45f));
    passwordInput.setColour(juce::TextEditor::focusedOutlineColourId, kStemhubPurple);
    passwordInput.setColour(juce::CaretComponent::caretColourId, kStemhubPurple);
    passwordInput.setPasswordCharacter('*');

    addAndMakeVisible(signInButton);
    signInButton.setColour(juce::TextButton::buttonColourId, kStemhubPurple);
    signInButton.setColour(juce::TextButton::buttonOnColourId, kStemhubPurple.brighter(0.15f));
    signInButton.setColour(juce::TextButton::textColourOffId, kStemhubLight);
    signInButton.setColour(juce::TextButton::textColourOnId, kStemhubLight);
    signInButton.onClick = [this]
    {
        if (onSignIn != nullptr)
            onSignIn();
    };
}

void LoginView::paint(juce::Graphics& g)
{
    const auto area = getLocalBounds().reduced(20);
    const int logoWidth = 240;
    const int logoHeight = 40;
    auto logoArea = juce::Rectangle<int>((getWidth() - logoWidth) / 2, area.getY() + 8, logoWidth, logoHeight);

    g.setColour(kStemhubLight);
    g.setFont(makeSyneFont(30.0f, juce::Font::bold));
    g.drawText("Stemhub.", logoArea, juce::Justification::centred, false);

    g.setColour(kStemhubPurple.withAlpha(0.9f));
    g.drawLine(static_cast<float>(logoArea.getX() + 46),
               static_cast<float>(logoArea.getBottom() + 2),
               static_cast<float>(logoArea.getRight() - 46),
               static_cast<float>(logoArea.getBottom() + 2),
               1.2f);
}

void LoginView::resized()
{
    auto area = getLocalBounds().reduced(20);
    const int fieldWidth = 220;
    const int x = (getWidth() - fieldWidth) / 2;

    area.removeFromTop(86);

    auto emailRow = area.removeFromTop(32);
    emailInput.setBounds(x, emailRow.getY(), fieldWidth, emailRow.getHeight());

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
