#include "ui/Views.hpp"

namespace
{
juce::Rectangle<int> computeFormBounds(const int containerWidth, const int containerHeight)
{
    const auto width = juce::jmin(420, juce::jmax(320, containerWidth - 48));
    const auto height = juce::jmin(520, juce::jmax(460, containerHeight - 40));
    const auto x = (containerWidth - width) / 2;
    const auto y = (containerHeight - height) / 2;
    return { x, y, width, height };
}

void styleLink(juce::TextButton& button, bool underlined)
{
    button.setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    button.setColour(juce::TextButton::buttonOnColourId, juce::Colours::transparentBlack);
    button.setColour(juce::TextButton::textColourOffId, stemhub::plugin::theme::PluginTheme::kForegroundSubtle);
    button.setColour(juce::TextButton::textColourOnId, stemhub::plugin::theme::PluginTheme::kForeground);
    button.setMouseCursor(juce::MouseCursor::PointingHandCursor);
    button.setSize(0, 24);
    button.getProperties().set("underlined", underlined);
}

void styleFieldLabel(juce::Label& label, const juce::String& text)
{
    label.setText(text, juce::dontSendNotification);
    label.setFont(stemhub::plugin::theme::bodyFont(13.0f, juce::Font::bold));
    label.setColour(juce::Label::textColourId, stemhub::plugin::theme::PluginTheme::kForegroundSubtle);
    label.setJustificationType(juce::Justification::centredLeft);
}
}

LoginView::LoginView()
{
    addAndMakeVisible(authStateLabel);
    stemhub::plugin::theme::styleStatusLabel(authStateLabel, {}, stemhub::plugin::theme::MessageStatus::neutral);
    authStateLabel.setVisible(false);

    addAndMakeVisible(logoLabel);
    logoLabel.setJustificationType(juce::Justification::centred);
    logoLabel.setColour(juce::Label::backgroundColourId, stemhub::plugin::theme::PluginTheme::kAccent);
    logoLabel.setColour(juce::Label::textColourId, stemhub::plugin::theme::PluginTheme::kBackground);
    logoLabel.setFont(stemhub::plugin::theme::headingFont(21.0f));
    logoLabel.setText("S", juce::dontSendNotification);

    addAndMakeVisible(titleLabel);
    titleLabel.setText("Stemhub Session", juce::dontSendNotification);
    titleLabel.setFont(stemhub::plugin::theme::headingFont(30.0f));
    titleLabel.setColour(juce::Label::textColourId, stemhub::plugin::theme::PluginTheme::kForeground);
    titleLabel.setJustificationType(juce::Justification::centred);

    addAndMakeVisible(subtitleLabel);
    subtitleLabel.setText("Sign in to sync your music projects", juce::dontSendNotification);
    subtitleLabel.setFont(stemhub::plugin::theme::bodyFont(14.0f));
    subtitleLabel.setColour(juce::Label::textColourId, stemhub::plugin::theme::PluginTheme::kForegroundTertiary);
    subtitleLabel.setJustificationType(juce::Justification::centred);

    addAndMakeVisible(emailLabel);
    styleFieldLabel(emailLabel, "Email");

    addAndMakeVisible(passwordLabel);
    styleFieldLabel(passwordLabel, "Password");

    addAndMakeVisible(emailInput);
    stemhub::plugin::theme::styleTextInput(emailInput, "you@example.com");

    addAndMakeVisible(passwordInput);
    stemhub::plugin::theme::styleTextInput(passwordInput, "........");
    passwordInput.setPasswordCharacter('*');

    addAndMakeVisible(signInButton);
    signInButton.setButtonText("Sign In");
    stemhub::plugin::theme::stylePrimaryButton(signInButton);
    signInButton.setTooltip("Sign in to access Stemhub projects.");
    signInButton.onClick = [this]
    {
        if (onSignIn != nullptr)
            onSignIn();
    };

    addAndMakeVisible(forgotPasswordButton);
    forgotPasswordButton.setButtonText("Forgot password?");
    styleLink(forgotPasswordButton, true);
    forgotPasswordButton.onClick = [] {};

    addAndMakeVisible(offlineButton);
    offlineButton.setButtonText("Continue Offline");
    stemhub::plugin::theme::styleSecondaryButton(offlineButton);
    offlineButton.onClick = [] {};
}

