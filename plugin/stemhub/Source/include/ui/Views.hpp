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
    void paint(juce::Graphics&) override;
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
    void setCanCreateProject(bool canCreate);
    [[nodiscard]] juce::String getSelectedProjectId() const;
    void resized() override;

    std::function<void()> onChooseProjectFile;
    std::function<void()> onOpenProject;
    std::function<void()> onCreateProject;
    std::function<void()> onSignOut;

private:
    std::vector<juce::String> comboProjectIds;
    bool hasExistingProjects { false };
    bool canCreateProject { false };
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
    void setProjectNameMessage(const juce::String& message) { projectNameLabel.setText(message, juce::dontSendNotification); }
    void setBranchNameMessage(const juce::String& message) { branchNameLabel.setText(message, juce::dontSendNotification); }
    void setCurrentVersionId(const juce::String& label)
    {
        const auto displayVersion = label.isNotEmpty()
            ? label
            : "Current version: not available";
        currentVersionLabel.setText(displayVersion, juce::dontSendNotification);
    }
    void setCurrentVersionFilePath(const juce::String& path)
    {
        const auto displayPath = path.isNotEmpty()
            ? path
            : "Opened file: not available";
        currentVersionFileLabel.setText("Opened file: " + displayPath, juce::dontSendNotification);
    }
    void setBranches(const std::vector<juce::String>& branchNames,
                     const std::vector<juce::String>& branchIds,
                     const juce::String& selectedBranchId);
    void setVersions(const std::vector<juce::String>& versionLabels,
                     const std::vector<juce::String>& versionIds,
                     const juce::String& selectedVersionId);
    void setPackagedFiles(const juce::String& rootLabel,
                          const std::vector<juce::String>& relativeFilePaths);
    [[nodiscard]] juce::String getSelectedBranchId() const;
    [[nodiscard]] juce::String getSelectedVersionId() const;
    [[nodiscard]] juce::String getCommitMessage() const noexcept { return commitMessageInput.getText().trim(); }
    void setCommitMessage(const juce::String& message) { commitMessageInput.setText(message, juce::dontSendNotification); }
    void clearCommitMessage() { commitMessageInput.clear(); }
    void resized() override;

    std::function<void()> onSave;
    std::function<void()> onSync;
    std::function<void()> onBranchChange;
    std::function<void()> onVersionSelectionChange;
    std::function<void()> onBackToProjects;
    std::function<void()> onSignOut;
    std::function<void()> onRestore;

private:
    std::vector<juce::String> comboBranchIds;
    std::vector<juce::String> comboVersionIds;
    juce::Label projectStatusLabel;
    juce::Label projectFileLabel;
    juce::Label projectNameLabel;
    juce::Label branchNameLabel;
    juce::Label branchLabel;
    juce::Label versionLabel;
    juce::Label currentVersionLabel;
    juce::Label currentVersionFileLabel;
    juce::Label commitMessageLabel;
    juce::ComboBox branchComboBox;
    juce::ComboBox versionComboBox;
    juce::TextButton backToProjectsButton { "< Projects" };
    juce::TextEditor commitMessageInput;
    juce::TextButton saveChanges { "Save" };
    juce::TextButton syncButton { "Sync latest" };
    juce::TextButton changeBranch { "Load" };
    juce::TextButton signOutButton { "Sign Out" };
    juce::TextButton restoreButton { "Restore" };
    juce::Label packagedFilesLabel;
    juce::TreeView packagedFilesTree;
};
