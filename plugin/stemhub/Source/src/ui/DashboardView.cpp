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

juce::String makeSlug(const juce::String& text)
{
    juce::String slug;
    bool previousWasDash = false;

    for (const auto character : text.toLowerCase())
    {
        if ((character >= 'a' && character <= 'z') || (character >= '0' && character <= '9'))
        {
            slug += juce::String::charToString(character);
            previousWasDash = false;
        }
        else if (!previousWasDash)
        {
            slug += "-";
            previousWasDash = true;
        }
    }

    return slug.trimCharactersAtStart("-").trimCharactersAtEnd("-");
}

juce::String shortenPath(const juce::String& path, int maxLength = 42)
{
    if (path.length() <= maxLength)
        return path;

    const auto tailLength = juce::jmax(12, maxLength / 2);
    const auto headLength = juce::jmax(10, maxLength - tailLength - 3);
    return path.substring(0, headLength) + "..." + path.substring(path.length() - tailLength);
}

juce::String extractVersionShortId(const juce::String& label)
{
    const auto separator = label.indexOf(" - ");
    return separator > 0 ? label.substring(0, separator) : label.substring(0, juce::jmin(8, label.length()));
}

juce::String extractVersionTitle(const juce::String& label)
{
    const auto separator = label.indexOf(" - ");
    const auto suffix = label.lastIndexOf(" (");

    if (separator < 0)
        return label;

    if (suffix > separator)
        return label.substring(separator + 3, suffix);

    return label.substring(separator + 3);
}

bool isGenericSnapshotTitle(const juce::String& title)
{
    const auto trimmed = title.trim();
    return trimmed.isEmpty()
        || trimmed.equalsIgnoreCase("save from plugin")
        || trimmed.equalsIgnoreCase("no save note");
}

juce::String extractVersionTimestamp(const juce::String& label)
{
    const auto suffix = label.lastIndexOf(" (");
    if (suffix < 0 || !label.endsWithChar(')'))
        return {};

    return label.substring(suffix + 2, label.length() - 1);
}

juce::String makeStatusChipText(stemhub::plugin::theme::MessageStatus status, const juce::String& message)
{
    juce::ignoreUnused(message);

    switch (status)
    {
        case stemhub::plugin::theme::MessageStatus::loading:
            return "Syncing";
        case stemhub::plugin::theme::MessageStatus::success:
            return "Synced";
        case stemhub::plugin::theme::MessageStatus::warning:
            return "Attention";
        case stemhub::plugin::theme::MessageStatus::error:
            return "Error";
        case stemhub::plugin::theme::MessageStatus::disabled:
            return "Disabled";
        case stemhub::plugin::theme::MessageStatus::neutral:
        default:
            return "Ready";
    }
}

void styleSectionLabel(juce::Label& label, const juce::String& text)
{
    label.setText(text, juce::dontSendNotification);
    label.setFont(stemhub::plugin::theme::bodyFont(12.0f, juce::Font::bold));
    label.setColour(juce::Label::textColourId, stemhub::plugin::theme::PluginTheme::kForegroundTertiary);
    label.setJustificationType(juce::Justification::centredLeft);
}

void styleBodyLabel(juce::Label& label, const juce::String& text, bool muted = false, bool mono = false)
{
    label.setText(text, juce::dontSendNotification);
    label.setFont(mono ? stemhub::plugin::theme::monoFont(11.5f)
                       : stemhub::plugin::theme::bodyFont(12.5f));
    label.setColour(juce::Label::textColourId,
                    muted ? stemhub::plugin::theme::PluginTheme::kForegroundSubtle
                          : stemhub::plugin::theme::PluginTheme::kForeground);
    label.setJustificationType(juce::Justification::centredLeft);
}

void styleFilterButton(juce::TextButton& button, bool selected)
{
    if (selected)
        stemhub::plugin::theme::stylePrimaryButton(button);
    else
        stemhub::plugin::theme::styleSecondaryButton(button);

    button.setButtonText(button.getButtonText());
}

struct ProjectCardData
{
    juce::String name;
    juce::String path;
    juce::String badge;
    bool selected { false };
    bool enabled { true };
};

juce::String normalizeProjectCardLine(const juce::String& value)
{
    auto normalized = value.toLowerCase().trim();
    normalized = normalized.upToLastOccurrenceOf(".", false, false);
    normalized = normalized.replaceCharacters("_-", "  ");
    normalized = normalized.retainCharacters("abcdefghijklmnopqrstuvwxyz0123456789 ");

    while (normalized.contains("  "))
        normalized = normalized.replace("  ", " ");

    return normalized.trim();
}

class ProjectCardComponent final : public juce::Component
{
public:
    std::function<void()> onSelect;
    std::function<void()> onOpen;

