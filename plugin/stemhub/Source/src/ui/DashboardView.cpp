#include "ui/Views.hpp"
#include <algorithm>
#include <utility>

namespace
{
void invokeIfBound(const std::function<void()>& callback)
{
    if (callback != nullptr)
        callback();
}

void applyButtonAvailability(juce::Component& button, bool isAvailable)
{
    button.setEnabled(isAvailable);
    button.setAlpha(isAvailable ? 1.0f : 0.5f);
}

void setMappedComboItems(juce::ComboBox& combo,
                         std::vector<juce::String>& mappedIds,
                         const std::vector<juce::String>& itemNames,
                         const std::vector<juce::String>& itemIds,
                         const juce::String& selectedItemId)
{
    combo.clear(juce::dontSendNotification);
    mappedIds.clear();

    for (size_t i = 0; i < itemNames.size() && i < itemIds.size(); ++i)
    {
        combo.addItem(itemNames[i], static_cast<int>(i) + 1);
        mappedIds.push_back(itemIds[i]);
    }

    if (mappedIds.empty())
    {
        combo.setSelectedId(0, juce::dontSendNotification);
        return;
    }

    if (selectedItemId.isNotEmpty())
    {
        for (size_t i = 0; i < mappedIds.size(); ++i)
        {
            if (mappedIds[i] == selectedItemId)
            {
                combo.setSelectedId(static_cast<int>(i) + 1, juce::dontSendNotification);
                return;
            }
        }
    }

    combo.setSelectedId(1, juce::dontSendNotification);
}

juce::String getMappedComboSelection(const juce::ComboBox& combo, const std::vector<juce::String>& mappedIds)
{
    const auto selectedIndex = combo.getSelectedItemIndex();
    if (selectedIndex < 0 || static_cast<size_t>(selectedIndex) >= mappedIds.size())
        return {};

    return mappedIds[static_cast<size_t>(selectedIndex)];
}

struct FileTreeNode
{
    juce::String name;
    bool isFile { false };
    std::vector<FileTreeNode> children;
};

class PackagedFilesTreeItem final : public juce::TreeViewItem
{
public:
    PackagedFilesTreeItem(juce::String textToShow, juce::String uniqueNameToUse, bool isDirectoryItem)
        : text(std::move(textToShow)),
          uniqueName(std::move(uniqueNameToUse)),
          isDirectory(isDirectoryItem)
    {
    }

    [[nodiscard]] bool mightContainSubItems() override
    {
        return getNumSubItems() > 0;
    }

    [[nodiscard]] juce::String getUniqueName() const override
    {
        return uniqueName;
    }

