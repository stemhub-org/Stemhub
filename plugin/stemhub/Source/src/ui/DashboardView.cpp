#include <algorithm>

#include "ui/DashboardView.hpp"
#include "ui/UiFormat.hpp"

namespace
{
namespace theme = stemhub::plugin::theme;
namespace uiformat = stemhub::uiformat;
using Theme = theme::PluginTheme;

void invokeIfBound(const std::function<void()>& callback)
{
    if (callback != nullptr)
        callback();
}
}

DashboardView::DashboardView()
{
    addAndMakeVisible(headerProjectLabel);
    headerProjectLabel.setText("NO PROJECT SELECTED", juce::dontSendNotification);
    headerProjectLabel.setFont(theme::displayFont(16.0f));
    headerProjectLabel.setColour(juce::Label::textColourId, Theme::kForeground);
    headerProjectLabel.setMinimumHorizontalScale(1.0f);
    headerProjectLabel.setBorderSize({});

    addAndMakeVisible(projectStatusLabel);
    theme::makeStatusChip(projectStatusLabel);

    addAndMakeVisible(actionHintLabel);
    theme::makeInlineStatus(actionHintLabel);

    addAndMakeVisible(footerCloudLabel);
    footerCloudLabel.setFont(theme::semiboldFont(11.0f));
    footerCloudLabel.setColour(juce::Label::textColourId, Theme::kForegroundSubtle);
    footerCloudLabel.setJustificationType(juce::Justification::centredRight);
    footerCloudLabel.setMinimumHorizontalScale(1.0f);
    footerCloudLabel.setBorderSize({});

    addAndMakeVisible(footerStorageLabel);
    footerStorageLabel.setFont(theme::labelFont(10.0f));
    footerStorageLabel.setColour(juce::Label::textColourId, Theme::kForegroundTertiary);
    footerStorageLabel.setJustificationType(juce::Justification::centredRight);
    footerStorageLabel.setMinimumHorizontalScale(1.0f);
    footerStorageLabel.setBorderSize({});

    addAndMakeVisible(branchComboBox);
    branchComboBox.setTextWhenNothingSelected("Workspace");
    branchComboBox.setTitle("Workspace");
    theme::styleComboBox(branchComboBox);
    branchComboBox.onChange = [this]
    {
        const auto index = branchComboBox.getSelectedItemIndex();
        if (index >= 0 && index < static_cast<int>(branches.size()) && onBranchChange != nullptr)
            onBranchChange(branches[static_cast<size_t>(index)].id);
    };

    addAndMakeVisible(backToProjectsButton);
    backToProjectsButton.setButtonText(theme::arrowLeft() + "  Projects");
    theme::styleGhostButton(backToProjectsButton);
    backToProjectsButton.onClick = [this] { invokeIfBound(onBackToProjects); };

    addAndMakeVisible(commitMessageInput);
    theme::styleTextInput(commitMessageInput, "What changed? (optional)");
    commitMessageInput.setTitle("Save note");
    commitMessageInput.onReturnKey = [this] { invokeIfBound(onSave); };

    addAndMakeVisible(saveChanges);
    saveChanges.setButtonText("Save snapshot");
    theme::stylePrimaryButton(saveChanges);
    saveChanges.setTooltip("Save the working copy as a new version.");
    saveChanges.onClick = [this] { invokeIfBound(onSave); };

    addAndMakeVisible(syncButton);
    syncButton.setButtonText("Sync");
    theme::styleGhostButton(syncButton);
    syncButton.setTooltip("Fetch the latest history for this branch.");
    syncButton.onClick = [this] { invokeIfBound(onSync); };

    addAndMakeVisible(signOutButton);
    signOutButton.setButtonText("Sign out");
    theme::styleGhostButton(signOutButton);
    signOutButton.onClick = [this] { invokeIfBound(onSignOut); };

    addChildComponent(cancelButton);
    theme::styleLinkButton(cancelButton, Theme::kForegroundSubtle, Theme::kForeground);
    cancelButton.getProperties().set("underlined", true);
    cancelButton.setTooltip("Stop the save or restore in progress.");
    cancelButton.onClick = [this] { invokeIfBound(onCancel); };

    addAndMakeVisible(timeline);
    timeline.onSelect = [this](const juce::String& versionId)
    {
        updateDetailCard();
        if (onVersionSelected != nullptr)
            onVersionSelected(versionId);
    };

    addAndMakeVisible(detailCard);
    detailCard.onRestore = [this]
    {
        if (onRestore != nullptr)
            onRestore(timeline.getSelectedVersionId());
    };

    applyStatus(shownStatus);
    updateDetailCard();
    updateFooterSummary();
}

void DashboardView::show(const DashboardModel& model)
{
    setProjectName(model.projectName);
    setBranches(model.branches, model.selectedBranchId);
    timeline.setVersions(model.versions, model.selectedVersionId);
    updateDetailCard();
    setStatus(model.status);
    setActivity(model.activity);
    setWorkingFile(model.workingFilePath);
}

