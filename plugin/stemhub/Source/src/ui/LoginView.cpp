#include "ui/Views.hpp"

namespace
{
juce::Rectangle<int> computeCardBounds(const int containerWidth, const int containerHeight)
{
    const auto width = juce::jmin(420, juce::jmax(280, containerWidth - 40));
    const auto height = juce::jmin(340, juce::jmax(280, containerHeight - 30));
    const auto x = (containerWidth - width) / 2;
    const auto y = (containerHeight - height) / 2;
    return { x, y, width, height };
}
}

LoginView::LoginView()
{
    addAndMakeVisible(authStateLabel);
    authStateLabel.setText("Sign in to continue", juce::dontSendNotification);
    stemhub::plugin::theme::styleStatusLabel(
        authStateLabel,
        "Sign in to continue",
        stemhub::plugin::theme::MessageStatus::neutral);

    addAndMakeVisible(emailInput);
    stemhub::plugin::theme::styleTextInput(emailInput, "Email");

    addAndMakeVisible(passwordInput);
    stemhub::plugin::theme::styleTextInput(passwordInput, "Password");
    passwordInput.setPasswordCharacter('*');

    addAndMakeVisible(signInButton);
    signInButton.setButtonText("Sign in");
    stemhub::plugin::theme::stylePrimaryButton(signInButton);
    signInButton.setTooltip("Sign in to access StemHub projects.");
    signInButton.onClick = [this]
    {
        if (onSignIn != nullptr)
            onSignIn();
    };
}

void LoginView::setMessage(const juce::String& message,
                          stemhub::plugin::theme::MessageStatus status)
{
    stemhub::plugin::theme::styleStatusLabel(authStateLabel, message, status);
    authStateLabel.setTooltip(message);
}

void LoginView::paint(juce::Graphics& g)
{
    const auto cardBounds = computeCardBounds(getWidth(), getHeight()).toFloat();
    g.fillAll(stemhub::plugin::theme::PluginTheme::kBackground);
    stemhub::plugin::theme::paintSurface(g, cardBounds);

    const auto logoArea = cardBounds.withY(cardBounds.getY() + 16.0f)
        .withHeight(44.0f)
        .withTrimmedBottom(12.0f);
    g.setColour(stemhub::plugin::theme::PluginTheme::kForeground);
    g.setFont(stemhub::plugin::theme::headingFont(34.0f));
    g.drawText("Stemhub", logoArea, juce::Justification::centred, false);
}

void LoginView::resized()
{
    auto area = computeCardBounds(getWidth(), getHeight());
    auto content = area.reduced(20);
    content.removeFromTop(66);

    const int fieldWidth = juce::jmin(260, content.getWidth());
    const int centerX = juce::jmax(0, content.getX() + (content.getWidth() - fieldWidth) / 2);

    auto statusRow = content.removeFromTop(42);
    authStateLabel.setBounds(statusRow);
    content.removeFromTop(18);

    auto emailRow = content.removeFromTop(30);
    emailInput.setBounds(centerX, emailRow.getY(), fieldWidth, emailRow.getHeight());

    content.removeFromTop(10);

    auto passwordRow = content.removeFromTop(30);
    passwordInput.setBounds(centerX, passwordRow.getY(), fieldWidth, passwordRow.getHeight());

    content.removeFromTop(10);

    auto signInRow = content.removeFromTop(34);
    signInButton.setBounds(centerX, signInRow.getY(), fieldWidth, signInRow.getHeight());
}
