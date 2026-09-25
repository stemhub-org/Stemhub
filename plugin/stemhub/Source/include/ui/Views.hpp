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
    juce::Label titleLabel;
    juce::Label subtitleLabel;
    juce::Label emailLabel;
    juce::Label passwordLabel;
    juce::TextButton forgotPasswordButton { "Forgot password?" };
    juce::TextButton offlineButton { "Continue offline" };
    juce::TextButton signInButton { "Sign In" };
};

// Everything a project tile shows; built by the editor from the processor's project list.
struct ProjectListItem
{
    juce::String id;
    juce::String name;
    juce::String description;
    juce::String category;
    bool isPublic { false };
};

class ProjectSelectionView : public juce::Component
{
public:
    ProjectSelectionView();

    void setMessage(const juce::String& message,
                    stemhub::plugin::theme::MessageStatus status = stemhub::plugin::theme::MessageStatus::neutral);
    void setProjectFileSelectionState(bool hasProjectFile, const juce::String& selectedProjectFilePath);
    void setProjects(const std::vector<ProjectListItem>& projects, const juce::String& selectedProjectId);
    void setCanCreateProject(bool canCreate);
    void setAccountName(const juce::String& accountName);
    [[nodiscard]] juce::String getSelectedProjectId() const { return selectedProjectId; }
    void resized() override;
    void paint(juce::Graphics& g) override;

    std::function<void()> onChooseProjectFile;
    std::function<void()> onOpenProject;
    std::function<void()> onCreateProject;
    std::function<void()> onSignOut;

    enum class ProjectFilter { All, Private, Public };

private:
    std::vector<ProjectListItem> allProjects;
    juce::String selectedProjectId;
    juce::String selectedProjectFilePath;
    bool canCreateProject { false };
    bool hasProjectFile { false };
    bool isLoadingProjects { false };
    ProjectFilter activeProjectFilter { ProjectFilter::All };
    juce::Label titleLabel;
    juce::Label accountLabel;
    juce::Label statusLabel;
    juce::TextEditor searchInput;
    juce::TextButton filterAllButton { "All" };
    juce::TextButton filterPrivateButton { "Private" };
    juce::TextButton filterPublicButton { "Public" };
    juce::Viewport projectGridViewport;
    juce::Component projectGridContent;
    juce::OwnedArray<juce::Component> projectTiles;
    juce::TextButton chooseProjectFileButton { "Choose file" };
    juce::TextButton createProjectButton { "Create project" };
    juce::TextButton signOutButton { "Sign out" };

    juce::Rectangle<int> headerLogoBounds;
    juce::Rectangle<int> projectCountBounds;
    int headerDividerY { 0 };

    void rebuildProjectTiles();
    void layoutProjectGrid();
    void updateProjectFilterButtons();
    void updateNewProjectControls();
    void selectProjectById(const juce::String& projectId, bool triggerOpen);
};

// One saved version as the history timeline and detail card show it.
struct VersionListItem
{
    juce::String id;
    juce::String message;
    juce::Time createdAt;
    juce::String sourceDaw;
    juce::String sourceFilename;
    juce::int64 sizeBytes { 0 };
    bool isOpenInDaw { false };
};

class DashboardView : public juce::Component
{
public:
    DashboardView();

    void setProjectStatusMessage(const juce::String& message,
                                stemhub::plugin::theme::MessageStatus status = stemhub::plugin::theme::MessageStatus::neutral);
    void setSelectedProjectFilePath(const juce::String& projectFilePath);
    void setProjectNameMessage(const juce::String& message);
    void setBranchNameMessage(const juce::String& message);
    void setBranches(const std::vector<juce::String>& branchNames,
                     const std::vector<juce::String>& branchIds,
                     const juce::String& selectedBranchId);
    void setVersions(const std::vector<VersionListItem>& versionItems, const juce::String& selectedVersionId);
    void setPackagedFiles(const juce::String& rootLabel,
                          const std::vector<juce::String>& relativeFilePaths);
    [[nodiscard]] juce::String getSelectedBranchId() const;
    [[nodiscard]] juce::String getSelectedVersionId() const { return selectedVersionId; }
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
    std::vector<VersionListItem> versions;
    juce::String selectedVersionId;
    juce::String selectedProjectFilePath;
    int packagedFileCount { 0 };
    juce::Rectangle<int> headerLogoBounds;
    juce::Rectangle<int> branchCaptionBounds;
    juce::Rectangle<int> workingCopyBounds;
    juce::Rectangle<int> detailBounds;
    int headerDividerY { 0 };
    int statusBarDividerY { 0 };
    juce::Label headerProjectLabel;
    juce::Label projectStatusLabel;
    juce::Label actionHintLabel;
    juce::Label footerCloudLabel;
    juce::Label footerStorageLabel;
    juce::Label restoreHintLabel;
    juce::ComboBox branchComboBox;
    juce::TextButton backToProjectsButton { "Projects" };
    juce::TextEditor commitMessageInput;
    juce::TextButton saveChanges { "Save snapshot" };
    juce::TextButton syncButton { "Sync" };
    juce::TextButton signOutButton { "Sign out" };
    juce::TextButton restoreButton { "Restore this version" };
    juce::Viewport versionListViewport;
    juce::Component versionListContent;
    juce::OwnedArray<juce::Component> versionRows;

    [[nodiscard]] const VersionListItem* findSelectedVersion() const;
    [[nodiscard]] int indexOfSelectedVersion() const;
    void rebuildVersionRows();
    void layoutVersionRows();
    void updateDetailControls();
    void updateFooterSummary();
    void selectVersionById(const juce::String& versionId, bool triggerChange);
};
