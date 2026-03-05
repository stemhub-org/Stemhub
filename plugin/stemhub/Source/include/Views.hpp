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

class ProjectSelectionView : public juce::Component
{
public:
    ProjectSelectionView();

    void setMessage(const juce::String& message) { statusLabel.setText(message, juce::dontSendNotification); }
    void setSelectedProjectFileMessage(const juce::String& message) { projectFileLabel.setText(message, juce::dontSendNotification); }
    void setProjects(const std::vector<juce::String>& projectNames,
                     const std::vector<juce::String>& projectIds,
                     const juce::String& selectedProjectId);
    void setHasExistingProjects(bool hasProjects);
    juce::String getSelectedProjectId() const;
    void resized() override;

    std::function<void()> onChooseProjectFile;
    std::function<void()> onOpenProject;
    std::function<void()> onCreateProject;
    std::function<void()> onSignOut;

private:
    std::vector<juce::String> comboProjectIds;
    bool hasExistingProjects { false };
    juce::Label statusLabel;
    juce::Label projectFileLabel;
    juce::Label existingProjectsLabel;
    juce::ComboBox projectComboBox;
    juce::TextButton chooseProjectFileButton { "Choose Project File" };
    juce::TextButton openProjectButton { "Open Project" };
    juce::TextButton createProjectButton { "Create Project" };
    juce::TextButton signOutButton { "Sign Out" };
};

class DashboardView : public juce::Component
{
public:
    DashboardView();

    void setProjectStatusMessage(const juce::String& message) { projectStatusLabel.setText(message, juce::dontSendNotification); }
    void setSelectedProjectFileMessage(const juce::String& message) { projectFileLabel.setText(message, juce::dontSendNotification); }
    void setCurrentProjectMessage(const juce::String& message) { currentProjectLabel.setText(message, juce::dontSendNotification); }
    void resized() override;

    std::function<void()> onSave;
    std::function<void()> onSync;
    std::function<void()> onBranchChange;
    std::function<void()> onSignOut;

private:
    juce::Label projectStatusLabel;
    juce::Label projectFileLabel;
    juce::Label currentProjectLabel;
    juce::TextButton saveChanges { "Save" };
    juce::TextButton syncButton { "Sync" };
    juce::TextButton changeBranch { "View Other Branches" };
    juce::TextButton signOutButton { "Sign Out" };
};