    void paintItem(juce::Graphics& g, int width, int height) override
    {
        if (isSelected())
            g.fillAll(stemhub::plugin::theme::PluginTheme::kAccent.withAlpha(0.18f));

        g.setColour(isDirectory
                        ? stemhub::plugin::theme::PluginTheme::kForeground
                        : stemhub::plugin::theme::PluginTheme::kForegroundSubtle);
        g.setFont(stemhub::plugin::theme::bodyFont(isDirectory ? 13.0f : 12.5f,
                                                   isDirectory ? juce::Font::bold : juce::Font::plain));
        g.drawText(text, 2, 0, width - 4, height, juce::Justification::centredLeft, true);
    }

private:
    juce::String text;
    juce::String uniqueName;
    bool isDirectory { false };
};

FileTreeNode* findOrCreateChild(FileTreeNode& parent, const juce::String& childName)
{
    auto it = std::find_if(parent.children.begin(), parent.children.end(), [&childName](const FileTreeNode& child)
    {
        return child.name == childName;
    });

    if (it != parent.children.end())
        return &(*it);

    parent.children.push_back(FileTreeNode{ childName, false, {} });
    return &parent.children.back();
}

void insertRelativePath(FileTreeNode& root, const juce::String& relativePath)
{
    juce::StringArray pathParts;
    pathParts.addTokens(relativePath.replaceCharacter('\\', '/'), "/", "");

    if (pathParts.isEmpty())
        return;

    auto* currentNode = &root;
    for (int i = 0; i < pathParts.size(); ++i)
    {
        auto* child = findOrCreateChild(*currentNode, pathParts[i]);
        child->isFile = (i == pathParts.size() - 1);
        currentNode = child;
    }
}

void sortTree(FileTreeNode& node)
{
    std::sort(node.children.begin(), node.children.end(), [](const FileTreeNode& lhs, const FileTreeNode& rhs)
    {
        const bool lhsIsDirectory = !lhs.children.empty() || !lhs.isFile;
        const bool rhsIsDirectory = !rhs.children.empty() || !rhs.isFile;

        if (lhsIsDirectory != rhsIsDirectory)
            return lhsIsDirectory;

        return lhs.name.compareNatural(rhs.name) < 0;
    });

    for (auto& child : node.children)
        sortTree(child);
}

void addTreeItemsRecursively(const FileTreeNode& node, juce::TreeViewItem& parent, const juce::String& parentPath)
{
    for (const auto& child : node.children)
    {
        const auto childPath = parentPath.isEmpty() ? child.name : parentPath + "/" + child.name;
        const bool isDirectory = !child.children.empty() || !child.isFile;
        auto* childItem = new PackagedFilesTreeItem(child.name, childPath, isDirectory);
        parent.addSubItem(childItem);

        if (!child.children.empty())
        {
            addTreeItemsRecursively(child, *childItem, childPath);
            childItem->setOpen(true);
        }
    }
}

juce::TreeViewItem* createPackagedFilesRootItem(const std::vector<juce::String>& relativeFilePaths)
{
    auto* rootItem = new PackagedFilesTreeItem("__root__", "__root__", true);

    if (relativeFilePaths.empty())
    {
        rootItem->addSubItem(new PackagedFilesTreeItem("No files detected", "__empty__", false));
        return rootItem;
    }

    FileTreeNode rootNode;
    rootNode.name = "__root__";

    for (const auto& relativePath : relativeFilePaths)
    {
        if (relativePath.isNotEmpty())
            insertRelativePath(rootNode, relativePath);
    }

    sortTree(rootNode);
    addTreeItemsRecursively(rootNode, *rootItem, {});
    return rootItem;
}
}

ProjectSelectionView::ProjectSelectionView()
{
    addAndMakeVisible(statusLabel);
    stemhub::plugin::theme::styleStatusLabel(statusLabel,
                                             "Project setup",
                                             stemhub::plugin::theme::MessageStatus::neutral);

    addAndMakeVisible(projectFileLabel);
    stemhub::plugin::theme::styleInfoLabel(projectFileLabel, "No DAW project file selected.", false, true);
    projectFileLabel.setTooltip("No DAW project file selected.");

    addAndMakeVisible(projectComboBox);
    projectComboBox.setTextWhenNothingSelected("Select existing project");
    stemhub::plugin::theme::styleComboBox(projectComboBox);

    addAndMakeVisible(chooseProjectFileButton);
    chooseProjectFileButton.setButtonText("Choose DAW project file");
    stemhub::plugin::theme::stylePrimaryButton(chooseProjectFileButton);
    chooseProjectFileButton.onClick = [this]
    {
        invokeIfBound(onChooseProjectFile);
    };

    addAndMakeVisible(openProjectButton);
    openProjectButton.setButtonText("Open Existing Project");
    stemhub::plugin::theme::styleSecondaryButton(openProjectButton);
    openProjectButton.onClick = [this]
    {
        invokeIfBound(onOpenProject);
    };
    openProjectButton.setVisible(false);

    addAndMakeVisible(createProjectButton);
    createProjectButton.setButtonText("Create New Project");
    stemhub::plugin::theme::styleSecondaryButton(createProjectButton);
    createProjectButton.onClick = [this]
    {
        invokeIfBound(onCreateProject);
    };
    createProjectButton.setVisible(false);

    addAndMakeVisible(signOutButton);
    signOutButton.setButtonText("Sign Out");
    signOutButton.onClick = [this]
    {
        invokeIfBound(onSignOut);
    };
    stemhub::plugin::theme::styleGhostButton(signOutButton);
}

