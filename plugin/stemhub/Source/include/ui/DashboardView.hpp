#pragma once

#include <functional>
#include <vector>

#include <JuceHeader.h>

#include "ui/PluginTheme.hpp"
#include "ui/VersionDetailCard.hpp"
#include "ui/VersionHistoryList.hpp"
#include "ui/ViewModels.hpp"

// The open project: its branch, the working copy with the message, the branch's history and the
// selected version's card, with a status bar below.
class DashboardView : public juce::Component
{
public:
    DashboardView();

    void show(const DashboardModel& model);
    // What a save of the working copy takes; a negative count while it is being counted.
    void setWorkingCopySize(int fileCount, juce::int64 totalBytes);
    void setMaxMessageLength(int maxLength) { messageInput.setInputRestrictions(maxLength); }

    [[nodiscard]] juce::String getMessage() const { return messageInput.getText().trim(); }
    void setMessage(const juce::String& message) { messageInput.setText(message, juce::dontSendNotification); }
    void clearMessage() { messageInput.clear(); }

    void paint(juce::Graphics& g) override;
    void resized() override;

    // The Save button, or Return in the message.
    std::function<void()> onSave;
    std::function<void()> onRefresh;
    std::function<void(const juce::String& branchId)> onBranchChange;
    std::function<void(const juce::String& versionId)> onVersionSelected;
    std::function<void()> onBackToProjects;
    std::function<void()> onSignOut;
    std::function<void(const juce::String& versionId)> onRestore;
    // Stops the save or restore in progress.
    std::function<void()> onCancel;

private:
    void setProjectName(const juce::String& name);
    void setBranches(const std::vector<BranchListItem>& branchItems, const juce::String& selectedBranchId);
    void setStatus(const Status& status);
    void applyStatus(const Status& status);
    void setActivity(SessionActivity activity);
    void setWorkingFile(const juce::String& path);
    // The card follows the history's selection.
    void updateDetailCard();
    void updateFooterSummary();

    juce::String projectName;
    std::vector<BranchListItem> branches;
    Status shownStatus;
    SessionActivity shownActivity { SessionActivity::idle };
    juce::String workingFilePath;
    int workingCopyFileCount { -1 };
    juce::int64 workingCopyTotalBytes { 0 };
    juce::Rectangle<int> headerLogoBounds;
    juce::Rectangle<int> branchCaptionBounds;
    juce::Rectangle<int> workingCopyBounds;
    int headerDividerY { 0 };
    int statusBarDividerY { 0 };
    juce::Label headerProjectLabel;
    juce::Label projectStatusLabel;
    juce::Label actionHintLabel;
    juce::Label footerCloudLabel;
    juce::Label footerStorageLabel;
    juce::ComboBox branchComboBox;
    juce::TextButton backToProjectsButton;
    juce::TextEditor messageInput;
    juce::TextButton saveButton;
    juce::TextButton refreshButton;
    juce::TextButton signOutButton;
    juce::TextButton cancelButton { "Cancel" };
    VersionHistoryList historyList;
    VersionDetailCard detailCard;
};
