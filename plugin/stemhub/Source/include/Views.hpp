#pragma once

#include <functional>

#include <JuceHeader.h>

class LoginView : public juce::Component
{
public:
    LoginView();

    juce::String getEmail() const noexcept { return emailInput.getText(); }
    juce::String getPassword() const noexcept { return passwordInput.getText(); }

    void clearEmail() { emailInput.clear(); }
    void clearPassword() { passwordInput.clear(); }
    void clearInputs() { clearEmail(); clearPassword(); }

    void setMessage(const juce::String& message) { authStateLabel.setText(message, juce::dontSendNotification); }
    void resized() override;

    std::function<void()> onSignIn;

private:
    juce::TextEditor emailInput;
    juce::TextEditor passwordInput;
    juce::Label authStateLabel;
    juce::TextButton signInButton { "Sign In" };
};

class DashboardView : public juce::Component
{
public:
    DashboardView();

    void setMessage(const juce::String& message) { statusLabel.setText(message, juce::dontSendNotification); }
    void setProjectStatusMessage(const juce::String& message) { projectStatusLabel.setText(message, juce::dontSendNotification); }
    void resized() override;

    std::function<void()> onSave;
    std::function<void()> onSync;
    std::function<void()> onBranchChange;
    std::function<void()> onSignOut;

private:
    juce::Label statusLabel;
    juce::Label projectStatusLabel;
    juce::TextButton saveChanges { "Save" };
    juce::TextButton syncButton { "Sync" };
    juce::TextButton changeBranch { "View Other Branches" };
    juce::TextButton signOutButton { "Sign Out" };
};