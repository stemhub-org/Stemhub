#include "ui/Views.hpp"
#include <algorithm>
#include <utility>

namespace
{
namespace theme = stemhub::plugin::theme;
using Theme = theme::PluginTheme;

void invokeIfBound(const std::function<void()>& callback)
{
    if (callback != nullptr)
        callback();
}

void applyButtonAvailability(juce::Component& button, bool isAvailable)
{
    button.setEnabled(isAvailable);
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

juce::String makeStatusChipText(theme::MessageStatus status, const juce::String& message)
{
    juce::ignoreUnused(message);

    switch (status)
    {
        case theme::MessageStatus::loading:
            return "Syncing";
        case theme::MessageStatus::success:
            return "Synced";
        case theme::MessageStatus::warning:
            return "Attention";
        case theme::MessageStatus::error:
            return "Error";
        case theme::MessageStatus::disabled:
            return "Disabled";
        case theme::MessageStatus::neutral:
        default:
            return "Ready";
    }
}

juce::String metaSeparator()
{
    return "  " + theme::middleDot() + "  ";
}

juce::String formatIndex(int index)
{
    return juce::String(index + 1).paddedLeft('0', 2);
}

void styleSectionLabel(juce::Label& label, const juce::String& text, juce::Colour colour = Theme::kForegroundSubtle)
{
    theme::styleMetaLabel(label, text, colour);
}

void styleBodyLabel(juce::Label& label, const juce::String& text, bool muted = false, float size = 12.5f)
{
    label.setText(text, juce::dontSendNotification);
    label.setFont(theme::bodyFont(size));
    label.setColour(juce::Label::textColourId, muted ? Theme::kForegroundSubtle : Theme::kForeground);
    label.setJustificationType(juce::Justification::centredLeft);
    label.setMinimumHorizontalScale(1.0f);
    label.setBorderSize({});
}

void styleFilterButton(juce::TextButton& button, bool selected)
{
    theme::styleSegmentButton(button, selected);
}

// Copies the callback before running it: the handler may rebuild (and delete) the card that owns it.
void invokeDetached(const std::function<void()>& callback)
{
    if (auto detached = callback)
        detached();
}

struct ProjectCardData
{
    juce::String name;
    juce::String path;
    juce::String badge;
    int index { 0 };
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

    ProjectCardComponent()
    {
        setMouseCursor(juce::MouseCursor::PointingHandCursor);
    }

    void setData(ProjectCardData nextData)
    {
        data = std::move(nextData);
        setTitle(data.name);
        repaint();
    }

    void paint(juce::Graphics& g) override
    {
        const auto hovered = isMouseOver(true);
        const auto bounds = getLocalBounds();

        g.setColour(hovered ? Theme::kSurfaceElevated : Theme::kSurface);
        g.fillRect(bounds);
        g.setColour(data.selected ? Theme::kAccent
                                  : (hovered ? Theme::kForeground.withAlpha(0.32f) : Theme::kSurfaceBorder));
        g.drawRect(bounds, 1);

        if (data.selected)
        {
            g.setColour(Theme::kAccent);
            g.fillRect(bounds.withWidth(3));
        }

        auto content = bounds.reduced(18, 0);

        g.setColour(data.selected || hovered ? Theme::kAccent : Theme::kForegroundTertiary);
        g.setFont(theme::labelFont(11.0f));
        g.drawText(formatIndex(data.index), content.removeFromLeft(36), juce::Justification::centredLeft, false);

        g.setColour(data.selected || hovered ? Theme::kAccent : Theme::kForegroundSubtle);
        g.setFont(theme::headingFont(17.0f));
        g.drawText(theme::arrowRight(), content.removeFromRight(20), juce::Justification::centredRight, false);
        content.removeFromRight(16);

        const auto badgeArea = content.removeFromRight(86).withSizeKeepingCentre(86, 22).toFloat();
        if (data.enabled)
            theme::paintTag(g, data.badge, badgeArea, Theme::kAccent, Theme::kAccent, true);
        else
            theme::paintTag(g, data.badge, badgeArea, Theme::kForegroundTertiary, Theme::kForegroundSubtle, true);
        content.removeFromRight(20);

        if (content.getWidth() > 360)
        {
            const auto patternArea = content.removeFromRight(112).withSizeKeepingCentre(112, 22).toFloat();
            theme::paintBlockPattern(g, patternArea, data.name, 12,
                                     Theme::kForeground.withAlpha(hovered ? 0.34f : 0.16f),
                                     Theme::kAccent.withAlpha(hovered ? 1.0f : 0.5f));
            content.removeFromRight(20);
        }

        const auto normalizedName = normalizeProjectCardLine(data.name);
        const auto normalizedPath = normalizeProjectCardLine(data.path);
        const auto showPath = data.path.isNotEmpty() && normalizedPath.isNotEmpty() && normalizedPath != normalizedName;

        auto textArea = content.withSizeKeepingCentre(content.getWidth(), showPath ? 38 : 22);
        g.setColour(Theme::kForeground);
        g.setFont(theme::headingFont(15.5f));
        g.drawText(data.name, textArea.removeFromTop(22), juce::Justification::centredLeft, true);

        if (showPath)
        {
            g.setColour(Theme::kForegroundSubtle);
            g.setFont(theme::bodyFont(11.5f));
            g.drawText(shortenPath(data.path), textArea, juce::Justification::centredLeft, true);
        }
    }

    void mouseEnter(const juce::MouseEvent&) override { repaint(); }
    void mouseExit(const juce::MouseEvent&) override { repaint(); }

    void mouseUp(const juce::MouseEvent& event) override
    {
        if (event.mouseWasClicked())
            invokeDetached(onSelect);
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
    juce::String shortId;
    int index { 0 };
    bool selected { false };
    bool placeholder { false };
};

class VersionCardComponent final : public juce::Component
{
public:
    std::function<void()> onSelect;

    void setData(VersionCardData nextData)
    {
        data = std::move(nextData);
        setTitle(data.title);
        setMouseCursor(data.placeholder ? juce::MouseCursor::NormalCursor : juce::MouseCursor::PointingHandCursor);
        repaint();
    }

    void paint(juce::Graphics& g) override
    {
        const auto bounds = getLocalBounds();
        const auto hovered = !data.placeholder && isMouseOver(true);
        const auto ink = data.selected;

        if (data.selected)
        {
            g.setColour(Theme::kAccent);
            g.fillRect(bounds);
        }
        else
        {
            if (hovered)
            {
                g.setColour(Theme::kSurface);
                g.fillRect(bounds);
            }

            g.setColour(Theme::kBorderSubtle);
            g.fillRect(bounds.withTop(bounds.getBottom() - 1));
        }

        auto content = bounds.reduced(16, 0);

        if (data.placeholder)
        {
            theme::paintBlockPattern(g, content.removeFromLeft(96).withSizeKeepingCentre(96, 18).toFloat(),
                                     "empty-history", 10, Theme::kForeground.withAlpha(0.12f),
                                     Theme::kForeground.withAlpha(0.24f));
            content.removeFromLeft(18);
        }
        else
        {
            g.setColour(ink ? Theme::kInk : (hovered ? Theme::kAccent : Theme::kForegroundTertiary));
            g.setFont(theme::labelFont(11.0f));
            g.drawText(formatIndex(data.index), content.removeFromLeft(36), juce::Justification::centredLeft, false);

            if (data.shortId.isNotEmpty())
            {
                g.setColour(ink ? Theme::kInk.withAlpha(0.7f) : Theme::kForegroundTertiary);
                g.setFont(theme::monoFont(11.0f));
                g.drawText(data.shortId, content.removeFromRight(72), juce::Justification::centredRight, true);
                content.removeFromRight(16);
            }

            if (content.getWidth() > 380)
            {
                theme::paintBlockPattern(g, content.removeFromRight(92).withSizeKeepingCentre(92, 18).toFloat(),
                                         data.shortId + data.title, 10,
                                         ink ? Theme::kInk.withAlpha(0.28f) : Theme::kForeground.withAlpha(hovered ? 0.3f : 0.14f),
                                         ink ? Theme::kInk : Theme::kAccent.withAlpha(hovered ? 1.0f : 0.55f));
                content.removeFromRight(20);
            }
        }

        auto textArea = content.withSizeKeepingCentre(content.getWidth(), 36);
        g.setColour(ink ? Theme::kInk : Theme::kForeground);
        g.setFont(theme::headingFont(14.0f));
        g.drawText(data.title, textArea.removeFromTop(20), juce::Justification::centredLeft, true);

        g.setColour(ink ? Theme::kInk.withAlpha(0.72f) : Theme::kForegroundSubtle);
        g.setFont(theme::bodyFont(11.5f));
        g.drawText(data.meta, textArea, juce::Justification::centredLeft, true);
    }

    void mouseEnter(const juce::MouseEvent&) override { repaint(); }
    void mouseExit(const juce::MouseEvent&) override { repaint(); }

    void mouseUp(const juce::MouseEvent& event) override
    {
        if (event.mouseWasClicked())
            invokeDetached(onSelect);
    }

private:
    VersionCardData data;
};
}

ProjectSelectionView::ProjectSelectionView()
{
    addAndMakeVisible(titleLabel);
    titleLabel.setText("YOUR PROJECTS.", juce::dontSendNotification);
    titleLabel.setFont(theme::displayFont(34.0f));
    titleLabel.setColour(juce::Label::textColourId, Theme::kForeground);
    titleLabel.setJustificationType(juce::Justification::bottomLeft);
    titleLabel.setMinimumHorizontalScale(1.0f);
    titleLabel.setBorderSize({});

    addAndMakeVisible(subtitleLabel);
    styleBodyLabel(subtitleLabel, "Pick up a session where you left it, or start one from a DAW file.", true, 13.0f);

    addAndMakeVisible(statusLabel);
    theme::styleStatusLabel(statusLabel, {}, theme::MessageStatus::neutral);
    statusLabel.setVisible(false);

    addAndMakeVisible(projectFileLabel);
    styleBodyLabel(projectFileLabel, "Choose a DAW project file to create new projects.", false, 12.5f);
    projectFileLabel.setFont(theme::mediumFont(12.5f));

    addAndMakeVisible(searchInput);
    theme::styleTextInput(searchInput, "Search projects");
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
    filterLocalButton.setConnectedEdges(juce::Button::ConnectedOnLeft | juce::Button::ConnectedOnRight);
    filterCloudButton.setConnectedEdges(juce::Button::ConnectedOnLeft);
    filterAllButton.setConnectedEdges(juce::Button::ConnectedOnRight);
    updateProjectFilterButtons();

    addAndMakeVisible(emptyStateTitle);
    emptyStateTitle.setText("NOTHING HERE YET.", juce::dontSendNotification);
    emptyStateTitle.setFont(theme::displayFont(20.0f));
    emptyStateTitle.setColour(juce::Label::textColourId, Theme::kForeground);
    emptyStateTitle.setJustificationType(juce::Justification::centred);
    emptyStateTitle.setMinimumHorizontalScale(1.0f);
    emptyStateTitle.setVisible(false);

    addAndMakeVisible(emptyStateSubtle);
    emptyStateSubtle.setText("Search returned nothing, or your account has no linked projects yet.", juce::dontSendNotification);
    emptyStateSubtle.setFont(theme::bodyFont(12.5f));
    emptyStateSubtle.setColour(juce::Label::textColourId, Theme::kForegroundSubtle);
    emptyStateSubtle.setJustificationType(juce::Justification::centredTop);
    emptyStateSubtle.setMinimumHorizontalScale(1.0f);
    emptyStateSubtle.setVisible(false);

    addAndMakeVisible(projectComboBox);
    theme::styleComboBox(projectComboBox);
    projectComboBox.setVisible(false);

    addAndMakeVisible(projectListViewport);
    projectListViewport.setViewedComponent(&projectListContent, false);
    projectListViewport.setScrollBarsShown(true, false);
    projectListViewport.setScrollBarThickness(8);

    addAndMakeVisible(chooseProjectFileButton);
    chooseProjectFileButton.setButtonText("Choose file");
    theme::styleSecondaryButton(chooseProjectFileButton);
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
    createProjectButton.setButtonText("New project  +");
    theme::stylePrimaryButton(createProjectButton);
    createProjectButton.setTooltip("Choose a DAW project file first, then create a StemHub project from it.");
    createProjectButton.onClick = [this]
    {
        invokeIfBound(onCreateProject);
    };

    addAndMakeVisible(signOutButton);
    signOutButton.setButtonText("Sign out");
    theme::styleGhostButton(signOutButton);
    signOutButton.onClick = [this]
    {
        invokeIfBound(onSignOut);
    };
}

void ProjectSelectionView::setMessage(const juce::String& message, stemhub::plugin::theme::MessageStatus status)
{
    theme::styleStatusLabel(statusLabel, message, status);
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
    projectFileLabel.setColour(juce::Label::textColourId, hasProjectFile ? Theme::kForeground : Theme::kForegroundSubtle);

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
    g.fillAll(Theme::kBackground);

    theme::paintMetaText(g, "StemHub / Projects", metaRowBounds, Theme::kForeground);
    g.setColour(Theme::kSurfaceBorder);
    g.fillRect(metaRowBounds.getX(), metaRowBounds.getBottom() + 10, getWidth() - 2 * metaRowBounds.getX(), 1);

    if (!fileStripBounds.isEmpty())
    {
        g.setColour(Theme::kSurface);
        g.fillRect(fileStripBounds);
        g.setColour(Theme::kSurfaceBorder);
        g.drawRect(fileStripBounds, 1);

        auto labelArea = fileStripBounds.reduced(14, 0).removeFromLeft(96);
        g.setColour(hasProjectFile ? Theme::kAccent : Theme::kForegroundTertiary);
        g.fillRect(labelArea.removeFromLeft(6).withSizeKeepingCentre(6, 6));
        labelArea.removeFromLeft(8);
        theme::paintMetaText(g, "Project file", labelArea, Theme::kForegroundSubtle);
    }

    if (emptyStateTitle.isVisible() && !emptyStateBounds.isEmpty())
    {
        g.setColour(Theme::kSurfaceBorder);
        g.drawRect(emptyStateBounds, 1);

        const auto pattern = emptyStateBounds.withSizeKeepingCentre(184, 34)
                                 .withY(emptyStateBounds.getCentreY() - 64)
                                 .toFloat();
        theme::paintBlockPattern(g, pattern, "no-projects", 16,
                                 Theme::kForeground.withAlpha(0.18f), Theme::kAccent);
    }
}

void ProjectSelectionView::resized()
{
    auto area = getLocalBounds().reduced(24, 18);

    auto metaRow = area.removeFromTop(24);
    signOutButton.setBounds(metaRow.removeFromRight(96));
    metaRowBounds = metaRow;
    area.removeFromTop(10 + 1 + 18);

    auto titleRow = area.removeFromTop(40);
    createProjectButton.setBounds(titleRow.removeFromRight(168).withTrimmedTop(2));
    titleRow.removeFromRight(16);
    titleLabel.setBounds(titleRow);

    area.removeFromTop(6);
    subtitleLabel.setBounds(area.removeFromTop(18));
    area.removeFromTop(18);

    auto toolbarRow = area.removeFromTop(38);
    auto filterBounds = toolbarRow.removeFromRight(222);
    toolbarRow.removeFromRight(10);
    searchInput.setBounds(toolbarRow);

    const auto segmentWidth = filterBounds.getWidth() / 3;
    filterAllButton.setBounds(filterBounds.removeFromLeft(segmentWidth));
    filterLocalButton.setBounds(filterBounds.removeFromLeft(segmentWidth));
    filterCloudButton.setBounds(filterBounds);

    area.removeFromTop(10);

    fileStripBounds = area.removeFromTop(46);
    auto fileRow = fileStripBounds.reduced(7, 7);
    chooseProjectFileButton.setBounds(fileRow.removeFromRight(124));
    fileRow.removeFromRight(12);
    fileRow.removeFromLeft(7 + 96 + 10);
    projectFileLabel.setBounds(fileRow);

    area.removeFromTop(10);

    if (statusLabel.isVisible())
    {
        statusLabel.setBounds(area.removeFromTop(32));
        area.removeFromTop(10);
    }
    else
    {
        statusLabel.setBounds(0, 0, 0, 0);
    }

    auto listArea = area;
    projectListViewport.setBounds(listArea);

    if (projectCards.isEmpty())
    {
        projectListContent.setBounds(0, 0, listArea.getWidth(), listArea.getHeight());
        emptyStateBounds = listArea;
        auto emptyBounds = listArea.withSizeKeepingCentre(listArea.getWidth() - 64, 70)
                               .withY(listArea.getCentreY() - 22);
        emptyStateTitle.setBounds(emptyBounds.removeFromTop(28));
        emptyBounds.removeFromTop(6);
        emptyStateSubtle.setBounds(emptyBounds.removeFromTop(36));
    }
    else
    {
        emptyStateBounds = {};
        const int cardHeight = 64;
        const int spacing = 6;
        const auto needsScroll = static_cast<int>(projectCards.size()) * (cardHeight + spacing) - spacing > listArea.getHeight();
        const auto cardWidth = listArea.getWidth() - (needsScroll ? 12 : 0);
        int y = 0;
        for (auto* card : projectCards)
        {
            card->setBounds(0, y, cardWidth, cardHeight);
            y += cardHeight + spacing;
        }

        projectListContent.setBounds(0, 0, cardWidth, juce::jmax(listArea.getHeight(), y - spacing));
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
            hasProjectFile ? juce::String("Synced") : juce::String("Link file"),
            projectCards.size() - 1,
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
    addAndMakeVisible(headerProjectLabel);
    headerProjectLabel.setText("NO PROJECT SELECTED", juce::dontSendNotification);
    headerProjectLabel.setFont(theme::displayFont(16.0f));
    headerProjectLabel.setColour(juce::Label::textColourId, Theme::kForeground);
    headerProjectLabel.setMinimumHorizontalScale(1.0f);
    headerProjectLabel.setBorderSize({});

    addAndMakeVisible(projectStatusLabel);
    theme::makeStatusChip(projectStatusLabel);
    theme::styleStatusLabel(projectStatusLabel, "Ready", theme::MessageStatus::neutral);

    addAndMakeVisible(snapshotSectionLabel);
    styleSectionLabel(snapshotSectionLabel, "Current snapshot", Theme::kAccent);

    addAndMakeVisible(snapshotTitleLabel);
    snapshotTitleLabel.setText("NO SNAPSHOTS YET.", juce::dontSendNotification);
    snapshotTitleLabel.setFont(theme::displayFont(28.0f));
    snapshotTitleLabel.setColour(juce::Label::textColourId, Theme::kForeground);
    snapshotTitleLabel.setMinimumHorizontalScale(1.0f);
    snapshotTitleLabel.setBorderSize({});

    addAndMakeVisible(snapshotMetaLabel);
    styleBodyLabel(snapshotMetaLabel, "Save a new snapshot to start history.", true, 12.0f);

    addAndMakeVisible(actionHintLabel);
    styleBodyLabel(actionHintLabel, "Create a snapshot to preserve your work.", true, 12.0f);

    addAndMakeVisible(historyLabel);
    styleSectionLabel(historyLabel, "Session history", Theme::kForeground);

    addAndMakeVisible(footerPathLabel);
    styleBodyLabel(footerPathLabel, "~/", true, 11.0f);

    addAndMakeVisible(footerCloudLabel);
    styleBodyLabel(footerCloudLabel, "stemhub.io/project", false, 11.0f);
    footerCloudLabel.setFont(theme::semiboldFont(11.0f));
    footerCloudLabel.setJustificationType(juce::Justification::centred);

    addAndMakeVisible(footerStorageLabel);
    footerStorageLabel.setFont(theme::labelFont(10.0f));
    footerStorageLabel.setColour(juce::Label::textColourId, Theme::kForegroundSubtle);
    footerStorageLabel.setJustificationType(juce::Justification::centredRight);
    footerStorageLabel.setMinimumHorizontalScale(1.0f);
    footerStorageLabel.setBorderSize({});

    addAndMakeVisible(branchComboBox);
    branchComboBox.setTextWhenNothingSelected("Workspace");
    theme::styleComboBox(branchComboBox);
    branchComboBox.onChange = [this]
    {
        invokeIfBound(onBranchChange);
    };

    addAndMakeVisible(versionComboBox);
    theme::styleComboBox(versionComboBox);
    versionComboBox.setVisible(false);
    versionComboBox.onChange = [this]
    {
        updateSnapshotSummary();
        invokeIfBound(onVersionSelectionChange);
    };

    addAndMakeVisible(backToProjectsButton);
    backToProjectsButton.setButtonText(theme::arrowLeft() + "  Projects");
    theme::styleGhostButton(backToProjectsButton);
    backToProjectsButton.onClick = [this]
    {
        invokeIfBound(onBackToProjects);
    };

    addChildComponent(commitMessageInput);
    theme::styleTextInput(commitMessageInput, "Describe this save");
    commitMessageInput.setVisible(false);

    addAndMakeVisible(saveChanges);
    saveChanges.setButtonText("Save snapshot");
    theme::stylePrimaryButton(saveChanges);
    saveChanges.setTooltip("Save a new version of this project (Cmd/Ctrl + S).");
    saveChanges.onClick = [this]
    {
        invokeIfBound(onSave);
    };

    addAndMakeVisible(syncButton);
    syncButton.setButtonText("Sync");
    theme::styleSecondaryButton(syncButton);
    syncButton.setTooltip("Fetch the latest version history.");
    syncButton.onClick = [this]
    {
        invokeIfBound(onSync);
    };

    addAndMakeVisible(signOutButton);
    signOutButton.setButtonText("Sign out");
    theme::styleGhostButton(signOutButton);
    signOutButton.onClick = [this]
    {
        invokeIfBound(onSignOut);
    };

    addAndMakeVisible(restoreButton);
    restoreButton.setButtonText("Restore");
    theme::styleSecondaryButton(restoreButton);
    restoreButton.setTooltip("Restore the selected snapshot into your project folder.");
    restoreButton.onClick = [this]
    {
        invokeIfBound(onRestore);
    };

    addAndMakeVisible(versionListViewport);
    versionListViewport.setViewedComponent(&versionListContent, false);
    versionListViewport.setScrollBarsShown(true, false);
    versionListViewport.setScrollBarThickness(8);

    updateFooterSummary();
}

void DashboardView::setProjectStatusMessage(const juce::String& message, stemhub::plugin::theme::MessageStatus status)
{
    theme::styleStatusLabel(projectStatusLabel, makeStatusChipText(status, message), status);
    projectStatusLabel.setTooltip(message);
    actionHintLabel.setText(message.isNotEmpty() ? message : "Create a snapshot to preserve your work.", juce::dontSendNotification);
    actionHintLabel.setColour(juce::Label::textColourId,
                              status == theme::MessageStatus::error ? Theme::kError : Theme::kForegroundSubtle);
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
    headerProjectLabel.setText(message.toUpperCase(), juce::dontSendNotification);
    headerProjectLabel.setTooltip(message);
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
    g.fillAll(Theme::kBackground);

    const auto left = 20;
    const auto width = getWidth() - 2 * left;

    theme::paintLogoTile(g, headerLogoBounds.toFloat(), Theme::kForeground, Theme::kInk);

    g.setColour(Theme::kSurfaceBorder);
    g.fillRect(left, headerDividerY, width, 1);
    g.fillRect(left, historyDividerY, width, 1);
    g.fillRect(left, footerDividerY, width, 1);

    const auto hasSnapshot = getSelectedVersionId().isNotEmpty();
    theme::paintBlockPattern(g, heroPatternBounds.toFloat(),
                             hasSnapshot ? getSelectedVersionId() : juce::String("no-snapshot"),
                             14,
                             Theme::kForeground.withAlpha(hasSnapshot ? 0.9f : 0.16f),
                             hasSnapshot ? Theme::kAccent : Theme::kForeground.withAlpha(0.3f));

    const auto versionCount = static_cast<int>(comboVersionIds.size());
    theme::paintMetaText(g,
                         versionCount == 1 ? juce::String("1 snapshot") : juce::String(versionCount) + " snapshots",
                         historyCountBounds,
                         Theme::kForegroundSubtle,
                         juce::Justification::centredRight);
}

void DashboardView::resized()
{
    auto area = getLocalBounds().reduced(20, 16);

    auto header = area.removeFromTop(36);
    backToProjectsButton.setBounds(header.removeFromLeft(112));
    header.removeFromLeft(12);
    headerLogoBounds = header.removeFromLeft(28).withSizeKeepingCentre(28, 28);
    header.removeFromLeft(12);

    signOutButton.setBounds(header.removeFromRight(92));
    header.removeFromRight(6);
    branchComboBox.setBounds(header.removeFromRight(148).withSizeKeepingCentre(148, 32));
    header.removeFromRight(8);
    projectStatusLabel.setBounds(header.removeFromRight(100).withSizeKeepingCentre(100, 26));
    header.removeFromRight(12);
    headerProjectLabel.setBounds(header.withSizeKeepingCentre(header.getWidth(), 22));

    area.removeFromTop(12);
    headerDividerY = area.getY();
    area.removeFromTop(22);

    auto hero = area.removeFromTop(82);
    const auto patternWidth = juce::jmin(210, hero.getWidth() / 3);
    heroPatternBounds = hero.removeFromRight(patternWidth).withSizeKeepingCentre(patternWidth, 50);
    hero.removeFromRight(24);
    snapshotSectionLabel.setBounds(hero.removeFromTop(14));
    hero.removeFromTop(8);
    snapshotTitleLabel.setBounds(hero.removeFromTop(36));
    updateSnapshotTitleFont();
    hero.removeFromTop(4);
    snapshotMetaLabel.setBounds(hero.removeFromTop(18));

    area.removeFromTop(14);
    auto actionButtons = area.removeFromTop(46);
    restoreButton.setBounds(actionButtons.removeFromRight(120));
    actionButtons.removeFromRight(8);
    syncButton.setBounds(actionButtons.removeFromRight(120));
    actionButtons.removeFromRight(8);
    saveChanges.setBounds(actionButtons);
    area.removeFromTop(8);
    actionHintLabel.setBounds(area.removeFromTop(18));

    area.removeFromTop(14);
    historyDividerY = area.getY();

    auto footer = area.removeFromBottom(30);
    footerDividerY = footer.getY();
    footer.removeFromTop(1);
    auto footerRow = footer.withTrimmedTop(4);
    footerStorageLabel.setBounds(footerRow.removeFromRight(150));
    footerRow.removeFromRight(12);
    footerPathLabel.setBounds(footerRow.removeFromLeft(footerRow.getWidth() / 2));
    footerCloudLabel.setBounds(footerRow);

    auto historyHeader = area.removeFromTop(40);
    historyCountBounds = historyHeader.removeFromRight(160);
    historyLabel.setBounds(historyHeader.withSizeKeepingCentre(historyHeader.getWidth(), 14));

    const auto listBounds = area;
    versionListViewport.setBounds(listBounds);

    const int rowHeight = 54;
    const auto needsScroll = static_cast<int>(versionCards.size()) * rowHeight > listBounds.getHeight();
    const auto rowWidth = listBounds.getWidth() - (needsScroll ? 12 : 0);
    int y = 0;
    for (auto* card : versionCards)
    {
        card->setBounds(0, y, rowWidth, rowHeight);
        y += rowHeight;
    }
    versionListContent.setBounds(0, 0, rowWidth, juce::jmax(listBounds.getHeight(), y));
}

void DashboardView::rebuildVersionCards()
{
    versionCards.clear(true);

    if (comboVersionIds.empty() || versionDisplayLabels.empty())
    {
        auto* card = static_cast<VersionCardComponent*>(versionCards.add(new VersionCardComponent()));
        card->setData(VersionCardData { "No snapshots yet", "Save a new snapshot to start this history.", {}, 0, false, true });
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
        const auto shortId = extractVersionShortId(label);
        auto meta = extractVersionTimestamp(label);
        if (meta.isEmpty())
            meta = "Version " + shortId;

        card->setData(VersionCardData {
            isGenericSnapshotTitle(extractVersionTitle(label)) ? juce::String("Snapshot")
                                                               : extractVersionTitle(label),
            meta,
            shortId,
            static_cast<int>(i),
            versionId == selectedVersionId,
            false
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
        snapshotTitleLabel.setText("NO SNAPSHOTS YET.", juce::dontSendNotification);
        juce::String metadata = branchComboBox.getText().isNotEmpty() ? branchComboBox.getText() : juce::String();
        if (selectedProjectFilePath.isNotEmpty())
        {
            if (metadata.isNotEmpty())
                metadata += metaSeparator();
            metadata += shortenPath(selectedProjectFilePath);
        }

        if (metadata.isNotEmpty())
            snapshotMetaLabel.setText(metadata, juce::dontSendNotification);
        updateSnapshotTitleFont();
        repaint();
        return;
    }

    for (size_t i = 0; i < comboVersionIds.size() && i < versionDisplayLabels.size(); ++i)
    {
        if (comboVersionIds[i] != selectedVersionId)
            continue;

        const auto extractedTitle = extractVersionTitle(versionDisplayLabels[i]);
        snapshotTitleLabel.setText((isGenericSnapshotTitle(extractedTitle) ? juce::String("Latest snapshot")
                                                                           : extractedTitle).toUpperCase(),
                                   juce::dontSendNotification);

        auto metadata = extractVersionTimestamp(versionDisplayLabels[i]);
        if (metadata.isEmpty())
            metadata = "Version " + extractVersionShortId(versionDisplayLabels[i]);

        if (branchComboBox.getText().isNotEmpty())
            metadata += metaSeparator() + branchComboBox.getText();

        if (selectedProjectFilePath.isNotEmpty())
            metadata += metaSeparator() + shortenPath(selectedProjectFilePath, 34);

        snapshotMetaLabel.setText(metadata, juce::dontSendNotification);
        updateSnapshotTitleFont();
        repaint();
        return;
    }
}

void DashboardView::updateSnapshotTitleFont()
{
    const auto availableWidth = static_cast<float>(snapshotTitleLabel.getWidth());
    if (availableWidth <= 0.0f)
        return;

    // Shrink long save notes before truncating them.
    snapshotTitleLabel.setFont(theme::displayFont(
        theme::fitDisplayFontSize({ snapshotTitleLabel.getText() }, availableWidth, 28.0f, 19.0f)));
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

    footerStorageLabel.setText(storage.toUpperCase(), juce::dontSendNotification);
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