    void setData(ProjectCardData nextData)
    {
        data = std::move(nextData);
        repaint();
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat().reduced(0.5f);
        g.setColour(data.selected ? stemhub::plugin::theme::PluginTheme::kSurfaceElevated
                                  : stemhub::plugin::theme::PluginTheme::kSurface);
        g.fillRoundedRectangle(bounds, 10.0f);

        g.setColour(data.selected ? stemhub::plugin::theme::PluginTheme::kAccent
                                  : stemhub::plugin::theme::PluginTheme::kSurfaceBorder);
        g.drawRoundedRectangle(bounds, 10.0f, data.selected ? 1.2f : 1.0f);

        if (data.selected)
        {
            g.setColour(stemhub::plugin::theme::PluginTheme::kAccent);
            g.fillRoundedRectangle(bounds.removeFromLeft(4.0f), 4.0f);
        }

        auto content = getLocalBounds().reduced(18, 14);
        auto titleArea = content.removeFromTop(28);
        g.setColour(stemhub::plugin::theme::PluginTheme::kForeground);
        g.setFont(stemhub::plugin::theme::headingFont(15.5f));
        g.drawText(data.name, titleArea, juce::Justification::centredLeft, true);

        const auto normalizedName = normalizeProjectCardLine(data.name);
        const auto normalizedPath = normalizeProjectCardLine(data.path);
        if (data.path.isNotEmpty() && normalizedPath.isNotEmpty() && normalizedPath != normalizedName)
        {
            content.removeFromTop(4);
            auto pathArea = content.removeFromTop(18);
            g.setColour(stemhub::plugin::theme::PluginTheme::kForegroundTertiary);
            g.setFont(stemhub::plugin::theme::monoFont(10.8f));
            g.drawText(shortenPath(data.path), pathArea, juce::Justification::centredLeft, true);
        }

        auto badgeBounds = juce::Rectangle<float>(88.0f, 30.0f)
                               .withCentre({ static_cast<float>(getWidth()) - 94.0f, static_cast<float>(getHeight()) - 28.0f });
        g.setColour(data.enabled ? stemhub::plugin::theme::PluginTheme::kSuccessSubtle.withAlpha(0.95f)
                                 : stemhub::plugin::theme::PluginTheme::kWarningSubtle.withAlpha(0.95f));
        g.fillRoundedRectangle(badgeBounds, 8.0f);
        g.setColour(data.enabled ? stemhub::plugin::theme::PluginTheme::kSuccess
                                 : stemhub::plugin::theme::PluginTheme::kWarning);
        g.setFont(stemhub::plugin::theme::bodyFont(10.8f, juce::Font::bold));
        g.drawText(data.badge, badgeBounds.toNearestInt(), juce::Justification::centred, false);

        g.setColour(data.selected ? stemhub::plugin::theme::PluginTheme::kAccent
                                  : stemhub::plugin::theme::PluginTheme::kForegroundSubtle);
        g.setFont(stemhub::plugin::theme::headingFont(16.0f));
        g.drawText(">", getWidth() - 38, 16, 18, 18, juce::Justification::centred, false);
    }

    void mouseUp(const juce::MouseEvent&) override
    {
        invokeIfBound(onSelect);
    }

    void mouseDoubleClick(const juce::MouseEvent&) override
    {
        juce::ignoreUnused(onOpen);
    }

private:
    ProjectCardData data;
};

struct VersionCardData
{
    juce::String title;
    juce::String meta;
    bool selected { false };
};

class VersionCardComponent final : public juce::Component
{
public:
    std::function<void()> onSelect;

    void setData(VersionCardData nextData)
    {
        data = std::move(nextData);
        repaint();
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();

        g.setColour(data.selected ? stemhub::plugin::theme::PluginTheme::kSurfaceElevated
                                  : stemhub::plugin::theme::PluginTheme::kBackground.darker(0.06f));
        g.fillRect(bounds);

        g.setColour(stemhub::plugin::theme::PluginTheme::kSurfaceBorder);
        g.drawLine(0.0f, bounds.getBottom() - 1.0f, bounds.getRight(), bounds.getBottom() - 1.0f, 1.0f);

        if (data.selected)
        {
            g.setColour(stemhub::plugin::theme::PluginTheme::kAccent);
            g.fillRect(0.0f, 0.0f, 4.0f, bounds.getHeight());
        }

        auto content = getLocalBounds().reduced(16, 11);
        g.setColour(data.selected ? stemhub::plugin::theme::PluginTheme::kAccent
                                  : stemhub::plugin::theme::PluginTheme::kForegroundTertiary);
        g.setFont(stemhub::plugin::theme::monoFont(10.0f));
        g.drawText("--", content.removeFromLeft(28), juce::Justification::centredLeft, false);

        auto textArea = content;
        auto titleArea = textArea.removeFromTop(24);
        g.setColour(stemhub::plugin::theme::PluginTheme::kForeground);
        g.setFont(stemhub::plugin::theme::headingFont(14.0f));
        g.drawText(data.title, titleArea, juce::Justification::centredLeft, true);

        g.setColour(stemhub::plugin::theme::PluginTheme::kForegroundSubtle);
        g.setFont(stemhub::plugin::theme::monoFont(10.5f));
        g.drawText(data.meta, textArea.removeFromTop(18), juce::Justification::centredLeft, true);
    }

