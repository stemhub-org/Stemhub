#include "../include/Views.hpp"

ProjectSelectionView::ProjectSelectionView()
{
    addAndMakeVisible(statusLabel);
    statusLabel.setJustificationType(juce::Justification::centred);
    statusLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    statusLabel.setFont(juce::FontOptions(20.0f, juce::Font::bold));

    addAndMakeVisible(projectFileLabel);
    projectFileLabel.setJustificationType(juce::Justification::centredLeft);
    projectFileLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    projectFileLabel.setFont(juce::FontOptions(14.0f, juce::Font::plain));
    projectFileLabel.setText("No project file selected.", juce::dontSendNotification);

    addAndMakeVisible(existingProjectsLabel);
    existingProjectsLabel.setText("Existing projects", juce::dontSendNotification);
    existingProjectsLabel.setJustificationType(juce::Justification::centredLeft);

    addAndMakeVisible(projectComboBox);
    projectComboBox.setTextWhenNothingSelected("Select a project");

    addAndMakeVisible(chooseProjectFileButton);
    chooseProjectFileButton.onClick = [this]
    {
        if (onChooseProjectFile != nullptr)
            onChooseProjectFile();
    };

    addAndMakeVisible(openProjectButton);
    openProjectButton.onClick = [this]
    {
        if (onOpenProject != nullptr)
            onOpenProject();
    };

    addAndMakeVisible(createProjectButton);
    createProjectButton.onClick = [this]
    {
        if (onCreateProject != nullptr)
            onCreateProject();
    };

    addAndMakeVisible(signOutButton);
    signOutButton.onClick = [this]
    {
        if (onSignOut != nullptr)
            onSignOut();
    };
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
}

void ProjectSelectionView::setHasExistingProjects(bool hasProjects)
{
    hasExistingProjects = hasProjects;
    existingProjectsLabel.setVisible(hasExistingProjects);
    projectComboBox.setVisible(hasExistingProjects);
    openProjectButton.setVisible(hasExistingProjects);
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

    auto titleRow = area.removeFromTop(56);
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

    auto createRow = area.removeFromTop(32);
    createProjectButton.setBounds(x, createRow.getY(), fieldWidth, createRow.getHeight());

    area.removeFromTop(20);

    auto signOutRow = area.removeFromTop(32);
    signOutButton.setBounds(x, signOutRow.getY(), fieldWidth, signOutRow.getHeight());
}

DashboardView::DashboardView()
{
    addAndMakeVisible(projectStatusLabel);
    projectStatusLabel.setJustificationType(juce::Justification::centredLeft);
    projectStatusLabel.setColour(juce::Label::textColourId, juce::Colours::yellowgreen);
    projectStatusLabel.setFont(juce::FontOptions(16.0f, juce::Font::plain));

    addAndMakeVisible(projectFileLabel);
    projectFileLabel.setJustificationType(juce::Justification::centredLeft);
    projectFileLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    projectFileLabel.setFont(juce::FontOptions(14.0f, juce::Font::plain));
    projectFileLabel.setText("No project file selected.", juce::dontSendNotification);

    addAndMakeVisible(currentProjectLabel);
    currentProjectLabel.setJustificationType(juce::Justification::centredLeft);
    currentProjectLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    currentProjectLabel.setFont(juce::FontOptions(15.0f, juce::Font::plain));

    addAndMakeVisible(saveChanges);
    saveChanges.onClick = [this]
    {
        if (onSave != nullptr)
            onSave();
    };

    addAndMakeVisible(syncButton);
    syncButton.onClick = [this]
    {
        if (onSync != nullptr)
            onSync();
    };

    addAndMakeVisible(changeBranch);
    changeBranch.onClick = [this]
    {
        if (onBranchChange != nullptr)
            onBranchChange();
    };

    addAndMakeVisible(signOutButton);
    signOutButton.onClick = [this]
    {
        if (onSignOut != nullptr)
            onSignOut();
    };
}

void DashboardView::resized()
{
    auto area = getLocalBounds().reduced(20);
    const int fieldWidth = 220;
    const int x = (getWidth() - fieldWidth) / 2;

    auto projectStatusRow = area.removeFromTop(32);
    projectStatusLabel.setBounds(x, projectStatusRow.getY(), fieldWidth, projectStatusRow.getHeight());

    area.removeFromTop(8);

    auto currentProjectRow = area.removeFromTop(28);
    currentProjectLabel.setBounds(x - 80, currentProjectRow.getY(), fieldWidth + 160, currentProjectRow.getHeight());

    area.removeFromTop(8);

    auto projectFileRow = area.removeFromTop(40);
    projectFileLabel.setBounds(x - 80, projectFileRow.getY(), fieldWidth + 160, projectFileRow.getHeight());

    area.removeFromTop(8);

    auto saveRow = area.removeFromTop(32);
    saveChanges.setBounds(x, saveRow.getY(), fieldWidth, saveRow.getHeight());

    area.removeFromTop(8);

    auto syncRow = area.removeFromTop(32);
    syncButton.setBounds(x, syncRow.getY(), fieldWidth, syncRow.getHeight());

    area.removeFromTop(8);

    auto branchRow = area.removeFromTop(32);
    changeBranch.setBounds(x, branchRow.getY(), fieldWidth, branchRow.getHeight());

    area.removeFromTop(24);

    auto signOutRow = area.removeFromTop(32);
    signOutButton.setBounds(x, signOutRow.getY(), fieldWidth, signOutRow.getHeight());
}
