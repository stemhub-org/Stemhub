#pragma once

#include <functional>

#include <JuceHeader.h>
#include "ui/PluginTheme.hpp"

// What the session is doing, as far as the views care. While it works, the controls that would
// start more work are disabled, and the one that started it shows it is busy.
enum class SessionActivity
{
    idle,
    loading, // projects or history
    saving,
    restoring
};

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

// Everything a project tile shows; built by the editor from the session's project list.
struct ProjectListItem
{
    juce::String id;
    juce::String name;
    juce::String description;
    juce::String category;
    bool isPublic { false };

    bool operator==(const ProjectListItem& other) const
    {
        return id == other.id && name == other.name && description == other.description
            && category == other.category && isPublic == other.isPublic;
    }

    bool operator!=(const ProjectListItem& other) const { return !(*this == other); }
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
    void setActivity(SessionActivity activity);
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
    // Saved without a note of its own.
    bool isUntitled { false };
    juce::Time createdAt;
    juce::String sourceDaw;
    juce::String sourceFilename;
    juce::int64 sizeBytes { 0 };
    bool isOpenInDaw { false };

    bool operator==(const VersionListItem& other) const
    {
        return id == other.id && message == other.message && isUntitled == other.isUntitled
            && createdAt == other.createdAt && sourceDaw == other.sourceDaw
            && sourceFilename == other.sourceFilename && sizeBytes == other.sizeBytes
            && isOpenInDaw == other.isOpenInDaw;
    }

    bool operator!=(const VersionListItem& other) const { return !(*this == other); }
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
    // What a save of the working file takes; a negative count while it is being counted.
    void setSnapshotSize(int fileCount, juce::int64 totalBytes);
    void setMaxNoteLength(int maxLength) { commitMessageInput.setInputRestrictions(maxLength); }
    void setActivity(SessionActivity activity);
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
    int snapshotFileCount { -1 };
    juce::int64 snapshotTotalBytes { 0 };
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