void ProjectSelectionView::setMessage(const juce::String& message,
                                     stemhub::plugin::theme::MessageStatus status)
{
    stemhub::plugin::theme::styleStatusLabel(statusLabel, message, status);
    statusLabel.setTooltip(message);
}

void ProjectSelectionView::setProjects(const std::vector<juce::String>& projectNames,
                                       const std::vector<juce::String>& projectIds,
                                       const juce::String& selectedProjectId)
{
    projectComboBox.clear(juce::dontSendNotification);
    comboProjectIds = projectIds;

    for (size_t i = 0; i < projectNames.size() && i < projectIds.size(); ++i)
        projectComboBox.addItem(projectNames[i], static_cast<int>(i) + 1);

    if (selectedProjectId.isNotEmpty())
    {
        for (size_t i = 0; i < comboProjectIds.size(); ++i)
        {
            if (comboProjectIds[i] == selectedProjectId)
            {
                projectComboBox.setSelectedId(static_cast<int>(i) + 1, juce::dontSendNotification);
                break;
            }
        }
    }
    else if (!comboProjectIds.empty())
    {
        projectComboBox.setSelectedId(1, juce::dontSendNotification);
    }
}

void ProjectSelectionView::setSelectedProjectFileMessage(const juce::String& message)
{
    projectFileLabel.setText(message, juce::dontSendNotification);
    projectFileLabel.setTooltip(message);
}

void ProjectSelectionView::setProjectFileSelectionState(bool fileSelected, const juce::String& selectedProjectFilePath)
{
    hasProjectFile = fileSelected;
    if (hasProjectFile)
    {
        const auto fileInfo = selectedProjectFilePath.isNotEmpty()
            ? selectedProjectFilePath
            : "DAW project file selected.";
        projectFileLabel.setText(fileInfo, juce::dontSendNotification);
        projectFileLabel.setTooltip(fileInfo);
    }
    else
    {
        const auto missingPath = "No DAW project file selected.";
        projectFileLabel.setText(missingPath, juce::dontSendNotification);
        projectFileLabel.setTooltip(missingPath);
    }

    projectComboBox.setEnabled(hasExistingProjects);
    openProjectButton.setVisible(hasExistingProjects);
    createProjectButton.setVisible(canCreateProject);
    applyButtonAvailability(openProjectButton, hasExistingProjects);
    applyButtonAvailability(createProjectButton, canCreateProject);
    applyButtonAvailability(projectComboBox, hasExistingProjects);
    projectComboBox.setVisible(hasExistingProjects);

    resized();
}

void ProjectSelectionView::setHasExistingProjects(bool hasProjects)
{
    hasExistingProjects = hasProjects;
    projectComboBox.setVisible(hasProjects);
    openProjectButton.setVisible(hasProjects);
    applyButtonAvailability(projectComboBox, hasProjects);
    applyButtonAvailability(openProjectButton, hasProjects);
    if (!hasProjects)
    {
        openProjectButton.setVisible(false);
        projectComboBox.setVisible(false);
        applyButtonAvailability(openProjectButton, false);
        applyButtonAvailability(projectComboBox, false);
    }

    resized();
}

void ProjectSelectionView::setCanCreateProject(bool canCreate)
{
    canCreateProject = canCreate && hasProjectFile;
    createProjectButton.setVisible(canCreateProject);
    applyButtonAvailability(createProjectButton, canCreateProject);
    resized();
}

juce::String ProjectSelectionView::getSelectedProjectId() const
{
    const auto selectedIndex = projectComboBox.getSelectedItemIndex();
    if (selectedIndex < 0 || static_cast<size_t>(selectedIndex) >= comboProjectIds.size())
        return {};

    return comboProjectIds[static_cast<size_t>(selectedIndex)];
}