void LoginView::setMessage(const juce::String& message, stemhub::plugin::theme::MessageStatus status)
{
    stemhub::plugin::theme::styleStatusLabel(authStateLabel, message, status);
    authStateLabel.setTooltip(message);
    authStateLabel.setVisible(message.isNotEmpty());

    const bool isLoadingInProgress = status == stemhub::plugin::theme::MessageStatus::loading;
    signInButton.setEnabled(!isLoadingInProgress);
    emailInput.setEnabled(!isLoadingInProgress);
    passwordInput.setEnabled(!isLoadingInProgress);
    forgotPasswordButton.setEnabled(!isLoadingInProgress);
    offlineButton.setEnabled(!isLoadingInProgress);
    signInButton.setAlpha(isLoadingInProgress ? 0.72f : 1.0f);
    signInButton.setButtonText(isLoadingInProgress ? "Signing in..." : "Sign In");
    resized();
    repaint();
}

void LoginView::paint(juce::Graphics& g)
{
    g.fillAll(stemhub::plugin::theme::PluginTheme::kBackground);

    const auto form = computeFormBounds(getWidth(), getHeight()).toFloat();
    const auto separatorY = form.getBottom() - 102.0f;

    g.setColour(stemhub::plugin::theme::PluginTheme::kBorderSubtle);
    g.drawLine(form.getX(), separatorY, form.getRight(), separatorY, 1.0f);

    const auto logoBounds = juce::Rectangle<float>(48.0f, 48.0f)
                                .withCentre({ static_cast<float>(getWidth()) * 0.5f, form.getY() + 24.0f });
    g.setColour(stemhub::plugin::theme::PluginTheme::kAccentGlow);
    g.fillRoundedRectangle(logoBounds.expanded(6.0f), 12.0f);
    g.setColour(stemhub::plugin::theme::PluginTheme::kAccent.withAlpha(0.25f));
    g.drawRoundedRectangle(logoBounds.expanded(0.5f), 12.0f, 1.0f);
}

void LoginView::resized()
{
    auto form = computeFormBounds(getWidth(), getHeight());
    auto content = form;

    auto logoRow = content.removeFromTop(64);
    logoLabel.setBounds(logoRow.withSizeKeepingCentre(48, 48));

    content.removeFromTop(18);
    auto titleRow = content.removeFromTop(42);
    titleLabel.setBounds(titleRow);

    auto subtitleRow = content.removeFromTop(30);
    subtitleLabel.setBounds(subtitleRow);

    content.removeFromTop(28);

    auto emailLabelRow = content.removeFromTop(22);
    emailLabel.setBounds(emailLabelRow);

    content.removeFromTop(8);
    auto emailRow = content.removeFromTop(46);
    emailInput.setBounds(emailRow);

    content.removeFromTop(18);
    auto passwordLabelRow = content.removeFromTop(22);
    passwordLabel.setBounds(passwordLabelRow);

    content.removeFromTop(8);
    auto passwordRow = content.removeFromTop(46);
    passwordInput.setBounds(passwordRow);

    content.removeFromTop(18);

    if (authStateLabel.isVisible())
    {
        auto statusRow = content.removeFromTop(38);
        authStateLabel.setBounds(statusRow);
        content.removeFromTop(12);
    }
    else
    {
        authStateLabel.setBounds(0, 0, 0, 0);
    }

    auto signRow = content.removeFromTop(46);
    signInButton.setBounds(signRow);

    content.removeFromTop(26);
    auto forgotRow = content.removeFromTop(24);
    forgotPasswordButton.setBounds(forgotRow.withWidth(220).withX(form.getX() + (form.getWidth() - 220) / 2));

    content.removeFromTop(44);
    auto offlineRow = content.removeFromTop(46);
    offlineButton.setBounds(offlineRow);
}
