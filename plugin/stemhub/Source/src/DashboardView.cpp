#include "../include/Views.hpp"

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

    addAndMakeVisible(signOutButton);
    styleCompactTopButton(signOutButton);
    signOutButton.onClick = [this]
    {
        invokeIfBound(onSignOut);
    };
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
    auto area = getLocalBounds().reduced(20);
    const int fieldWidth = 280;
    const int x = (getWidth() - fieldWidth) / 2;
    const int contentLeft = area.getX();
    const int contentRight = area.getRight();

    auto topActionsRow = area.removeFromTop(22);
    const int backButtonWidth = 112;
    const int signOutButtonWidth = 46;
    backToProjectsButton.setBounds(contentLeft, topActionsRow.getY(), backButtonWidth, topActionsRow.getHeight());
    signOutButton.setBounds(contentRight - signOutButtonWidth, topActionsRow.getY(), signOutButtonWidth, topActionsRow.getHeight());

    area.removeFromTop(4);

    auto projectStatusRow = area.removeFromTop(24);
    projectStatusLabel.setBounds(x, projectStatusRow.getY(), fieldWidth, projectStatusRow.getHeight());

    area.removeFromTop(4);

    auto projectNameRow = area.removeFromTop(22);
    projectNameLabel.setBounds(x - 80, projectNameRow.getY(), fieldWidth + 160, projectNameRow.getHeight());

    area.removeFromTop(1);

    auto branchNameRow = area.removeFromTop(22);
    branchNameLabel.setBounds(x - 80, branchNameRow.getY(), fieldWidth + 160, branchNameRow.getHeight());

    area.removeFromTop(4);

    auto projectFileRow = area.removeFromTop(28);
    projectFileLabel.setBounds(x - 80, projectFileRow.getY(), fieldWidth + 160, projectFileRow.getHeight());

    area.removeFromTop(4);

    auto branchLabelRow = area.removeFromTop(18);
    branchLabel.setBounds(x, branchLabelRow.getY(), fieldWidth, branchLabelRow.getHeight());

    area.removeFromTop(1);

    auto branchComboRow = area.removeFromTop(24);
    branchComboBox.setBounds(x, branchComboRow.getY(), fieldWidth, branchComboRow.getHeight());

    area.removeFromTop(4);

    auto branchRow = area.removeFromTop(26);
    changeBranch.setBounds(x, branchRow.getY(), fieldWidth, branchRow.getHeight());

    area.removeFromTop(5);

    auto versionLabelRow = area.removeFromTop(18);
    versionLabel.setBounds(x, versionLabelRow.getY(), fieldWidth, versionLabelRow.getHeight());

    area.removeFromTop(1);

    auto versionComboRow = area.removeFromTop(24);
    versionComboBox.setBounds(x, versionComboRow.getY(), fieldWidth, versionComboRow.getHeight());

    area.removeFromTop(4);

    auto commitMessageLabelRow = area.removeFromTop(18);
    commitMessageLabel.setBounds(x, commitMessageLabelRow.getY(), fieldWidth, commitMessageLabelRow.getHeight());

    area.removeFromTop(1);

    auto commitMessageRow = area.removeFromTop(24);
    commitMessageInput.setBounds(x, commitMessageRow.getY(), fieldWidth, commitMessageRow.getHeight());

    area.removeFromTop(4);

    auto saveRow = area.removeFromTop(26);
    saveChanges.setBounds(x, saveRow.getY(), fieldWidth, saveRow.getHeight());

    area.removeFromTop(4);

    auto syncRow = area.removeFromTop(26);
    syncButton.setBounds(x, syncRow.getY(), fieldWidth, syncRow.getHeight());
}