void ProjectSelectionView::resized()
{
    auto area = getLocalBounds().reduced(16);
    const int contentWidth = juce::jmin(500, area.getWidth());
    auto card = area.withWidth(contentWidth).withX((area.getWidth() - contentWidth) / 2);
    const int controlWidth = 320;
    const int centerX = card.getX() + (card.getWidth() - controlWidth) / 2;
    const int contentRight = card.getRight();

    auto topActionsRow = card.removeFromTop(24);
    const int signOutButtonWidth = 72;
    signOutButton.setBounds(contentRight - signOutButtonWidth, topActionsRow.getY(), signOutButtonWidth, topActionsRow.getHeight());

    card.removeFromTop(8);
    auto statusRow = card.removeFromTop(40);
    statusLabel.setBounds(statusRow);

    card.removeFromTop(14);
    auto fileRow = card.removeFromTop(30);
    projectFileLabel.setBounds(centerX, fileRow.getY(), controlWidth, fileRow.getHeight());

    card.removeFromTop(12);
    auto chooseRow = card.removeFromTop(36);
    chooseProjectFileButton.setBounds(centerX, chooseRow.getY(), controlWidth, chooseRow.getHeight());

    if (hasExistingProjects)
    {
        card.removeFromTop(18);
        auto comboRow = card.removeFromTop(30);
        projectComboBox.setBounds(centerX, comboRow.getY(), controlWidth, comboRow.getHeight());

        card.removeFromTop(8);
        auto openRow = card.removeFromTop(34);
        openProjectButton.setBounds(centerX, openRow.getY(), controlWidth, openRow.getHeight());
        card.removeFromTop(8);
    }
    else
    {
        projectComboBox.setBounds(0, 0, 0, 0);
        openProjectButton.setBounds(0, 0, 0, 0);
        card.removeFromTop(42);
    }

    if (createProjectButton.isVisible())
    {
        auto createRow = card.removeFromTop(34);
        createProjectButton.setBounds(centerX, createRow.getY(), controlWidth, createRow.getHeight());
        card.removeFromTop(8);
    }
    else
    {
        createProjectButton.setBounds(0, 0, 0, 0);
    }
}

DashboardView::DashboardView()
{
    addAndMakeVisible(projectStatusLabel);
    stemhub::plugin::theme::styleStatusLabel(projectStatusLabel,
                                             "Project status",
                                             stemhub::plugin::theme::MessageStatus::neutral);

    addAndMakeVisible(projectFileLabel);
    stemhub::plugin::theme::styleInfoLabel(projectFileLabel,
                                           "No project file selected.",
                                           false,
                                           true);
    projectFileLabel.setTooltip("No project file selected.");

    addAndMakeVisible(projectNameLabel);
    stemhub::plugin::theme::styleInfoLabel(projectNameLabel, "Project: not selected");

    addAndMakeVisible(branchNameLabel);
    stemhub::plugin::theme::styleInfoLabel(branchNameLabel, "Workspace: not selected", false, true);

    addAndMakeVisible(branchComboBox);
    branchComboBox.setTextWhenNothingSelected("Select workspace");
    stemhub::plugin::theme::styleComboBox(branchComboBox);
    branchComboBox.onChange = [this]
    {
        invokeIfBound(onBranchChange);
    };

    addAndMakeVisible(versionComboBox);
    versionComboBox.setTextWhenNothingSelected("Saved versions");
    stemhub::plugin::theme::styleComboBox(versionComboBox);
    versionComboBox.onChange = [this]
    {
        invokeIfBound(onVersionSelectionChange);
    };

    addAndMakeVisible(backToProjectsButton);
    backToProjectsButton.setButtonText("< Projects");
    stemhub::plugin::theme::styleGhostButton(backToProjectsButton);
    backToProjectsButton.onClick = [this]
    {
        invokeIfBound(onBackToProjects);
    };

    addAndMakeVisible(commitMessageInput);
    stemhub::plugin::theme::styleTextInput(commitMessageInput, "Describe this save");
    commitMessageInput.setMultiLine(false);
    commitMessageInput.setReturnKeyStartsNewLine(false);
    commitMessageInput.setScrollbarsShown(false);
    commitMessageInput.setJustification(juce::Justification::centredLeft);
    commitMessageInput.setTooltip("Optional note to attach to this save action.");

    addAndMakeVisible(saveChanges);
    saveChanges.setButtonText("Save version");
    stemhub::plugin::theme::stylePrimaryButton(saveChanges);
    saveChanges.onClick = [this]
    {
        invokeIfBound(onSave);
    };

    addAndMakeVisible(syncButton);
    syncButton.setButtonText("Sync latest");
    stemhub::plugin::theme::styleSecondaryButton(syncButton);
    syncButton.onClick = [this]
    {
        invokeIfBound(onSync);
    };

    addAndMakeVisible(changeBranch);
    changeBranch.setButtonText("Load workspace");
    stemhub::plugin::theme::styleSecondaryButton(changeBranch);
    changeBranch.onClick = [this]
    {
        invokeIfBound(onBranchChange);
    };
    changeBranch.setVisible(false);

    addAndMakeVisible(signOutButton);
    signOutButton.setButtonText("Sign Out");
    stemhub::plugin::theme::styleGhostButton(signOutButton);
    signOutButton.onClick = [this]
    {
        invokeIfBound(onSignOut);
    };

    addAndMakeVisible(restoreButton);
    restoreButton.setButtonText("Restore selection");
    stemhub::plugin::theme::styleSecondaryButton(restoreButton);
    restoreButton.onClick = [this]
    {
        invokeIfBound(onRestore);
    };

    addAndMakeVisible(packagedFilesLabel);
    stemhub::plugin::theme::styleInfoLabel(packagedFilesLabel, "Files included in snapshot", true, false);

    addAndMakeVisible(packagedFilesTree);
    packagedFilesTree.setRootItemVisible(false);
    packagedFilesTree.setIndentSize(14);
    packagedFilesTree.setColour(juce::TreeView::backgroundColourId, stemhub::plugin::theme::PluginTheme::kSurfaceSoft.withAlpha(0.9f));
    packagedFilesTree.setColour(juce::TreeView::linesColourId, stemhub::plugin::theme::PluginTheme::kSurfaceBorder);
    packagedFilesTree.setRootItem(createPackagedFilesRootItem({}));
}

