#include "ui/Views.hpp"
#include <algorithm>
#include <utility>

namespace
{
const auto kStemhubPurple = juce::Colour::fromRGB(0x9C, 0x57, 0xDF);
const auto kStemhubDark = juce::Colour::fromRGB(0x1E, 0x1E, 0x1E);
const auto kStemhubLight = juce::Colour::fromRGB(0xF1, 0xF1, 0xF1);
const auto kStemhubSurface = juce::Colour::fromRGB(0x2B, 0x2B, 0x30);

void invokeIfBound(const std::function<void()>& callback)
{
    if (callback != nullptr)
        callback();
}

void styleComboBox(juce::ComboBox& combo)
{
    combo.setColour(juce::ComboBox::backgroundColourId, kStemhubSurface);
    combo.setColour(juce::ComboBox::textColourId, kStemhubLight);
    combo.setColour(juce::ComboBox::outlineColourId, kStemhubLight.withAlpha(0.45f));
    combo.setColour(juce::ComboBox::arrowColourId, kStemhubLight);
}

void stylePrimaryButton(juce::TextButton& button)
{
    button.setColour(juce::TextButton::buttonColourId, kStemhubPurple);
    button.setColour(juce::TextButton::buttonOnColourId, kStemhubPurple.brighter(0.15f));
    button.setColour(juce::TextButton::textColourOffId, kStemhubLight);
    button.setColour(juce::TextButton::textColourOnId, kStemhubLight);
}

void styleSecondaryButton(juce::TextButton& button)
{
    button.setColour(juce::TextButton::buttonColourId, kStemhubSurface.withAlpha(0.95f));
    button.setColour(juce::TextButton::buttonOnColourId, kStemhubSurface.brighter(0.12f));
    button.setColour(juce::TextButton::textColourOffId, kStemhubLight);
    button.setColour(juce::TextButton::textColourOnId, kStemhubLight);
}

void styleCompactTopButton(juce::TextButton& button)
{
    button.setColour(juce::TextButton::buttonColourId, kStemhubDark.withAlpha(0.85f));
    button.setColour(juce::TextButton::buttonOnColourId, kStemhubPurple.withAlpha(0.8f));
    button.setColour(juce::TextButton::textColourOffId, kStemhubLight.withAlpha(0.9f));
    button.setColour(juce::TextButton::textColourOnId, kStemhubLight);
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
            g.fillAll(kStemhubPurple.withAlpha(0.18f));

        g.setColour(kStemhubLight.withAlpha(isDirectory ? 0.95f : 0.78f));
        g.setFont(juce::FontOptions(isDirectory ? 13.0f : 12.5f,
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
    statusLabel.setJustificationType(juce::Justification::centred);
    statusLabel.setColour(juce::Label::textColourId, kStemhubLight);
    statusLabel.setFont(juce::FontOptions(20.0f, juce::Font::bold));

    addAndMakeVisible(projectFileLabel);
    projectFileLabel.setJustificationType(juce::Justification::centredLeft);
    projectFileLabel.setColour(juce::Label::textColourId, kStemhubLight.withAlpha(0.8f));
    projectFileLabel.setFont(juce::FontOptions(14.0f, juce::Font::plain));
    projectFileLabel.setText("No project file selected.", juce::dontSendNotification);

    addAndMakeVisible(existingProjectsLabel);
    existingProjectsLabel.setText("Existing projects", juce::dontSendNotification);
    existingProjectsLabel.setJustificationType(juce::Justification::centredLeft);
    existingProjectsLabel.setColour(juce::Label::textColourId, kStemhubLight);

    addAndMakeVisible(projectComboBox);
    projectComboBox.setTextWhenNothingSelected("Select a project");
    styleComboBox(projectComboBox);

    addAndMakeVisible(chooseProjectFileButton);
    styleSecondaryButton(chooseProjectFileButton);
    chooseProjectFileButton.onClick = [this]
    {
        invokeIfBound(onChooseProjectFile);
    };

    addAndMakeVisible(openProjectButton);
    stylePrimaryButton(openProjectButton);
    openProjectButton.onClick = [this]
    {
        invokeIfBound(onOpenProject);
    };

    addAndMakeVisible(createProjectButton);
    stylePrimaryButton(createProjectButton);
    createProjectButton.onClick = [this]
    {
        invokeIfBound(onCreateProject);
    };
    createProjectButton.setVisible(false);

    addAndMakeVisible(signOutButton);
    signOutButton.onClick = [this]
    {
        invokeIfBound(onSignOut);
    };

    styleCompactTopButton(signOutButton);
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

void ProjectSelectionView::setHasExistingProjects(bool hasProjects)
{
    hasExistingProjects = hasProjects;
    existingProjectsLabel.setVisible(hasExistingProjects);
    projectComboBox.setVisible(hasExistingProjects);
    openProjectButton.setVisible(hasExistingProjects);
    resized();
}

void ProjectSelectionView::setCanCreateProject(bool canCreate)
{
    canCreateProject = canCreate;
    createProjectButton.setVisible(canCreateProject);
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
    auto area = getLocalBounds().reduced(20);
    const int fieldWidth = 260;
    const int x = (getWidth() - fieldWidth) / 2;
    const int contentRight = area.getRight();

    auto topActionsRow = area.removeFromTop(22);
    const int signOutButtonWidth = 46;
    signOutButton.setBounds(contentRight - signOutButtonWidth, topActionsRow.getY(), signOutButtonWidth, topActionsRow.getHeight());

    area.removeFromTop(8);

    auto titleRow = area.removeFromTop(48);
    statusLabel.setBounds(titleRow);

    area.removeFromTop(16);

    auto fileRow = area.removeFromTop(40);
    projectFileLabel.setBounds(x - 80, fileRow.getY(), fieldWidth + 160, fileRow.getHeight());

    area.removeFromTop(8);

    auto chooseRow = area.removeFromTop(32);
    chooseProjectFileButton.setBounds(x, chooseRow.getY(), fieldWidth, chooseRow.getHeight());

    area.removeFromTop(16);

    if (hasExistingProjects)
    {
        auto existingLabelRow = area.removeFromTop(24);
        existingProjectsLabel.setBounds(x, existingLabelRow.getY(), fieldWidth, existingLabelRow.getHeight());

        area.removeFromTop(4);

        auto comboRow = area.removeFromTop(28);
        projectComboBox.setBounds(x, comboRow.getY(), fieldWidth, comboRow.getHeight());

        area.removeFromTop(8);

        auto openRow = area.removeFromTop(32);
        openProjectButton.setBounds(x, openRow.getY(), fieldWidth, openRow.getHeight());

        area.removeFromTop(16);
    }

    if (canCreateProject)
    {
        auto createRow = area.removeFromTop(32);
        createProjectButton.setBounds(x, createRow.getY(), fieldWidth, createRow.getHeight());
        area.removeFromTop(20);
    }
    else
    {
        area.removeFromTop(8);
    }
}

DashboardView::DashboardView()
{
    addAndMakeVisible(projectStatusLabel);
    projectStatusLabel.setJustificationType(juce::Justification::centredLeft);
    projectStatusLabel.setColour(juce::Label::textColourId, kStemhubPurple.brighter(0.25f));
    projectStatusLabel.setFont(juce::FontOptions(16.0f, juce::Font::plain));

    addAndMakeVisible(projectFileLabel);
    projectFileLabel.setJustificationType(juce::Justification::centredLeft);
    projectFileLabel.setColour(juce::Label::textColourId, kStemhubLight.withAlpha(0.8f));
    projectFileLabel.setFont(juce::FontOptions(14.0f, juce::Font::plain));
    projectFileLabel.setText("No project file selected.", juce::dontSendNotification);

    addAndMakeVisible(projectNameLabel);
    projectNameLabel.setJustificationType(juce::Justification::centredLeft);
    projectNameLabel.setColour(juce::Label::textColourId, kStemhubLight);
    projectNameLabel.setFont(juce::FontOptions(15.0f, juce::Font::bold));

    addAndMakeVisible(branchNameLabel);
    branchNameLabel.setJustificationType(juce::Justification::centredLeft);
    branchNameLabel.setColour(juce::Label::textColourId, kStemhubLight.withAlpha(0.85f));
    branchNameLabel.setFont(juce::FontOptions(14.0f, juce::Font::plain));

    addAndMakeVisible(branchLabel);
    branchLabel.setText("Branch", juce::dontSendNotification);
    branchLabel.setJustificationType(juce::Justification::centredLeft);
    branchLabel.setColour(juce::Label::textColourId, kStemhubLight);

    addAndMakeVisible(branchComboBox);
    branchComboBox.setTextWhenNothingSelected("No branch available");
    styleComboBox(branchComboBox);
    branchComboBox.onChange = [this]
    {
        invokeIfBound(onBranchChange);
    };

    addAndMakeVisible(versionLabel);
    versionLabel.setText("Version history", juce::dontSendNotification);
    versionLabel.setJustificationType(juce::Justification::centredLeft);
    versionLabel.setColour(juce::Label::textColourId, kStemhubLight);

    addAndMakeVisible(versionComboBox);
    versionComboBox.setTextWhenNothingSelected("No versions available");
    styleComboBox(versionComboBox);
    versionComboBox.onChange = [this]
    {
        invokeIfBound(onVersionSelectionChange);
    };

    addAndMakeVisible(currentVersionLabel);
    currentVersionLabel.setText("Current version: not set", juce::dontSendNotification);
    currentVersionLabel.setJustificationType(juce::Justification::centredLeft);
    currentVersionLabel.setColour(juce::Label::textColourId, kStemhubLight.withAlpha(0.8f));
    currentVersionLabel.setFont(juce::FontOptions(12.5f, juce::Font::plain));

    addAndMakeVisible(currentVersionFileLabel);
    currentVersionFileLabel.setText("Opened file: not set", juce::dontSendNotification);
    currentVersionFileLabel.setJustificationType(juce::Justification::centredLeft);
    currentVersionFileLabel.setColour(juce::Label::textColourId, kStemhubLight.withAlpha(0.6f));
    currentVersionFileLabel.setFont(juce::FontOptions(10.5f, juce::Font::plain));

    addAndMakeVisible(backToProjectsButton);
    styleCompactTopButton(backToProjectsButton);
    backToProjectsButton.onClick = [this]
    {
        invokeIfBound(onBackToProjects);
    };

    addAndMakeVisible(commitMessageLabel);
    commitMessageLabel.setText("Commit message", juce::dontSendNotification);
    commitMessageLabel.setJustificationType(juce::Justification::centredLeft);
    commitMessageLabel.setColour(juce::Label::textColourId, kStemhubLight);

    addAndMakeVisible(commitMessageInput);
    commitMessageInput.setTextToShowWhenEmpty("Describe this save", juce::Colours::grey);
    commitMessageInput.setMultiLine(false);
    commitMessageInput.setReturnKeyStartsNewLine(false);
    commitMessageInput.setScrollbarsShown(false);
    commitMessageInput.setJustification(juce::Justification::centredLeft);
    commitMessageInput.setColour(juce::TextEditor::backgroundColourId, kStemhubSurface);
    commitMessageInput.setColour(juce::TextEditor::textColourId, kStemhubLight);
    commitMessageInput.setColour(juce::TextEditor::outlineColourId, kStemhubLight.withAlpha(0.45f));
    commitMessageInput.setColour(juce::CaretComponent::caretColourId, kStemhubLight);

    addAndMakeVisible(saveChanges);
    stylePrimaryButton(saveChanges);
    saveChanges.onClick = [this]
    {
        invokeIfBound(onSave);
    };

    addAndMakeVisible(syncButton);
    styleSecondaryButton(syncButton);
    syncButton.onClick = [this]
    {
        invokeIfBound(onSync);
    };

    addAndMakeVisible(changeBranch);
    styleSecondaryButton(changeBranch);
    changeBranch.onClick = [this]
    {
        invokeIfBound(onBranchChange);
    };
    changeBranch.setVisible(false);

    addAndMakeVisible(signOutButton);
    styleCompactTopButton(signOutButton);
    signOutButton.onClick = [this]
    {
        invokeIfBound(onSignOut);
    };

    addAndMakeVisible(restoreButton);
    styleSecondaryButton(restoreButton);
    restoreButton.onClick = [this]
    {
        invokeIfBound(onRestore);
    };

    addAndMakeVisible(packagedFilesLabel);
    packagedFilesLabel.setText("Packaged files", juce::dontSendNotification);
    packagedFilesLabel.setJustificationType(juce::Justification::centredLeft);
    packagedFilesLabel.setColour(juce::Label::textColourId, kStemhubLight.withAlpha(0.9f));
    packagedFilesLabel.setFont(juce::FontOptions(14.0f, juce::Font::bold));

    addAndMakeVisible(packagedFilesTree);
    packagedFilesTree.setRootItemVisible(false);
    packagedFilesTree.setIndentSize(14);
    packagedFilesTree.setColour(juce::TreeView::backgroundColourId, kStemhubSurface.withAlpha(0.88f));
    packagedFilesTree.setRootItem(createPackagedFilesRootItem({}));
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
}

void DashboardView::setPackagedFiles(const juce::String& rootLabel,
                                     const std::vector<juce::String>& relativeFilePaths)
{
    auto labelText = juce::String("Packaged files");
    if (!relativeFilePaths.empty())
        labelText += " (" + juce::String(static_cast<int>(relativeFilePaths.size())) + ")";

    packagedFilesLabel.setText(labelText, juce::dontSendNotification);
    packagedFilesTree.setRootItem(createPackagedFilesRootItem(relativeFilePaths));
    if (auto* rootItem = packagedFilesTree.getRootItem())
        rootItem->setOpen(true);

    packagedFilesTree.setTooltip(rootLabel.isNotEmpty()
        ? "Bundle root: " + rootLabel
        : "No project root selected.");
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
    auto area = getLocalBounds().reduced(18);
    const int contentLeft = area.getX();
    const int contentRight = area.getRight();

    auto topActionsRow = area.removeFromTop(24);
    const int backButtonWidth = 104;
    const int signOutButtonWidth = 78;
    backToProjectsButton.setBounds(contentLeft, topActionsRow.getY(), backButtonWidth, topActionsRow.getHeight());
    signOutButton.setBounds(contentRight - signOutButtonWidth, topActionsRow.getY(), signOutButtonWidth, topActionsRow.getHeight());

    area.removeFromTop(6);

    auto projectStatusRow = area.removeFromTop(22);
    projectStatusLabel.setBounds(projectStatusRow);

    area.removeFromTop(8);

    auto contentRow = area;
    const int leftColumnWidth = juce::jmin(320, juce::jmax(260, static_cast<int>(contentRow.getWidth() * 0.46f)));
    auto leftColumn = contentRow.removeFromLeft(leftColumnWidth);
    contentRow.removeFromLeft(12);
    auto rightColumn = contentRow;

    const int controlX = leftColumn.getX();
    const int controlWidth = juce::jmax(210, leftColumn.getWidth() - 8);
    const int controlHeight = 24;

    auto projectNameRow = leftColumn.removeFromTop(20);
    projectNameLabel.setBounds(controlX, projectNameRow.getY(), controlWidth, projectNameRow.getHeight());

    auto branchNameRow = leftColumn.removeFromTop(20);
    branchNameLabel.setBounds(controlX, branchNameRow.getY(), controlWidth, branchNameRow.getHeight());

    leftColumn.removeFromTop(2);
    auto projectFileRow = leftColumn.removeFromTop(22);
    projectFileLabel.setBounds(controlX, projectFileRow.getY(), controlWidth, projectFileRow.getHeight());

    leftColumn.removeFromTop(8);
    auto branchLabelRow = leftColumn.removeFromTop(18);
    branchLabel.setBounds(controlX, branchLabelRow.getY(), controlWidth, branchLabelRow.getHeight());

    auto branchRow = leftColumn.removeFromTop(controlHeight);
    branchComboBox.setBounds(controlX, branchRow.getY(), controlWidth, branchRow.getHeight());
    changeBranch.setBounds(0, 0, 0, 0);

    leftColumn.removeFromTop(6);
    auto versionLabelRow = leftColumn.removeFromTop(18);
    versionLabel.setBounds(controlX, versionLabelRow.getY(), controlWidth, versionLabelRow.getHeight());

    auto versionRow = leftColumn.removeFromTop(controlHeight);
    versionComboBox.setBounds(controlX, versionRow.getY(), controlWidth, versionRow.getHeight());

    leftColumn.removeFromTop(6);
    auto currentVersionIdRow = leftColumn.removeFromTop(18);
    currentVersionLabel.setBounds(controlX, currentVersionIdRow.getY(), controlWidth, currentVersionIdRow.getHeight());

    leftColumn.removeFromTop(4);
    auto currentVersionFileRow = leftColumn.removeFromTop(30);
    currentVersionFileLabel.setBounds(controlX, currentVersionFileRow.getY(), controlWidth, currentVersionFileRow.getHeight());

    leftColumn.removeFromTop(6);
    auto commitMessageLabelRow = leftColumn.removeFromTop(18);
    commitMessageLabel.setBounds(controlX, commitMessageLabelRow.getY(), controlWidth, commitMessageLabelRow.getHeight());

    auto commitMessageRow = leftColumn.removeFromTop(controlHeight);
    commitMessageInput.setBounds(controlX, commitMessageRow.getY(), controlWidth, commitMessageRow.getHeight());

    leftColumn.removeFromTop(8);
    auto saveRow = leftColumn.removeFromTop(controlHeight + 2);
    saveChanges.setBounds(controlX, saveRow.getY(), controlWidth, saveRow.getHeight());

    leftColumn.removeFromTop(6);
    auto utilitiesRow = leftColumn.removeFromTop(controlHeight);
    const int utilityButtonWidth = juce::jmax(100, (controlWidth - 6) / 2);
    syncButton.setBounds(controlX, utilitiesRow.getY(), utilityButtonWidth, utilitiesRow.getHeight());
    restoreButton.setBounds(controlX + utilityButtonWidth + 6,
                            utilitiesRow.getY(),
                            controlWidth - utilityButtonWidth - 6,
                            utilitiesRow.getHeight());

    auto filesLabelRow = rightColumn.removeFromTop(18);
    packagedFilesLabel.setBounds(rightColumn.getX(), filesLabelRow.getY(), rightColumn.getWidth(), filesLabelRow.getHeight());
    rightColumn.removeFromTop(4);
    packagedFilesTree.setBounds(rightColumn);
}