    void mouseUp(const juce::MouseEvent&) override
    {
        invokeIfBound(onSelect);
    }

private:
    VersionCardData data;
};
}

ProjectSelectionView::ProjectSelectionView()
{
    addAndMakeVisible(titleLabel);
    titleLabel.setText("Your Projects", juce::dontSendNotification);
    titleLabel.setFont(stemhub::plugin::theme::headingFont(22.0f));
    titleLabel.setColour(juce::Label::textColourId, stemhub::plugin::theme::PluginTheme::kForeground);
    titleLabel.setJustificationType(juce::Justification::centredLeft);

    addAndMakeVisible(subtitleLabel);
    subtitleLabel.setText("Select a project to continue", juce::dontSendNotification);
    subtitleLabel.setFont(stemhub::plugin::theme::bodyFont(12.5f));
    subtitleLabel.setColour(juce::Label::textColourId, stemhub::plugin::theme::PluginTheme::kForegroundTertiary);
    subtitleLabel.setJustificationType(juce::Justification::centredLeft);

    addAndMakeVisible(statusLabel);
    stemhub::plugin::theme::styleStatusLabel(statusLabel, {}, stemhub::plugin::theme::MessageStatus::neutral);
    statusLabel.setVisible(false);

    addAndMakeVisible(projectFileLabel);
    styleBodyLabel(projectFileLabel, "Choose a DAW project file to create new projects.", true, true);

    addAndMakeVisible(searchInput);
    stemhub::plugin::theme::styleTextInput(searchInput, "Search projects...");
    searchInput.onTextChange = [this]
    {
        rebuildProjectCards();
        resized();
    };

    addAndMakeVisible(filterAllButton);
    filterAllButton.onClick = [this]
    {
        activeProjectFilter = ProjectFilter::All;
        updateProjectFilterButtons();
        rebuildProjectCards();
    };

    addAndMakeVisible(filterLocalButton);
    filterLocalButton.onClick = [this]
    {
        activeProjectFilter = ProjectFilter::Local;
        updateProjectFilterButtons();
        rebuildProjectCards();
    };

    addAndMakeVisible(filterCloudButton);
    filterCloudButton.onClick = [this]
    {
        activeProjectFilter = ProjectFilter::Cloud;
        updateProjectFilterButtons();
        rebuildProjectCards();
    };
    updateProjectFilterButtons();

    addAndMakeVisible(emptyStateTitle);
    emptyStateTitle.setText("No projects found", juce::dontSendNotification);
    emptyStateTitle.setFont(stemhub::plugin::theme::headingFont(18.0f));
    emptyStateTitle.setColour(juce::Label::textColourId, stemhub::plugin::theme::PluginTheme::kForeground);
    emptyStateTitle.setJustificationType(juce::Justification::centred);
    emptyStateTitle.setVisible(false);

    addAndMakeVisible(emptyStateSubtle);
    emptyStateSubtle.setText("Search returned nothing, or your account has no linked projects yet.", juce::dontSendNotification);
    emptyStateSubtle.setFont(stemhub::plugin::theme::bodyFont(12.0f));
    emptyStateSubtle.setColour(juce::Label::textColourId, stemhub::plugin::theme::PluginTheme::kForegroundSubtle);
    emptyStateSubtle.setJustificationType(juce::Justification::centred);
    emptyStateSubtle.setVisible(false);

    addAndMakeVisible(projectComboBox);
    stemhub::plugin::theme::styleComboBox(projectComboBox);
    projectComboBox.setVisible(false);

    addAndMakeVisible(projectListViewport);
    projectListViewport.setViewedComponent(&projectListContent, false);
    projectListViewport.setScrollBarsShown(true, false);

    addAndMakeVisible(chooseProjectFileButton);
    chooseProjectFileButton.setButtonText("Choose File");
    stemhub::plugin::theme::styleGhostButton(chooseProjectFileButton);
    chooseProjectFileButton.onClick = [this]
    {
        invokeIfBound(onChooseProjectFile);
    };

    addAndMakeVisible(openProjectButton);
    openProjectButton.setVisible(false);
    openProjectButton.onClick = [this]
    {
        invokeIfBound(onOpenProject);
    };

    addAndMakeVisible(createProjectButton);
    createProjectButton.setButtonText("New Project");
    stemhub::plugin::theme::stylePrimaryButton(createProjectButton);
    createProjectButton.onClick = [this]
    {
        invokeIfBound(onCreateProject);
    };

    addAndMakeVisible(signOutButton);
    signOutButton.setButtonText("Sign Out");
    stemhub::plugin::theme::styleGhostButton(signOutButton);
    signOutButton.onClick = [this]
    {
        invokeIfBound(onSignOut);
    };
}

