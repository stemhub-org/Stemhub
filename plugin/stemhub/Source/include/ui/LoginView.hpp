#pragma once

#include <functional>

#include <JuceHeader.h>

#include "ui/PluginTheme.hpp"
#include "ui/ViewModels.hpp"

// The sign-in screen. Enter in either field signs in; while signing in, the form waits and a
// Cancel link stops it.
class LoginView : public juce::Component
{
public:
    LoginView();

    void show(const LoginModel& model);
    // A message about the form itself, such as a missing field, until the model changes.
    void showFormMessage(const juce::String& message);

    [[nodiscard]] juce::String getEmail() const { return emailInput.getText(); }
    [[nodiscard]] juce::String getPassword() const { return passwordInput.getText(); }
    void clearInputs();

    void paint(juce::Graphics&) override;
    void resized() override;

    std::function<void()> onSignIn;
    // Stops the sign-in in progress.
    std::function<void()> onCancel;

private:
    void showStatus(const juce::String& message, stemhub::plugin::theme::MessageStatus status, bool isSigningIn);

    LoginModel shown;
    juce::TextEditor emailInput;
    juce::TextEditor passwordInput;
    juce::Label authStateLabel;
    juce::Label titleLabel;
    juce::Label subtitleLabel;
    juce::Label emailLabel;
    juce::Label passwordLabel;
    juce::TextButton signInButton;
    juce::TextButton cancelButton;
};