void DashboardView::setProjectStatusMessage(const juce::String& message,
                                           stemhub::plugin::theme::MessageStatus status)
{
    stemhub::plugin::theme::styleStatusLabel(projectStatusLabel, message, status);
    projectStatusLabel.setTooltip(message);
}

void DashboardView::setProjectNameMessage(const juce::String& message)
{
    projectNameLabel.setText(message, juce::dontSendNotification);
    projectNameLabel.setTooltip(message);
}

void DashboardView::setBranchNameMessage(const juce::String& message)
{
    branchNameLabel.setText(message, juce::dontSendNotification);
    branchNameLabel.setTooltip(message);
}

void DashboardView::setSelectedProjectFileMessage(const juce::String& message)
{
    projectFileLabel.setText(message, juce::dontSendNotification);
    projectFileLabel.setTooltip(message);
}

void DashboardView::setBranches(const std::vector<juce::String>& branchNames,
                                const std::vector<juce::String>& branchIds,
                                const juce::String& selectedBranchId)
{
    setMappedComboItems(branchComboBox, comboBranchIds, branchNames, branchIds, selectedBranchId);
}

void DashboardView::setVersions(const std::vector<juce::String>& versionLabels,
                                const std::vector<juce::String>& versionIds,
                                const juce::String& selectedVersionId)
{
    setMappedComboItems(versionComboBox, comboVersionIds, versionLabels, versionIds, selectedVersionId);
    versionComboBox.setTooltip(versionComboBox.getText());
}

void DashboardView::setPackagedFiles(const juce::String& rootLabel,
                                     const std::vector<juce::String>& relativeFilePaths)
{
    juce::ignoreUnused(rootLabel);

    auto labelText = juce::String("Files included in snapshot");
    if (!relativeFilePaths.empty())
        labelText += " (" + juce::String(static_cast<int>(relativeFilePaths.size())) + ")";

    packagedFilesLabel.setText(labelText, juce::dontSendNotification);
    packagedFilesTree.setRootItem(createPackagedFilesRootItem(relativeFilePaths));
    if (auto* rootItem = packagedFilesTree.getRootItem())
        rootItem->setOpen(true);

    packagedFilesTree.setTooltip(relativeFilePaths.empty()
                                   ? "No packaged files detected."
                                   : "Files that will be included when saving a version.");
}

