#pragma once

#include <string>
#include <map>
#include <JuceHeader.h>
#include "User.hpp"
#include "States.hpp"

class LoginView : public juce::Component {
    public:
        const juce::String getUsername() const noexcept { return usernameInput.getText(); }
        const juce::String getPassword() const noexcept { return passwordInput.getText(); }

        void clearUsername() { usernameInput.clear(); }
        void clearPassword() { passwordInput.clear(); }
        void clearInputs() { clearUsername(); clearPassword(); }

        void setMessage(const juce::String& message) { authStateLabel.setText(message, juce::dontSendNotification); }
        std::function<void()> onSignIn;

    private:
        juce::TextEditor usernameInput;
        juce::TextEditor passwordInput;
        juce::Label authStateLabel;
        juce::TextButton signInButton { "Sign In" };
};

class DashboardView : public juce::Component {
    public:
        void setMessage(const juce::String& message) { StatusLabel.setText(message, juce::dontSendNotification); }
        std::function<void()> onSave;
        std::function<void()> onSync;
        std::function<void()> onBranchChange;
        std::function<void()> onSignOut;

    private:
        juce::Label StatusLabel;
        juce::TextButton saveChanges { "Save" }; // Commit button
        juce::TextButton syncButton { "Sync" }; // pull / merge button
        juce::TextButton changeBranch { "view other branches" }; // branch management button
        juce::TextButton signOutButton { "Sign Out" };
};