void ProjectSelectionView::setMessage(const juce::String& message, stemhub::plugin::theme::MessageStatus status)
{
    stemhub::plugin::theme::styleStatusLabel(statusLabel, message, status);
    statusLabel.setTooltip(message);
    statusLabel.setVisible(message.isNotEmpty());
    resized();
}

void ProjectSelectionView::setProjects(const std::vector<juce::String>& projectNames,
                                       const std::vector<juce::String>& projectIds,
                                       const juce::String& selectedProjectId)
{
    allProjects.clear();
    allProjects.reserve(std::min(projectNames.size(), projectIds.size()));
    for (size_t i = 0; i < projectNames.size() && i < projectIds.size(); ++i)
        allProjects.emplace_back(projectNames[i], projectIds[i]);

    projectComboBox.clear(juce::dontSendNotification);
    comboProjectIds = projectIds;

    for (size_t i = 0; i < projectNames.size() && i < projectIds.size(); ++i)
        projectComboBox.addItem(projectNames[i], static_cast<int>(i) + 1);

    if (selectedProjectId.isNotEmpty())
        selectProjectById(selectedProjectId, false);
    else if (!comboProjectIds.empty())
        projectComboBox.setSelectedId(1, juce::dontSendNotification);

    rebuildProjectCards();
}

void ProjectSelectionView::setSelectedProjectFileMessage(const juce::String& message)
{
    projectFileLabel.setText(message, juce::dontSendNotification);
    projectFileLabel.setTooltip(message);
    rebuildProjectCards();
}

void ProjectSelectionView::setProjectFileSelectionState(bool fileSelected, const juce::String& selectedProjectFilePath)
{
    hasProjectFile = fileSelected;

    const auto fileInfo = hasProjectFile
                              ? shortenPath(selectedProjectFilePath.isNotEmpty() ? selectedProjectFilePath
                                                                                  : "DAW project file selected.")
                              : juce::String("Choose a DAW project file to create new projects.");
    projectFileLabel.setText(fileInfo, juce::dontSendNotification);
    projectFileLabel.setTooltip(selectedProjectFilePath);

    createProjectButton.setVisible(true);
    applyButtonAvailability(createProjectButton, canCreateProject);
    rebuildProjectCards();
    resized();
}

void ProjectSelectionView::setHasExistingProjects(bool hasProjects)
{
    hasExistingProjects = hasProjects;
    rebuildProjectCards();
    resized();
}

void ProjectSelectionView::setCanCreateProject(bool canCreate)
{
    canCreateProject = canCreate && hasProjectFile;
    createProjectButton.setVisible(true);
    applyButtonAvailability(createProjectButton, canCreateProject);
    repaint();
}

juce::String ProjectSelectionView::getSelectedProjectId() const
{
    return getMappedComboSelection(projectComboBox, comboProjectIds);
}

void ProjectSelectionView::paint(juce::Graphics& g)
{
    auto panel = getLocalBounds().toFloat().reduced(14.0f);
    stemhub::plugin::theme::paintCard(g, panel, true);

    auto headerDividerY = panel.getY() + 112.0f;
    g.setColour(stemhub::plugin::theme::PluginTheme::kBorderSubtle);
    g.drawLine(panel.getX(), headerDividerY, panel.getRight(), headerDividerY, 1.0f);
}