void DashboardView::setProjectName(const juce::String& name)
{
    if (name == projectName)
        return;

    projectName = name;
    headerProjectLabel.setText(name.toUpperCase(), juce::dontSendNotification);
    headerProjectLabel.setTooltip(name);
    updateFooterSummary();
}

void DashboardView::setBranches(const std::vector<BranchListItem>& branchItems, const juce::String& selectedBranchId)
{
    if (branchItems != branches)
    {
        branches = branchItems;
        branchComboBox.clear(juce::dontSendNotification);
        for (size_t i = 0; i < branches.size(); ++i)
            branchComboBox.addItem(branches[i].name, static_cast<int>(i) + 1);
    }

    const auto selected = std::find_if(branches.begin(), branches.end(), [&selectedBranchId](const BranchListItem& branch)
    {
        return branch.id == selectedBranchId;
    });

    // An unknown branch shows the first one, as the history always did.
    const auto itemId = branches.empty()             ? 0
                      : selected != branches.end() ? static_cast<int>(selected - branches.begin()) + 1
                                                     : 1;
    branchComboBox.setSelectedId(itemId, juce::dontSendNotification);
    branchComboBox.setTooltip(selected != branches.end() ? selected->name : juce::String("Workspace not selected"));
}

void DashboardView::setStatus(const Status& status)
{
    if (status == shownStatus)
        return;

    shownStatus = status;
    applyStatus(status);
}

void DashboardView::applyStatus(const Status& status)
{
    const auto messageStatus = theme::messageStatusFor(status.severity);
    theme::styleStatusLabel(projectStatusLabel, uiformat::statusChipText(status.severity), messageStatus);
    projectStatusLabel.setTooltip(status.text);

    theme::styleStatusLabel(actionHintLabel, status.text.isNotEmpty() ? status.text : juce::String("Ready."), messageStatus);
    const auto isProblem = messageStatus == theme::MessageStatus::warning || messageStatus == theme::MessageStatus::error;
    actionHintLabel.setColour(juce::Label::textColourId, isProblem ? Theme::kForeground : Theme::kForegroundSubtle);
    actionHintLabel.setFont(theme::bodyFont(11.5f));
    actionHintLabel.setTooltip(status.text);
}

void DashboardView::setActivity(const SessionActivity activity)
{
    if (activity == shownActivity)
        return;

    shownActivity = activity;
    const auto isIdle = activity == SessionActivity::idle;
    const auto isSaving = activity == SessionActivity::saving;
    const auto isRestoring = activity == SessionActivity::restoring;

    saveChanges.setEnabled(isIdle);
    theme::setButtonBusy(saveChanges, isSaving);
    saveChanges.setButtonText(isSaving ? "Saving..." : "Save snapshot");

    detailCard.setActivity(activity);
    syncButton.setEnabled(isIdle);
    branchComboBox.setEnabled(isIdle);

    // A save or restore belongs to this project, and a save to the note being typed.
    backToProjectsButton.setEnabled(!isSaving && !isRestoring);
    commitMessageInput.setEnabled(!isSaving && !isRestoring);
    cancelButton.setVisible(isSaving || isRestoring);
}

void DashboardView::setWorkingFile(const juce::String& path)
{
    if (path == workingFilePath)
        return;

    workingFilePath = path;
    updateFooterSummary();
    repaint();
}

void DashboardView::setSnapshotSize(const int fileCount, const juce::int64 totalBytes)
{
    if (fileCount == snapshotFileCount && totalBytes == snapshotTotalBytes)
        return;

    snapshotFileCount = fileCount;
    snapshotTotalBytes = totalBytes;
    updateFooterSummary();
    repaint();
}

void DashboardView::updateDetailCard()
{
    const auto& versions = timeline.getVersions();
    const auto& selectedId = timeline.getSelectedVersionId();
    const auto selected = std::find_if(versions.begin(), versions.end(), [&selectedId](const VersionListItem& version)
    {
        return version.id == selectedId;
    });

    if (selected == versions.end())
    {
        detailCard.clearVersion();
        return;
    }

    // Newest first: the oldest version is number 1.
    const auto count = static_cast<int>(versions.size());
    detailCard.setVersion(*selected, count - static_cast<int>(selected - versions.begin()), count);
}

void DashboardView::updateFooterSummary()
{
    const auto slug = uiformat::slug(headerProjectLabel.getText());
    footerCloudLabel.setText("stemhub.io/" + (slug.isNotEmpty() ? slug : juce::String("project")), juce::dontSendNotification);

    footerStorageLabel.setText(uiformat::snapshotSummary(workingFilePath.isNotEmpty(), snapshotFileCount, snapshotTotalBytes)
                                   .toUpperCase(),
                               juce::dontSendNotification);
    footerStorageLabel.setTooltip(workingFilePath);
}