juce::String DashboardView::getSelectedBranchId() const
{
    return getMappedComboSelection(branchComboBox, comboBranchIds);
}

juce::String DashboardView::getSelectedVersionId() const
{
    return getMappedComboSelection(versionComboBox, comboVersionIds);
}

void DashboardView::resized()
{
    auto area = getLocalBounds().reduced(14);
    const int contentLeft = area.getX();
    const int contentRight = area.getRight();

    auto topActionsRow = area.removeFromTop(24);
    const int backButtonWidth = 102;
    const int signOutButtonWidth = 78;
    backToProjectsButton.setBounds(contentLeft, topActionsRow.getY(), backButtonWidth, topActionsRow.getHeight());
    signOutButton.setBounds(contentRight - signOutButtonWidth, topActionsRow.getY(), signOutButtonWidth, topActionsRow.getHeight());

    area.removeFromTop(8);

    auto statusRow = area.removeFromTop(40);
    projectStatusLabel.setBounds(statusRow);

    area.removeFromTop(8);

    auto contentRow = area;
    const int leftColumnWidth = juce::jmin(360, juce::jmax(250, static_cast<int>(contentRow.getWidth() * 0.50f)));
    auto leftColumn = contentRow.removeFromLeft(leftColumnWidth);
    contentRow.removeFromLeft(12);
    auto rightColumn = contentRow;

    const int leftX = leftColumn.getX();
    const int controlWidth = juce::jmax(220, leftColumn.getWidth() - 8);
    const int controlHeight = 30;
    const int utilityButtonWidth = juce::jmax(90, (controlWidth - 8) / 2);

    auto projectFileHintRow = leftColumn.removeFromTop(36);
    projectFileLabel.setBounds(leftX, projectFileHintRow.getY(), controlWidth, projectFileHintRow.getHeight());

    auto projectNameRow = leftColumn.removeFromTop(22);
    projectNameLabel.setBounds(leftX, projectNameRow.getY(), controlWidth, projectNameRow.getHeight());

    auto branchNameRow = leftColumn.removeFromTop(20);
    branchNameLabel.setBounds(leftX, branchNameRow.getY(), controlWidth, branchNameRow.getHeight());

    auto branchRow = leftColumn.removeFromTop(controlHeight);
    branchComboBox.setBounds(leftX, branchRow.getY(), controlWidth, branchRow.getHeight());

    leftColumn.removeFromTop(8);
    auto versionRow = leftColumn.removeFromTop(controlHeight);
    versionComboBox.setBounds(leftX, versionRow.getY(), controlWidth, versionRow.getHeight());

    leftColumn.removeFromTop(10);
    auto commitMessageRow = leftColumn.removeFromTop(controlHeight);
    commitMessageInput.setBounds(leftX, commitMessageRow.getY(), controlWidth, commitMessageRow.getHeight());

    leftColumn.removeFromTop(10);
    auto saveRow = leftColumn.removeFromTop(controlHeight);
    saveChanges.setBounds(leftX, saveRow.getY(), controlWidth, saveRow.getHeight());

    leftColumn.removeFromTop(8);
    auto utilitiesRow = leftColumn.removeFromTop(controlHeight);
    syncButton.setBounds(leftX, utilitiesRow.getY(), utilityButtonWidth, utilitiesRow.getHeight());
    restoreButton.setBounds(leftX + utilityButtonWidth + 8,
                           utilitiesRow.getY(),
                           controlWidth - utilityButtonWidth - 8,
                           utilitiesRow.getHeight());

    auto rightTop = rightColumn.removeFromTop(22);
    packagedFilesLabel.setBounds(rightColumn.getX(), rightTop.getY(), rightColumn.getWidth(), rightTop.getHeight());
    rightColumn.removeFromTop(6);
    packagedFilesTree.setBounds(rightColumn);
}