void ProjectSelectionView::resized()
{
    auto area = getLocalBounds().reduced(24, 20);

    auto headerRow = area.removeFromTop(62);
    auto actions = headerRow.removeFromRight(240);
    auto signOutArea = actions.removeFromRight(86);
    signOutButton.setBounds(signOutArea.removeFromTop(38));
    actions.removeFromRight(10);
    createProjectButton.setBounds(actions.removeFromTop(38));

    auto titleArea = headerRow;
    titleLabel.setBounds(titleArea.removeFromTop(28));
    subtitleLabel.setBounds(titleArea.removeFromTop(20));

    auto toolbarRow = area.removeFromTop(42);
    auto filtersArea = toolbarRow.removeFromRight(288);
    searchInput.setBounds(toolbarRow.reduced(0, 1));

    auto filterBounds = filtersArea.reduced(0, 1);
    const auto buttonWidth = (filterBounds.getWidth() - 16) / 3;
    filterAllButton.setBounds(filterBounds.removeFromLeft(buttonWidth));
    filterBounds.removeFromLeft(8);
    filterLocalButton.setBounds(filterBounds.removeFromLeft(buttonWidth));
    filterBounds.removeFromLeft(8);
    filterCloudButton.setBounds(filterBounds);

    area.removeFromTop(10);

    auto fileRow = area.removeFromTop(32);
    auto chooseArea = fileRow.removeFromRight(104);
    chooseProjectFileButton.setBounds(chooseArea);
    fileRow.removeFromRight(12);
    projectFileLabel.setBounds(fileRow);

    if (statusLabel.isVisible())
    {
        area.removeFromTop(8);
        auto statusRow = area.removeFromTop(30);
        statusLabel.setBounds(statusRow);
    }
    else
    {
        statusLabel.setBounds(0, 0, 0, 0);
        area.removeFromTop(8);
    }

    auto listArea = area;
    projectListViewport.setBounds(listArea);

    if (projectCards.isEmpty())
    {
        projectListContent.setBounds(0, 0, listArea.getWidth(), listArea.getHeight());
        auto emptyBounds = listArea.reduced(32, 60);
        emptyStateTitle.setBounds(emptyBounds.removeFromTop(28));
        emptyBounds.removeFromTop(6);
        emptyStateSubtle.setBounds(emptyBounds.removeFromTop(32));
    }
    else
    {
        const int cardHeight = 84;
        const int spacing = 14;
        int y = 0;
        for (auto* card : projectCards)
        {
            card->setBounds(0, y, listArea.getWidth() - 12, cardHeight);
            y += cardHeight + spacing;
        }

        projectListContent.setBounds(0, 0, listArea.getWidth() - 12, juce::jmax(listArea.getHeight(), y));
        emptyStateTitle.setBounds(0, 0, 0, 0);
        emptyStateSubtle.setBounds(0, 0, 0, 0);
    }
}

void ProjectSelectionView::rebuildProjectCards()
{
    projectCards.clear(true);

    const auto query = searchInput.getText().trim().toLowerCase();
    const auto currentSelection = getSelectedProjectId();

    for (const auto& [name, id] : allProjects)
    {
        if (query.isNotEmpty() && !name.toLowerCase().contains(query) && !id.toLowerCase().contains(query))
            continue;

        auto* card = static_cast<ProjectCardComponent*>(projectCards.add(new ProjectCardComponent()));
        card->setData(ProjectCardData {
            name,
            projectFileLabel.getText().isNotEmpty() ? projectFileLabel.getText() : juce::String("Choose a DAW project file"),
            hasProjectFile ? juce::String("SYNCED") : juce::String("LINK FILE"),
            id == currentSelection,
            hasProjectFile
        });
        card->onSelect = [this, id]
        {
            selectProjectById(id, true);
        };
        card->onOpen = [this, id]
        {
            selectProjectById(id, false);
            invokeIfBound(onOpenProject);
        };
        projectListContent.addAndMakeVisible(card);
    }

    const bool hasVisibleProjects = !projectCards.isEmpty();
    emptyStateTitle.setVisible(!hasVisibleProjects);
    emptyStateSubtle.setVisible(!hasVisibleProjects);
    projectListViewport.setVisible(hasVisibleProjects);
    resized();
    repaint();
}

void ProjectSelectionView::updateProjectFilterButtons()
{
    styleFilterButton(filterAllButton, activeProjectFilter == ProjectFilter::All);
    styleFilterButton(filterLocalButton, activeProjectFilter == ProjectFilter::Local);
    styleFilterButton(filterCloudButton, activeProjectFilter == ProjectFilter::Cloud);
}

void ProjectSelectionView::selectProjectById(const juce::String& projectId, bool triggerOpen)
{
    for (size_t i = 0; i < comboProjectIds.size(); ++i)
    {
        if (comboProjectIds[i] == projectId)
        {
            projectComboBox.setSelectedId(static_cast<int>(i) + 1, juce::dontSendNotification);
            break;
        }
    }

    rebuildProjectCards();

    if (triggerOpen)
        invokeIfBound(onOpenProject);
}