void DashboardView::paint(juce::Graphics& g)
{
    g.fillAll(Theme::kBackground);

    theme::paintLogoTile(g, headerLogoBounds.toFloat(), Theme::kForeground, Theme::kInk);

    g.setColour(Theme::kSurfaceBorder);
    g.fillRect(24, headerDividerY, getWidth() - 48, 1);
    g.fillRect(24, statusBarDividerY, getWidth() - 48, 1);

    theme::paintMetaText(g, "Branch", branchCaptionBounds, Theme::kForegroundSubtle);

    // Working copy: the head of the timeline, not yet saved.
    const auto ruleX = workingCopyBounds.getX() + VersionTimeline::kRuleX;
    auto content = workingCopyBounds.withTrimmedLeft(VersionTimeline::kGutter);
    auto metaRow = content.removeFromTop(16);
    const juce::Rectangle<float> node { static_cast<float>(ruleX) + 0.5f - 6.0f,
                                        static_cast<float>(metaRow.getCentreY()) - 6.0f, 12.0f, 12.0f };

    g.setColour(Theme::kSurfaceBorder);
    g.fillRect(static_cast<float>(ruleX), node.getBottom(), 1.0f,
               static_cast<float>(timeline.getY() - static_cast<int>(node.getBottom())));
    g.setColour(Theme::kBackground);
    g.fillRect(node);
    g.setColour(Theme::kAccent);
    g.drawRect(node, 1.5f);

    auto fileText = workingFilePath.isNotEmpty() ? juce::File(workingFilePath).getFileName() : juce::String("No local file yet");
    if (workingFilePath.isNotEmpty() && snapshotFileCount > 1)
        fileText += uiformat::metaSeparator() + juce::String(snapshotFileCount) + " files";

    const auto metaWidth = static_cast<int>(std::ceil(juce::GlyphArrangement::getStringWidth(theme::labelFont(10.0f),
                                                                                           "WORKING COPY"))) + 2;
    theme::paintMetaText(g, "Working copy", metaRow.removeFromLeft(metaWidth), Theme::kForeground);
    metaRow.removeFromLeft(14);
    g.setColour(Theme::kForegroundSubtle);
    g.setFont(theme::bodyFont(11.5f));
    g.drawText(fileText, metaRow, juce::Justification::centredRight, true);
}

void DashboardView::resized()
{
    auto area = getLocalBounds().reduced(24, 16);

    auto header = area.removeFromTop(36);
    backToProjectsButton.setBounds(header.removeFromLeft(104));
    header.removeFromLeft(12);
    headerLogoBounds = header.removeFromLeft(28).withSizeKeepingCentre(28, 28);
    header.removeFromLeft(12);
    signOutButton.setBounds(header.removeFromRight(84));
    header.removeFromRight(8);
    projectStatusLabel.setBounds(header.removeFromRight(104).withSizeKeepingCentre(104, 26));
    header.removeFromRight(16);
    headerProjectLabel.setBounds(header.withSizeKeepingCentre(header.getWidth(), 22));

    area.removeFromTop(12);
    headerDividerY = area.getY();
    area.removeFromTop(1 + 14);

    auto statusBar = area.removeFromBottom(18);
    area.removeFromBottom(10);
    statusBarDividerY = area.getBottom();
    area.removeFromBottom(14);

    footerStorageLabel.setBounds(statusBar.removeFromRight(120));
    statusBar.removeFromRight(12);
    footerCloudLabel.setBounds(statusBar.removeFromRight(170));
    statusBar.removeFromRight(16);
    cancelButton.setBounds(statusBar.removeFromRight(52));
    statusBar.removeFromRight(8);
    actionHintLabel.setBounds(statusBar);

    detailCard.setBounds(area.removeFromRight(juce::jlimit(220, 280, area.getWidth() * 2 / 5)));
    area.removeFromRight(20);

    auto historyHeader = area.removeFromTop(30);
    const auto captionWidth = static_cast<int>(std::ceil(juce::GlyphArrangement::getStringWidth(theme::labelFont(10.0f),
                                                                                                "BRANCH"))) + 12;
    branchCaptionBounds = historyHeader.removeFromLeft(captionWidth);
    syncButton.setBounds(historyHeader.removeFromRight(64));
    historyHeader.removeFromRight(8);
    branchComboBox.setBounds(historyHeader.removeFromLeft(juce::jmin(170, historyHeader.getWidth())));

    area.removeFromTop(16);
    workingCopyBounds = area.removeFromTop(16 + 8 + 38);
    auto controls = workingCopyBounds.withTrimmedLeft(VersionTimeline::kGutter).withTrimmedTop(16 + 8);
    saveChanges.setBounds(controls.removeFromRight(128));
    controls.removeFromRight(8);
    commitMessageInput.setBounds(controls);

    area.removeFromTop(12);
    timeline.setBounds(area);
}
