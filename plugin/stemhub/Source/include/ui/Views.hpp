#pragma once

#include <functional>

#include <JuceHeader.h>
#include "ui/PluginTheme.hpp"

class LoginView : public juce::Component
{
public:
    LoginView();

    juce::String getEmail() const noexcept { return emailInput.getText(); }
    juce::String getPassword() const noexcept { return passwordInput.getText(); }

    void clearEmail() { emailInput.clear(); }
    void clearPassword() { passwordInput.clear(); }
    void clearInputs() { clearEmail(); clearPassword(); }

    void setMessage(const juce::String& message,
                    stemhub::plugin::theme::MessageStatus status = stemhub::plugin::theme::MessageStatus::neutral);
    void paint(juce::Graphics&) override;
    void resized() override;
    std::function<void()> onSignIn;

private:
    juce::TextEditor emailInput;
    juce::TextEditor passwordInput;
    juce::Label authStateLabel;
    juce::Label logoLabel;
    juce::Label titleLabel;
    juce::Label subtitleLabel;
    juce::Label emailLabel;
    juce::Label passwordLabel;
    juce::TextButton forgotPasswordButton { "Forgot password?" };
    juce::TextButton offlineButton { "Continue offline" };
    juce::TextButton signInButton { "Sign In" };
};

class ProjectSelectionView : public juce::Component
{
public:
    ProjectSelectionView();

    void setMessage(const juce::String& message,
                    stemhub::plugin::theme::MessageStatus status = stemhub::plugin::theme::MessageStatus::neutral);
    void setSelectedProjectFileMessage(const juce::String& message);
    void setProjectFileSelectionState(bool hasProjectFile, const juce::String& selectedProjectFilePath);
    void setProjects(const std::vector<juce::String>& projectNames,
                     const std::vector<juce::String>& projectIds,
                     const juce::String& selectedProjectId);
    void setHasExistingProjects(bool hasProjects);
    void setCanCreateProject(bool canCreate);
    [[nodiscard]] juce::String getSelectedProjectId() const;
    void resized() override;
    void paint(juce::Graphics& g) override;

    std::function<void()> onChooseProjectFile;
    std::function<void()> onOpenProject;
    std::function<void()> onCreateProject;
    std::function<void()> onSignOut;

    enum class ProjectFilter { All, Local, Cloud };

private:
    std::vector<juce::String> comboProjectIds;
    std::vector<std::pair<juce::String, juce::String>> allProjects;
    bool hasExistingProjects { false };
    bool canCreateProject { false };
    bool hasProjectFile { false };
    ProjectFilter activeProjectFilter { ProjectFilter::All };
    juce::Label titleLabel;
    juce::Label subtitleLabel;
    juce::Label emptyStateTitle;
    juce::Label emptyStateSubtle;
    juce::Label statusLabel;
    juce::Label projectFileLabel;
    juce::TextEditor searchInput;
    juce::TextButton filterAllButton { "All" };
    juce::TextButton filterLocalButton { "Local" };
    juce::TextButton filterCloudButton { "Cloud" };
    juce::ComboBox projectComboBox;
    juce::Viewport projectListViewport;
    juce::Component projectListContent;
    juce::OwnedArray<juce::Component> projectCards;
    juce::TextButton chooseProjectFileButton { "Choose Project File" };
    juce::TextButton openProjectButton { "Open Project" };
    juce::TextButton createProjectButton { "Create Project" };
    juce::TextButton signOutButton { "Sign Out" };

    void rebuildProjectCards();
    void updateProjectFilterButtons();
    void selectProjectById(const juce::String& projectId, bool triggerOpen);
};

class DashboardView : public juce::Component
{
public:
    DashboardView();

    void setProjectStatusMessage(const juce::String& message,
                                stemhub::plugin::theme::MessageStatus status = stemhub::plugin::theme::MessageStatus::neutral);
    void setSelectedProjectFileMessage(const juce::String& message);
    void setSelectedProjectFilePath(const juce::String& projectFilePath)
    {
        selectedProjectFilePath = projectFilePath;
        updateFooterSummary();
        updateSnapshotSummary();
    }
    void setProjectNameMessage(const juce::String& message);
    void setBranchNameMessage(const juce::String& message);
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
    void paint(juce::Graphics& g) override;
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
    std::vector<juce::String> versionDisplayLabels;
    juce::String selectedProjectFilePath;
    int packagedFileCount { 0 };
    juce::Label headerLogoLabel;
    juce::Label headerTitle;
    juce::Label headerProjectLabel;
    juce::Label projectStatusLabel;
    juce::Label snapshotSectionLabel;
    juce::Label snapshotTitleLabel;
    juce::Label snapshotMetaLabel;
    juce::Label actionHintLabel;
    juce::Label historyLabel;
    juce::Label footerPathLabel;
    juce::Label footerCloudLabel;
    juce::Label footerStorageLabel;
    juce::ComboBox branchComboBox;
    juce::ComboBox versionComboBox;
    juce::TextButton backToProjectsButton { "< Projects" };
    juce::TextEditor commitMessageInput;
    juce::TextButton saveChanges { "Save" };
    juce::TextButton syncButton { "Sync latest" };
    juce::TextButton signOutButton { "Sign Out" };
    juce::TextButton restoreButton { "Restore" };
    juce::Viewport versionListViewport;
    juce::Component versionListContent;
    juce::OwnedArray<juce::Component> versionCards;

    void rebuildVersionCards();
    void updateSnapshotSummary();
    void updateFooterSummary();
    void selectVersionById(const juce::String& versionId, bool triggerChange);
};