DashboardView::DashboardView()
{
    addAndMakeVisible(headerLogoLabel);
    headerLogoLabel.setText("S", juce::dontSendNotification);
    headerLogoLabel.setFont(stemhub::plugin::theme::headingFont(18.0f));
    headerLogoLabel.setJustificationType(juce::Justification::centred);
    headerLogoLabel.setColour(juce::Label::backgroundColourId, stemhub::plugin::theme::PluginTheme::kAccent);
    headerLogoLabel.setColour(juce::Label::textColourId, stemhub::plugin::theme::PluginTheme::kBackground);

    addAndMakeVisible(headerTitle);
    headerTitle.setText("Stemhub", juce::dontSendNotification);
    headerTitle.setFont(stemhub::plugin::theme::headingFont(17.0f));
    headerTitle.setColour(juce::Label::textColourId, stemhub::plugin::theme::PluginTheme::kForeground);

    addAndMakeVisible(headerProjectLabel);
    headerProjectLabel.setText("No project selected", juce::dontSendNotification);
    headerProjectLabel.setFont(stemhub::plugin::theme::headingFont(15.0f));
    headerProjectLabel.setColour(juce::Label::textColourId, stemhub::plugin::theme::PluginTheme::kForegroundSubtle);

    addAndMakeVisible(projectStatusLabel);
    stemhub::plugin::theme::styleStatusLabel(projectStatusLabel, "Ready", stemhub::plugin::theme::MessageStatus::neutral);

    addAndMakeVisible(snapshotSectionLabel);
    styleSectionLabel(snapshotSectionLabel, "CURRENT SNAPSHOT");

    addAndMakeVisible(snapshotTitleLabel);
    snapshotTitleLabel.setText("No snapshots yet", juce::dontSendNotification);
    snapshotTitleLabel.setFont(stemhub::plugin::theme::headingFont(22.0f));
    snapshotTitleLabel.setColour(juce::Label::textColourId, stemhub::plugin::theme::PluginTheme::kForeground);

    addAndMakeVisible(snapshotMetaLabel);
    styleBodyLabel(snapshotMetaLabel, "Save a new snapshot to start history.", true, true);

    addAndMakeVisible(actionHintLabel);
    styleBodyLabel(actionHintLabel, "Create a snapshot to preserve your work.", true, false);

    addAndMakeVisible(historyLabel);
    styleSectionLabel(historyLabel, "SESSION HISTORY");

    addAndMakeVisible(footerPathLabel);
    styleBodyLabel(footerPathLabel, "~/", true, true);

    addAndMakeVisible(footerCloudLabel);
    styleBodyLabel(footerCloudLabel, "stemhub.io/project", true, true);

    addAndMakeVisible(footerStorageLabel);
    footerStorageLabel.setFont(stemhub::plugin::theme::monoFont(11.5f));
    footerStorageLabel.setColour(juce::Label::textColourId, stemhub::plugin::theme::PluginTheme::kForegroundSubtle);
    footerStorageLabel.setJustificationType(juce::Justification::centredRight);

    addAndMakeVisible(branchComboBox);
    branchComboBox.setTextWhenNothingSelected("Workspace");
    stemhub::plugin::theme::styleComboBox(branchComboBox);
    branchComboBox.onChange = [this]
    {
        invokeIfBound(onBranchChange);
    };

    addAndMakeVisible(versionComboBox);
    stemhub::plugin::theme::styleComboBox(versionComboBox);
    versionComboBox.setVisible(false);
    versionComboBox.onChange = [this]
    {
        updateSnapshotSummary();
        invokeIfBound(onVersionSelectionChange);
    };

    addAndMakeVisible(backToProjectsButton);
    backToProjectsButton.setButtonText("Back");
    stemhub::plugin::theme::styleGhostButton(backToProjectsButton);
    backToProjectsButton.onClick = [this]
    {
        invokeIfBound(onBackToProjects);
    };

    addChildComponent(commitMessageInput);
    stemhub::plugin::theme::styleTextInput(commitMessageInput, "Describe this save");
    commitMessageInput.setVisible(false);

    addAndMakeVisible(saveChanges);
    saveChanges.setButtonText("Save Snapshot");
    stemhub::plugin::theme::stylePrimaryButton(saveChanges);
    saveChanges.onClick = [this]
    {
        invokeIfBound(onSave);
    };

    addAndMakeVisible(syncButton);
    syncButton.setButtonText("Sync");
    stemhub::plugin::theme::styleSecondaryButton(syncButton);
    syncButton.onClick = [this]
    {
        invokeIfBound(onSync);
    };

    addAndMakeVisible(signOutButton);
    signOutButton.setButtonText("Sign Out");
    stemhub::plugin::theme::styleGhostButton(signOutButton);
    signOutButton.onClick = [this]
    {
        invokeIfBound(onSignOut);
    };

    addAndMakeVisible(restoreButton);
    restoreButton.setButtonText("Restore");
    stemhub::plugin::theme::styleSecondaryButton(restoreButton);
    restoreButton.onClick = [this]
    {
        invokeIfBound(onRestore);
    };

    addAndMakeVisible(versionListViewport);
    versionListViewport.setViewedComponent(&versionListContent, false);
    versionListViewport.setScrollBarsShown(true, false);

    updateFooterSummary();
}

void DashboardView::setProjectStatusMessage(const juce::String& message, stemhub::plugin::theme::MessageStatus status)
{
    stemhub::plugin::theme::styleStatusLabel(projectStatusLabel, makeStatusChipText(status, message), status);
    projectStatusLabel.setTooltip(message);
    actionHintLabel.setText(message.isNotEmpty() ? message : "Create a snapshot to preserve your work.", juce::dontSendNotification);
}

void DashboardView::setSelectedProjectFileMessage(const juce::String& message)
{
    footerPathLabel.setTooltip(message);

    if (getSelectedVersionId().isEmpty())
    {
        snapshotMetaLabel.setText(message, juce::dontSendNotification);
        snapshotMetaLabel.setTooltip(message);
    }
}

