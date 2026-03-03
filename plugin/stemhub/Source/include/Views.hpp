#pragma once

#include <functional>

#include <JuceHeader.h>

class LoginView : public juce::Component
{
public:
    LoginView();

    juce::String getUsername() const noexcept { return usernameInput.getText(); }
    juce::String getPassword() const noexcept { return passwordInput.getText(); }

    void clearUsername() { usernameInput.clear(); }
    void clearPassword() { passwordInput.clear(); }
    void clearInputs() { clearUsername(); clearPassword(); }

    void setMessage(const juce::String& message) { authStateLabel.setText(message, juce::dontSendNotification); }
    void resized() override;

    std::function<void()> onSignIn;

private:
    juce::TextEditor usernameInput;
    juce::TextEditor passwordInput;
    juce::Label authStateLabel;
    juce::TextButton signInButton { "Sign In" };
};

class DashboardView : public juce::Component
{
public:
    DashboardView();

    void setMessage(const juce::String& message) { statusLabel.setText(message, juce::dontSendNotification); }
    void resized() override;

    std::function<void()> onSave;
    std::function<void()> onSync;
    std::function<void()> onBranchChange;
    std::function<void()> onSignOut;

private:
    juce::Label statusLabel;
    juce::TextButton saveChanges { "Save" };
    juce::TextButton syncButton { "Sync" };
    juce::TextButton changeBranch { "View Other Branches" };
    juce::TextButton signOutButton { "Sign Out" };
};