void DashboardView::setProjectNameMessage(const juce::String& message)
{
    headerProjectLabel.setText(message, juce::dontSendNotification);
    updateFooterSummary();
}

void DashboardView::setBranchNameMessage(const juce::String& message)
{
    branchComboBox.setTooltip(message);
    updateSnapshotSummary();
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
    versionDisplayLabels = versionLabels;
    setMappedComboItems(versionComboBox, comboVersionIds, versionLabels, versionIds, selectedVersionId);
    rebuildVersionCards();
    updateSnapshotSummary();
}

void DashboardView::setPackagedFiles(const juce::String& rootLabel, const std::vector<juce::String>& relativeFilePaths)
{
    juce::ignoreUnused(rootLabel);
    packagedFileCount = static_cast<int>(relativeFilePaths.size());
    updateFooterSummary();
}

juce::String DashboardView::getSelectedBranchId() const
{
    return getMappedComboSelection(branchComboBox, comboBranchIds);
}

juce::String DashboardView::getSelectedVersionId() const
{
    return getMappedComboSelection(versionComboBox, comboVersionIds);
}

void DashboardView::paint(juce::Graphics& g)
{
    auto panel = getLocalBounds().toFloat().reduced(14.0f);
    stemhub::plugin::theme::paintCard(g, panel, true);

    const auto left = panel.getX();
    const auto right = panel.getRight();
    const auto top = panel.getY();

    g.setColour(stemhub::plugin::theme::PluginTheme::kBorderSubtle);
    g.drawLine(left, top + 74.0f, right, top + 74.0f, 1.0f);
    g.drawLine(left, top + 170.0f, right, top + 170.0f, 1.0f);
    g.drawLine(left, top + 258.0f, right, top + 258.0f, 1.0f);
    g.drawLine(left, panel.getBottom() - 40.0f, right, panel.getBottom() - 40.0f, 1.0f);

    g.setColour(stemhub::plugin::theme::PluginTheme::kSurfaceElevated.withAlpha(0.55f));
    g.fillRect(panel.getX() + 2.0f, top + 259.0f, panel.getWidth() - 4.0f, panel.getHeight() - 299.0f);
}

void DashboardView::resized()
{
    auto area = getLocalBounds().reduced(14);

    auto header = area.removeFromTop(60).reduced(16, 8);
    backToProjectsButton.setBounds(header.removeFromLeft(56));
    header.removeFromLeft(10);
    headerLogoLabel.setBounds(header.removeFromLeft(34));
    header.removeFromLeft(8);
    headerTitle.setBounds(header.removeFromLeft(98));
    header.removeFromLeft(8);
    headerProjectLabel.setBounds(header.removeFromLeft(juce::jmax(122, header.getWidth() - 220)));

    auto headerRight = header;
    signOutButton.setBounds(headerRight.removeFromRight(74));
    headerRight.removeFromRight(8);
    branchComboBox.setBounds(headerRight.removeFromRight(124));
    headerRight.removeFromRight(8);
    projectStatusLabel.setBounds(headerRight.removeFromRight(82));

    auto snapshot = area.removeFromTop(96).reduced(16, 14);
    snapshotSectionLabel.setBounds(snapshot.removeFromTop(16));
    snapshot.removeFromTop(8);
    snapshotTitleLabel.setBounds(snapshot.removeFromTop(30));
    snapshot.removeFromTop(6);
    snapshotMetaLabel.setBounds(snapshot.removeFromTop(18));

    auto actions = area.removeFromTop(88).reduced(16, 14);
    auto actionButtons = actions.removeFromTop(42);
    auto utilityWidth = 96;
    restoreButton.setBounds(actionButtons.removeFromRight(96));
    actionButtons.removeFromRight(10);
    syncButton.setBounds(actionButtons.removeFromRight(utilityWidth));
    actionButtons.removeFromRight(10);
    saveChanges.setBounds(actionButtons);
    actions.removeFromTop(10);
    actionHintLabel.setBounds(actions.removeFromTop(18));

    auto historyArea = area;
    historyArea.removeFromBottom(48);
    auto historyHeader = historyArea.removeFromTop(42).reduced(16, 10);
    historyLabel.setBounds(historyHeader);

    auto listBounds = historyArea.reduced(0, 0);
    versionListViewport.setBounds(listBounds);

    const int rowHeight = 76;
    int y = 0;
    for (auto* card : versionCards)
    {
        card->setBounds(0, y, listBounds.getWidth() - 12, rowHeight);
        y += rowHeight;
    }
    versionListContent.setBounds(0, 0, listBounds.getWidth() - 12, juce::jmax(listBounds.getHeight(), y));

    auto footer = area.removeFromBottom(40).reduced(16, 8);
    auto storageArea = footer.removeFromRight(124);
    footerStorageLabel.setBounds(storageArea);
    footer.removeFromRight(12);
    footerCloudLabel.setBounds(footer.removeFromRight(210));
    footer.removeFromRight(10);
    footerPathLabel.setBounds(footer);
}

void DashboardView::rebuildVersionCards()
{
    versionCards.clear(true);

    if (comboVersionIds.empty() || versionDisplayLabels.empty())
    {
        auto* card = static_cast<VersionCardComponent*>(versionCards.add(new VersionCardComponent()));
        card->setData(VersionCardData { "No snapshots yet", "Save a new snapshot to start this history.", false });
        versionListContent.addAndMakeVisible(card);
        resized();
        repaint();
        return;
    }

    const auto selectedVersionId = getSelectedVersionId();

    for (size_t i = 0; i < comboVersionIds.size() && i < versionDisplayLabels.size(); ++i)
    {
        auto* card = static_cast<VersionCardComponent*>(versionCards.add(new VersionCardComponent()));
        const auto& versionId = comboVersionIds[i];
        const auto& label = versionDisplayLabels[i];
        auto meta = extractVersionTimestamp(label);
        if (meta.isEmpty())
            meta = "Version " + extractVersionShortId(label);
        else
            meta = meta + " / " + extractVersionShortId(label);

        card->setData(VersionCardData {
            isGenericSnapshotTitle(extractVersionTitle(label)) ? juce::String("Snapshot")
                                                               : extractVersionTitle(label),
            meta,
            versionId == selectedVersionId
        });
        card->onSelect = [this, versionId]
        {
            selectVersionById(versionId, true);
        };
        versionListContent.addAndMakeVisible(card);
    }

    resized();
    repaint();
}

void DashboardView::updateSnapshotSummary()
{
    const auto selectedVersionId = getSelectedVersionId();

    if (selectedVersionId.isEmpty())
    {
        snapshotTitleLabel.setText("No snapshots yet", juce::dontSendNotification);
        juce::String metadata = branchComboBox.getText().isNotEmpty() ? branchComboBox.getText() : juce::String();
        if (selectedProjectFilePath.isNotEmpty())
        {
            if (metadata.isNotEmpty())
                metadata += " / ";
            metadata += shortenPath(selectedProjectFilePath);
        }

        if (metadata.isNotEmpty())
            snapshotMetaLabel.setText(metadata, juce::dontSendNotification);
        return;
    }

    for (size_t i = 0; i < comboVersionIds.size() && i < versionDisplayLabels.size(); ++i)
    {
        if (comboVersionIds[i] != selectedVersionId)
            continue;

        const auto extractedTitle = extractVersionTitle(versionDisplayLabels[i]);
        snapshotTitleLabel.setText(isGenericSnapshotTitle(extractedTitle) ? "Latest snapshot"
                                                                          : extractedTitle,
                                   juce::dontSendNotification);

        auto metadata = extractVersionTimestamp(versionDisplayLabels[i]);
        if (metadata.isEmpty())
            metadata = "Version " + extractVersionShortId(versionDisplayLabels[i]);

        if (branchComboBox.getText().isNotEmpty())
            metadata += " / " + branchComboBox.getText();

        if (selectedProjectFilePath.isNotEmpty())
            metadata += " / " + shortenPath(selectedProjectFilePath, 34);

        snapshotMetaLabel.setText(metadata, juce::dontSendNotification);
        return;
    }
}

void DashboardView::updateFooterSummary()
{
    const auto displayPath = selectedProjectFilePath.isNotEmpty()
                                 ? shortenPath(selectedProjectFilePath, 34)
                                 : juce::String("~/No local project selected");
    footerPathLabel.setText(displayPath, juce::dontSendNotification);
    footerPathLabel.setTooltip(selectedProjectFilePath);

    const auto slug = makeSlug(headerProjectLabel.getText());
    footerCloudLabel.setText("stemhub.io/" + (slug.isNotEmpty() ? slug : juce::String("project")), juce::dontSendNotification);

    juce::String storage;
    if (selectedProjectFilePath.isNotEmpty())
    {
        const auto fileSize = juce::File(selectedProjectFilePath).getSize();
        storage = juce::File::descriptionOfSizeInBytes(fileSize > 0 ? fileSize : 0) + " / 5 GB";
    }
    else
    {
        storage = packagedFileCount > 0 ? juce::String(packagedFileCount) + " files ready"
                                        : juce::String("No local file");
    }

    footerStorageLabel.setText(storage, juce::dontSendNotification);
}

void DashboardView::selectVersionById(const juce::String& versionId, bool triggerChange)
{
    for (size_t i = 0; i < comboVersionIds.size(); ++i)
    {
        if (comboVersionIds[i] != versionId)
            continue;

        versionComboBox.setSelectedId(static_cast<int>(i) + 1,
                                      triggerChange ? juce::sendNotificationSync : juce::dontSendNotification);
        break;
    }

    rebuildVersionCards();
    updateSnapshotSummary();
}
