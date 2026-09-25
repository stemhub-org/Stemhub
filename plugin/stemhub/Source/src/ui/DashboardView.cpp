#include "ui/Views.hpp"
#include <algorithm>
#include <utility>

namespace
{
namespace theme = stemhub::plugin::theme;
using Theme = theme::PluginTheme;

constexpr int kTileHeight = 176;
constexpr int kTileGap = 12;
constexpr int kTilePadding = 16;
constexpr auto kTileSpanProperty = "stemhubSpan";

// History timeline: nodes sit on a rule in a fixed gutter left of every row.
constexpr int kTimelineGutter = 28;
constexpr int kTimelineRuleX = 9;
constexpr int kVersionRowHeight = 50;

void invokeIfBound(const std::function<void()>& callback)
{
    if (callback != nullptr)
        callback();
}

// Copies the callback before running it: the handler may rebuild (and delete) the component that owns it.
void invokeDetached(const std::function<void()>& callback)
{
    if (auto detached = callback)
        detached();
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

bool isGenericSnapshotTitle(const juce::String& title)
{
    const auto trimmed = title.trim();
    return trimmed.isEmpty()
        || trimmed.equalsIgnoreCase("save from plugin")
        || trimmed.equalsIgnoreCase("no save note");
}

juce::String displayTitleFor(const VersionListItem& version)
{
    return isGenericSnapshotTitle(version.message) ? juce::String("Untitled snapshot") : version.message.trim();
}

juce::String makeStatusChipText(theme::MessageStatus status)
{
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

juce::String twoDigits(int value)
{
    return juce::String(value).paddedLeft('0', 2);
}

bool hasTime(const juce::Time& time)
{
    return time.toMilliseconds() > 0;
}

juce::String formatRelativeTime(const juce::Time& time)
{
    if (!hasTime(time))
        return {};

    const auto seconds = (juce::Time::getCurrentTime() - time).inSeconds();
    if (seconds < 60.0)
        return "Just now";
    if (seconds < 3600.0)
        return juce::String(static_cast<int>(seconds / 60.0)) + " min ago";
    if (seconds < 86400.0)
        return juce::String(static_cast<int>(seconds / 3600.0)) + " h ago";
    if (seconds < 2.0 * 86400.0)
        return "Yesterday";
    if (seconds < 7.0 * 86400.0)
        return juce::String(static_cast<int>(seconds / 86400.0)) + " days ago";

    return time.formatted("%d %b %Y");
}

juce::String formatTimestamp(const juce::Time& time, bool withYear)
{
    if (!hasTime(time))
        return "Unknown time";

    return time.formatted(withYear ? "%a %d %b %Y, %H:%M" : "%a %d %b, %H:%M");
}

//==============================================================================
// Projects grid

class NewProjectTileComponent final : public juce::Component
{
public:
    NewProjectTileComponent()
    {
        setInterceptsMouseClicks(false, true);
        setTitle("New project");
    }

    void setState(bool linkedFile, const juce::String& filePath)
    {
        hasFile = linkedFile;
        path = filePath;
        repaint();
    }

    void paint(juce::Graphics& g) override
    {
        const auto bounds = getLocalBounds();
        g.setColour(Theme::kPaper);
        g.fillRect(bounds);

        auto content = bounds.reduced(kTilePadding);
        auto header = content.removeFromTop(18);
        g.setColour(Theme::kAccent);
        g.fillRect(header.removeFromLeft(8).withSizeKeepingCentre(8, 8));
        header.removeFromLeft(8);
        theme::paintMetaText(g, "New project", header, Theme::kInk);

        content.removeFromBottom(16 + 8 + 34); // button + link/meta row, laid out by the view
        content.removeFromTop(12);

        if (hasFile)
        {
            const juce::File file(path);
            theme::paintMetaText(g, "From", content.removeFromTop(14), Theme::kInkSubtle, juce::Justification::centredLeft, 9.5f);
            content.removeFromTop(2);
            g.setColour(Theme::kInk);
            g.setFont(theme::headingFont(14.5f));
            g.drawFittedText(file.getFileName(), content.removeFromTop(20), juce::Justification::centredLeft, 1, 1.0f);
            g.setColour(Theme::kInkSubtle);
            g.setFont(theme::bodyFont(11.5f));
            g.drawFittedText(file.getParentDirectory().getFullPathName(), content.removeFromTop(16),
                             juce::Justification::centredLeft, 1, 1.0f);
        }
        else
        {
            g.setColour(Theme::kInkSubtle);
            g.setFont(theme::bodyFont(12.5f));
            g.drawFittedText("Turn the DAW project you are working on into a StemHub project.",
                             content, juce::Justification::topLeft, 3, 1.0f);

            const auto metaRow = bounds.reduced(kTilePadding).removeFromBottom(16);
            theme::paintMetaText(g, ".flp  " + theme::middleDot() + "  .als", metaRow, Theme::kInkSubtle,
                                 juce::Justification::centredLeft, 9.5f);
        }
    }

private:
    bool hasFile { false };
    juce::String path;
};

class ProjectTileComponent final : public juce::Component
{
public:
    std::function<void()> onOpen;

    ProjectTileComponent()
    {
        setMouseCursor(juce::MouseCursor::PointingHandCursor);
    }

    void setData(ProjectListItem nextProject, int nextIndex, bool isSelected)
    {
        project = std::move(nextProject);
        index = nextIndex;
        selected = isSelected;
        setTitle(project.name);
        setDescription(project.description);
        repaint();
    }

    void setSelected(bool isSelected)
    {
        selected = isSelected;
        repaint();
    }

    [[nodiscard]] const juce::String& getProjectId() const noexcept { return project.id; }

    void paint(juce::Graphics& g) override
    {
        const auto hovered = isEnabled() && isMouseOver(true);
        const auto bounds = getLocalBounds();
        const auto foreground = selected ? Theme::kInk : Theme::kForeground;
        const auto subtle = selected ? Theme::kInk.withAlpha(0.66f) : Theme::kForegroundSubtle;

        g.setColour(selected ? Theme::kAccent : (hovered ? Theme::kSurfaceElevated : Theme::kSurface));
        g.fillRect(bounds);
        if (!selected)
        {
            g.setColour(hovered ? Theme::kForeground.withAlpha(0.35f) : Theme::kSurfaceBorder);
            g.drawRect(bounds, 1);
        }

        auto content = bounds.reduced(kTilePadding);

        auto header = content.removeFromTop(18);
        theme::paintMetaText(g, twoDigits(index + 1), header, subtle);
        const auto visibility = juce::String(project.isPublic ? "Public" : "Private");
        const auto tagArea = header.removeFromRight(static_cast<int>(theme::tagWidth(visibility))).toFloat();
        const auto tagColour = selected ? Theme::kInk : (project.isPublic ? Theme::kAccent : Theme::kForegroundSubtle);
        theme::paintTag(g, visibility, tagArea, tagColour.withMultipliedAlpha(project.isPublic || selected ? 1.0f : 0.6f),
                        tagColour, true);

        content.removeFromTop(14);
        theme::paintSequencerArt(g, content.removeFromTop(38).toFloat(), project.id + project.name, 3, 16,
                                 selected ? Theme::kInk.withAlpha(0.28f)
                                          : Theme::kForeground.withAlpha(hovered ? 0.36f : 0.2f),
                                 selected ? Theme::kInk : Theme::kAccent);

        auto metaRow = content.removeFromBottom(16);
        if (hovered || selected)
        {
            g.setColour(selected ? Theme::kInk : Theme::kAccent);
            g.setFont(theme::headingFont(15.0f));
            g.drawText(theme::arrowRight(), metaRow.removeFromRight(18), juce::Justification::centredRight, false);
            metaRow.removeFromRight(6);
        }

        const auto category = project.category.trim().isNotEmpty() ? project.category.trim() : juce::String("Project");
        const auto categoryWidth = juce::jmin(metaRow.getWidth(),
                                              static_cast<int>(std::ceil(juce::GlyphArrangement::getStringWidth(
                                                  theme::labelFont(9.5f), category.toUpperCase()))));
        theme::paintMetaText(g, category, metaRow.removeFromLeft(categoryWidth), subtle, juce::Justification::centredLeft, 9.5f);

        if (project.description.trim().isNotEmpty() && metaRow.getWidth() > 30)
        {
            metaRow.removeFromLeft(4);
            g.setColour(subtle);
            g.setFont(theme::bodyFont(11.5f));
            g.drawText(theme::middleDot() + "  " + project.description.trim(), metaRow, juce::Justification::centredLeft, true);
        }

        content.removeFromBottom(8);
        juce::StringArray words;
        words.addTokens(project.name.toUpperCase(), " ", "");
        const auto fontSize = theme::fitDisplayFontSize(words, static_cast<float>(content.getWidth()), 17.0f, 12.0f);
        theme::paintDisplayText(g, project.name.toUpperCase(), content, fontSize, foreground, 2, true);
    }

    void mouseEnter(const juce::MouseEvent&) override { repaint(); }
    void mouseExit(const juce::MouseEvent&) override { repaint(); }

    void enablementChanged() override
    {
        setMouseCursor(isEnabled() ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
        repaint();
    }

    // JUCE still delivers mouse events to disabled components.
    void mouseUp(const juce::MouseEvent& event) override
    {
        if (isEnabled() && event.mouseWasClicked())
            invokeDetached(onOpen);
    }

private:
    ProjectListItem project;
    int index { 0 };
    bool selected { false };
};

// Loading skeletons and empty states share the grid so the layout never jumps.
class PlaceholderTileComponent final : public juce::Component
{
public:
    PlaceholderTileComponent(juce::String titleText, juce::String messageText, juce::String patternSeed)
        : title(std::move(titleText)), message(std::move(messageText)), seed(std::move(patternSeed))
    {
        setInterceptsMouseClicks(false, false);
    }

    void paint(juce::Graphics& g) override
    {
        const auto bounds = getLocalBounds();
        g.setColour(Theme::kSurfaceBorder);
        g.drawRect(bounds, 1);

        auto content = bounds.reduced(kTilePadding);
        content.removeFromTop(18 + 14);
        theme::paintSequencerArt(g, content.removeFromTop(38).withWidth(juce::jmin(content.getWidth(), 184)).toFloat(),
                                 seed, 3, 16, Theme::kForeground.withAlpha(0.08f), Theme::kForeground.withAlpha(0.16f));

        if (title.isEmpty())
            return;

        auto text = content.removeFromBottom(58);
        theme::paintDisplayText(g, title, text.removeFromTop(22), 17.0f, Theme::kForeground, 1);
        text.removeFromTop(6);
        g.setColour(Theme::kForegroundSubtle);
        g.setFont(theme::bodyFont(12.0f));
        g.drawFittedText(message, text, juce::Justification::topLeft, 2, 1.0f);
    }

private:
    juce::String title;
    juce::String message;
    juce::String seed;
};

//==============================================================================
// Session history timeline

struct VersionRowData
{
    VersionListItem version;
    int number { 0 };
    bool selected { false };
    bool isLast { false };
    bool placeholder { false };
};

class VersionRowComponent final : public juce::Component
{
public:
    std::function<void()> onSelect;

    void setData(VersionRowData nextData)
    {
        data = std::move(nextData);
        setTitle(data.placeholder ? juce::String("No snapshots yet") : displayTitleFor(data.version));
        setMouseCursor(data.placeholder ? juce::MouseCursor::NormalCursor : juce::MouseCursor::PointingHandCursor);
        repaint();
    }

    void setSelected(bool isSelected)
    {
        data.selected = isSelected;
        repaint();
    }

    [[nodiscard]] const juce::String& getVersionId() const noexcept { return data.version.id; }

    void paint(juce::Graphics& g) override
    {
        const auto bounds = getLocalBounds();
        const auto hovered = !data.placeholder && isMouseOver(true);
        const auto centreY = static_cast<float>(bounds.getCentreY());
        auto block = bounds.withTrimmedLeft(kTimelineGutter - 6);

        if (data.selected)
        {
            g.setColour(Theme::kAccent);
            g.fillRect(block);
        }
        else if (hovered)
        {
            g.setColour(Theme::kSurface);
            g.fillRect(block);
        }

        g.setColour(Theme::kSurfaceBorder);
        g.fillRect(static_cast<float>(kTimelineRuleX), 0.0f, 1.0f,
                   data.isLast ? centreY : static_cast<float>(bounds.getHeight()));

        const juce::Rectangle<float> node { static_cast<float>(kTimelineRuleX) + 0.5f - 5.0f, centreY - 5.0f, 10.0f, 10.0f };
        if (data.placeholder)
        {
            g.setColour(Theme::kBackground);
            g.fillRect(node);
            g.setColour(Theme::kForegroundTertiary);
            g.drawRect(node, 1.0f);
        }
        else if (data.version.isOpenInDaw)
        {
            g.setColour(Theme::kAccent);
            g.fillRect(node);
        }
        else
        {
            g.setColour(data.selected ? Theme::kForeground : Theme::kBackground);
            g.fillRect(node);
            g.setColour(Theme::kForeground.withAlpha(data.selected ? 1.0f : 0.45f));
            g.drawRect(node, 1.0f);
        }

        const auto ink = data.selected;
        auto content = block.reduced(12, 0);

        if (data.placeholder)
        {
            auto text = content.withSizeKeepingCentre(content.getWidth(), 36);
            g.setColour(Theme::kForegroundSubtle);
            g.setFont(theme::headingFont(13.5f));
            g.drawText("No snapshots yet", text.removeFromTop(20), juce::Justification::centredLeft, true);
            g.setColour(Theme::kForegroundTertiary);
            g.setFont(theme::bodyFont(11.5f));
            g.drawText("Your first save starts this timeline.", text, juce::Justification::centredLeft, true);
            return;
        }

        const auto relative = formatRelativeTime(data.version.createdAt);
        theme::paintMetaText(g, relative, content.removeFromRight(84), ink ? Theme::kInk.withAlpha(0.7f) : Theme::kForegroundSubtle,
                             juce::Justification::centredRight, 9.5f);

        if (data.version.isOpenInDaw)
        {
            const auto label = juce::String("In DAW");
            const auto width = static_cast<int>(theme::tagWidth(label));
            content.removeFromRight(10);
            const auto tagArea = content.removeFromRight(width).withSizeKeepingCentre(width, 18).toFloat();
            theme::paintTag(g, label, tagArea, ink ? Theme::kInk : Theme::kAccent, ink ? Theme::kInk : Theme::kAccent, true);
        }
        content.removeFromRight(12);

        auto text = content.withSizeKeepingCentre(content.getWidth(), 36);
        g.setColour(ink ? Theme::kInk : Theme::kForeground);
        g.setFont(theme::headingFont(13.5f));
        g.drawText(displayTitleFor(data.version), text.removeFromTop(20), juce::Justification::centredLeft, true);

        g.setColour(ink ? Theme::kInk.withAlpha(0.7f) : Theme::kForegroundSubtle);
        g.setFont(theme::bodyFont(11.5f));
        g.drawText("V" + twoDigits(data.number) + metaSeparator() + formatTimestamp(data.version.createdAt, false),
                   text, juce::Justification::centredLeft, true);
    }

    void mouseEnter(const juce::MouseEvent&) override { repaint(); }
    void mouseExit(const juce::MouseEvent&) override { repaint(); }

    void mouseUp(const juce::MouseEvent& event) override
    {
        if (!data.placeholder && event.mouseWasClicked())
            invokeDetached(onSelect);
    }

private:
    VersionRowData data;
};
}

//==============================================================================
ProjectSelectionView::ProjectSelectionView()
{
    addAndMakeVisible(titleLabel);
    titleLabel.setText("PROJECTS", juce::dontSendNotification);
    titleLabel.setFont(theme::displayFont(21.0f));
    titleLabel.setColour(juce::Label::textColourId, Theme::kForeground);
    titleLabel.setJustificationType(juce::Justification::centredLeft);
    titleLabel.setMinimumHorizontalScale(1.0f);
    titleLabel.setBorderSize({});

    addAndMakeVisible(accountLabel);
    theme::styleMetaLabel(accountLabel, {}, Theme::kForegroundSubtle);
    accountLabel.setJustificationType(juce::Justification::centredRight);

    addAndMakeVisible(statusLabel);
    theme::makeInlineStatus(statusLabel);
    theme::styleStatusLabel(statusLabel, {}, theme::MessageStatus::neutral);
    statusLabel.setVisible(false);

    addAndMakeVisible(searchInput);
    theme::styleTextInput(searchInput, "Search projects");
    searchInput.onTextChange = [this] { rebuildProjectTiles(); };
    searchInput.onEscapeKey = [this] { searchInput.clear(); rebuildProjectTiles(); };

    const auto bindFilter = [this](juce::TextButton& button, ProjectFilter filter)
    {
        addAndMakeVisible(button);
        button.onClick = [this, filter]
        {
            activeProjectFilter = filter;
            updateProjectFilterButtons();
            rebuildProjectTiles();
        };
    };
    bindFilter(filterAllButton, ProjectFilter::All);
    bindFilter(filterPrivateButton, ProjectFilter::Private);
    bindFilter(filterPublicButton, ProjectFilter::Public);
    updateProjectFilterButtons();

    addAndMakeVisible(projectGridViewport);
    projectGridViewport.setViewedComponent(&projectGridContent, false);
    projectGridViewport.setScrollBarsShown(true, false);
    projectGridViewport.setScrollBarThickness(8);

    // The "new project" controls live inside the first grid tile, so they scroll with it.
    projectGridContent.addAndMakeVisible(chooseProjectFileButton);
    chooseProjectFileButton.onClick = [this] { invokeIfBound(onChooseProjectFile); };

    projectGridContent.addAndMakeVisible(createProjectButton);
    createProjectButton.onClick = [this] { invokeIfBound(onCreateProject); };

    addAndMakeVisible(signOutButton);
    signOutButton.setButtonText("Sign out");
    theme::styleGhostButton(signOutButton);
    signOutButton.onClick = [this] { invokeIfBound(onSignOut); };

    updateNewProjectControls();
    rebuildProjectTiles();
}

void ProjectSelectionView::setMessage(const juce::String& message, stemhub::plugin::theme::MessageStatus status)
{
    theme::styleStatusLabel(statusLabel, message, status);
    const auto isProblem = status == theme::MessageStatus::warning || status == theme::MessageStatus::error;
    statusLabel.setColour(juce::Label::textColourId, isProblem ? Theme::kForeground : Theme::kForegroundSubtle);
    statusLabel.setFont(theme::bodyFont(12.0f));
    statusLabel.setTooltip(message);
    statusLabel.setVisible(message.isNotEmpty());

    const auto wasLoading = isLoadingProjects;
    isLoadingProjects = status == theme::MessageStatus::loading;
    if (wasLoading != isLoadingProjects)
        rebuildProjectTiles();

    resized();
}

void ProjectSelectionView::setProjects(const std::vector<ProjectListItem>& projects, const juce::String& projectId)
{
    allProjects = projects;

    const auto stillListed = [this](const juce::String& id)
    {
        return std::any_of(allProjects.begin(), allProjects.end(), [&id](const auto& project) { return project.id == id; });
    };

    if (projectId.isNotEmpty())
        selectedProjectId = projectId;
    else if (!stillListed(selectedProjectId))
        selectedProjectId.clear();

    rebuildProjectTiles();
    repaint();
}

void ProjectSelectionView::setProjectFileSelectionState(bool fileSelected, const juce::String& selectedProjectFilePathToShow)
{
    hasProjectFile = fileSelected;
    selectedProjectFilePath = fileSelected ? selectedProjectFilePathToShow : juce::String();
    canCreateProject = canCreateProject && hasProjectFile;
    updateNewProjectControls();
    rebuildProjectTiles();
}

void ProjectSelectionView::setCanCreateProject(bool canCreate)
{
    canCreateProject = canCreate && hasProjectFile;
    updateNewProjectControls();
}

void ProjectSelectionView::setAccountName(const juce::String& accountName)
{
    accountLabel.setText(accountName.toUpperCase(), juce::dontSendNotification);
    resized();
}

void ProjectSelectionView::setActivity(SessionActivity activity)
{
    // The tiles and the new-project controls wait for the project being opened or created.
    projectGridContent.setEnabled(activity == SessionActivity::idle);
}

void ProjectSelectionView::paint(juce::Graphics& g)
{
    g.fillAll(Theme::kBackground);

    theme::paintLogoTile(g, headerLogoBounds.toFloat(), Theme::kForeground, Theme::kInk);
    theme::paintMetaText(g,
                         isLoadingProjects && allProjects.empty() ? juce::String("--")
                                                                  : twoDigits(static_cast<int>(allProjects.size())),
                         projectCountBounds,
                         Theme::kForegroundSubtle);

    g.setColour(Theme::kSurfaceBorder);
    g.fillRect(24, headerDividerY, getWidth() - 48, 1);
    g.setColour(Theme::kBorderSubtle);
    g.fillRect(24, filterAllButton.getBottom() - 1, getWidth() - 48, 1);
}

void ProjectSelectionView::resized()
{
    auto area = getLocalBounds().reduced(24, 16);

    auto header = area.removeFromTop(36);
    headerLogoBounds = header.removeFromLeft(28).withSizeKeepingCentre(28, 28);
    header.removeFromLeft(12);
    const auto titleWidth = static_cast<int>(std::ceil(juce::GlyphArrangement::getStringWidth(titleLabel.getFont(),
                                                                                              titleLabel.getText()))) + 4;
    titleLabel.setBounds(header.removeFromLeft(titleWidth).withSizeKeepingCentre(titleWidth, 26));
    header.removeFromLeft(10);
    projectCountBounds = header.removeFromLeft(28).withTrimmedTop(2);

    signOutButton.setBounds(header.removeFromRight(84));

    if (accountLabel.getText().isNotEmpty())
    {
        const auto accountWidth = juce::jmin(140, static_cast<int>(std::ceil(juce::GlyphArrangement::getStringWidth(
                                                      accountLabel.getFont(), accountLabel.getText()))) + 4);
        header.removeFromRight(4);
        accountLabel.setBounds(header.removeFromRight(accountWidth));
        header.removeFromRight(18);
    }
    else
    {
        accountLabel.setBounds({});
        header.removeFromRight(12);
    }

    const auto searchWidth = juce::jmin(220, header.getWidth());
    searchInput.setBounds(header.removeFromRight(searchWidth).withSizeKeepingCentre(searchWidth, 34));

    area.removeFromTop(12);
    headerDividerY = area.getY();
    area.removeFromTop(1 + 10);

    auto tabsRow = area.removeFromTop(30);
    for (auto* tab : { &filterAllButton, &filterPrivateButton, &filterPublicButton })
    {
        const auto width = static_cast<int>(std::ceil(juce::GlyphArrangement::getStringWidth(
                               theme::labelFont(10.5f), tab->getButtonText().toUpperCase()))) + 4;
        tab->setBounds(tabsRow.removeFromLeft(width));
        tabsRow.removeFromLeft(22);
    }

    if (statusLabel.isVisible())
    {
        const auto textWidth = static_cast<int>(std::ceil(juce::GlyphArrangement::getStringWidth(statusLabel.getFont(),
                                                                                                 statusLabel.getText())));
        const auto width = juce::jmin(tabsRow.getWidth(), textWidth + 20);
        statusLabel.setBounds(tabsRow.removeFromRight(width).withTrimmedBottom(4));
    }
    else
    {
        statusLabel.setBounds({});
    }

    area.removeFromTop(16);
    area.removeFromBottom(2);
    projectGridViewport.setBounds(area);
    layoutProjectGrid();
}

void ProjectSelectionView::layoutProjectGrid()
{
    const auto layoutWithWidth = [this](int width, bool apply)
    {
        const auto columns = width >= 560 ? 3 : 2;
        const auto tileWidth = (width - kTileGap * (columns - 1)) / columns;
        int column = 0;
        int row = 0;

        for (auto* tile : projectTiles)
        {
            const auto span = juce::jlimit(1, columns, static_cast<int>(tile->getProperties().getWithDefault(kTileSpanProperty, 1)));
            if (column + span > columns)
            {
                column = 0;
                ++row;
            }

            if (apply)
                tile->setBounds(column * (tileWidth + kTileGap),
                                row * (kTileHeight + kTileGap),
                                tileWidth * span + kTileGap * (span - 1),
                                kTileHeight);

            column += span;
        }

        const auto rows = projectTiles.isEmpty() ? 0 : row + 1;
        return rows * kTileHeight + juce::jmax(0, rows - 1) * kTileGap;
    };

    auto width = projectGridViewport.getWidth();
    if (layoutWithWidth(width, false) > projectGridViewport.getHeight())
        width -= 12;

    const auto height = layoutWithWidth(width, true);
    projectGridContent.setSize(width, height);

    juce::Rectangle<int> newTileBounds;
    for (auto* tile : projectTiles)
        if (dynamic_cast<NewProjectTileComponent*>(tile) != nullptr)
            newTileBounds = tile->getBounds();

    auto inner = newTileBounds.reduced(kTilePadding);
    const auto bottomRow = inner.removeFromBottom(16);
    inner.removeFromBottom(8);
    const auto buttonRow = inner.removeFromBottom(34);

    if (hasProjectFile)
    {
        createProjectButton.setBounds(buttonRow);
        chooseProjectFileButton.setBounds(bottomRow.withWidth(juce::jmin(bottomRow.getWidth(), 96)));
    }
    else
    {
        createProjectButton.setBounds({});
        chooseProjectFileButton.setBounds(buttonRow);
    }

    createProjectButton.toFront(false);
    chooseProjectFileButton.toFront(false);
}

void ProjectSelectionView::rebuildProjectTiles()
{
    projectTiles.clear(true);

    auto* newTile = new NewProjectTileComponent();
    newTile->setState(hasProjectFile, selectedProjectFilePath);
    projectTiles.add(newTile);
    projectGridContent.addAndMakeVisible(newTile);

    const auto query = searchInput.getText().trim().toLowerCase();

    for (size_t i = 0; i < allProjects.size(); ++i)
    {
        const auto& project = allProjects[i];

        if ((activeProjectFilter == ProjectFilter::Private && project.isPublic)
            || (activeProjectFilter == ProjectFilter::Public && !project.isPublic))
            continue;

        if (query.isNotEmpty()
            && !project.name.toLowerCase().contains(query)
            && !project.description.toLowerCase().contains(query)
            && !project.category.toLowerCase().contains(query))
            continue;

        auto* tile = new ProjectTileComponent();
        tile->setData(project, static_cast<int>(i), project.id == selectedProjectId);
        tile->onOpen = [this, id = project.id]
        {
            selectProjectById(id, true);
        };
        projectTiles.add(tile);
        projectGridContent.addAndMakeVisible(tile);
    }

    if (projectTiles.size() == 1)
    {
        if (isLoadingProjects && allProjects.empty())
        {
            for (int i = 0; i < 2; ++i)
            {
                auto* skeleton = new PlaceholderTileComponent({}, {}, "loading-" + juce::String(i));
                projectTiles.add(skeleton);
                projectGridContent.addAndMakeVisible(skeleton);
            }
        }
        else
        {
            juce::String title = "NOTHING HERE YET.";
            juce::String message = "Projects you create or join on StemHub show up here.";

            if (query.isNotEmpty())
            {
                title = "NO MATCH.";
                message = "Nothing matches \"" + searchInput.getText().trim() + "\". Try another name.";
            }
            else if (activeProjectFilter != ProjectFilter::All && !allProjects.empty())
            {
                title = activeProjectFilter == ProjectFilter::Public ? "NO PUBLIC PROJECTS." : "NO PRIVATE PROJECTS.";
                message = "Switch to All to see every project on this account.";
            }

            auto* empty = new PlaceholderTileComponent(title, message, "no-projects");
            empty->getProperties().set(kTileSpanProperty, 2);
            projectTiles.add(empty);
            projectGridContent.addAndMakeVisible(empty);
        }
    }

    layoutProjectGrid();
    repaint();
}

void ProjectSelectionView::updateProjectFilterButtons()
{
    theme::styleTabButton(filterAllButton, activeProjectFilter == ProjectFilter::All);
    theme::styleTabButton(filterPrivateButton, activeProjectFilter == ProjectFilter::Private);
    theme::styleTabButton(filterPublicButton, activeProjectFilter == ProjectFilter::Public);
}

void ProjectSelectionView::updateNewProjectControls()
{
    if (hasProjectFile)
    {
        theme::stylePrimaryButton(createProjectButton);
        createProjectButton.setButtonText("Create project  +");
        createProjectButton.setTooltip("Create a StemHub project from this DAW file.");
        createProjectButton.setVisible(true);
        createProjectButton.setEnabled(canCreateProject);

        theme::styleLinkButton(chooseProjectFileButton, Theme::kInkSubtle, Theme::kInk);
        chooseProjectFileButton.setButtonText("Change file");
        chooseProjectFileButton.getProperties().set("underlined", true);
        chooseProjectFileButton.getProperties().set("stemhubAlignLeft", true);
    }
    else
    {
        createProjectButton.setVisible(false);

        theme::stylePrimaryButton(chooseProjectFileButton);
        chooseProjectFileButton.setColour(juce::TextButton::buttonColourId, Theme::kInk);
        chooseProjectFileButton.setColour(juce::TextButton::textColourOffId, Theme::kPaper);
        chooseProjectFileButton.setColour(juce::TextButton::textColourOnId, Theme::kPaper);
        chooseProjectFileButton.setButtonText("Choose DAW file  " + theme::arrowRight());
        chooseProjectFileButton.getProperties().set("underlined", false);
        chooseProjectFileButton.getProperties().set("stemhubAlignLeft", false);
    }

    chooseProjectFileButton.setTooltip("Pick the .flp or .als file this project should track.");
    layoutProjectGrid();
}

void ProjectSelectionView::selectProjectById(const juce::String& projectId, bool triggerOpen)
{
    selectedProjectId = projectId;

    for (auto* tile : projectTiles)
        if (auto* projectTile = dynamic_cast<ProjectTileComponent*>(tile))
            projectTile->setSelected(projectTile->getProjectId() == selectedProjectId);

    if (triggerOpen)
        invokeIfBound(onOpenProject);
}

//==============================================================================
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

    addAndMakeVisible(actionHintLabel);
    theme::makeInlineStatus(actionHintLabel);
    theme::styleStatusLabel(actionHintLabel, {}, theme::MessageStatus::neutral);

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

    addAndMakeVisible(restoreHintLabel);
    restoreHintLabel.setText("Opens in your DAW as a new copy. Your project stays as it is.",
                             juce::dontSendNotification);
    restoreHintLabel.setFont(theme::bodyFont(11.5f));
    restoreHintLabel.setColour(juce::Label::textColourId, Theme::kInkSubtle);
    restoreHintLabel.setJustificationType(juce::Justification::topLeft);
    restoreHintLabel.setMinimumHorizontalScale(1.0f);
    restoreHintLabel.setBorderSize({});

    addAndMakeVisible(branchComboBox);
    branchComboBox.setTextWhenNothingSelected("Workspace");
    theme::styleComboBox(branchComboBox);
    branchComboBox.onChange = [this] { invokeIfBound(onBranchChange); };

    addAndMakeVisible(backToProjectsButton);
    backToProjectsButton.setButtonText(theme::arrowLeft() + "  Projects");
    theme::styleGhostButton(backToProjectsButton);
    backToProjectsButton.onClick = [this] { invokeIfBound(onBackToProjects); };

    addAndMakeVisible(commitMessageInput);
    theme::styleTextInput(commitMessageInput, "What changed? (optional)");
    commitMessageInput.onReturnKey = [this] { invokeIfBound(onSave); };

    addAndMakeVisible(saveChanges);
    saveChanges.setButtonText("Save snapshot");
    theme::stylePrimaryButton(saveChanges);
    saveChanges.setTooltip("Save the working copy as a new version (Cmd/Ctrl + S).");
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

    addAndMakeVisible(restoreButton);
    restoreButton.setButtonText("Restore this version");
    theme::stylePrimaryButton(restoreButton);
    restoreButton.setColour(juce::TextButton::buttonColourId, Theme::kInk);
    restoreButton.setColour(juce::TextButton::textColourOffId, Theme::kPaper);
    restoreButton.setColour(juce::TextButton::textColourOnId, Theme::kPaper);
    restoreButton.onClick = [this] { invokeIfBound(onRestore); };

    addAndMakeVisible(versionListViewport);
    versionListViewport.setViewedComponent(&versionListContent, false);
    versionListViewport.setScrollBarsShown(true, false);
    versionListViewport.setScrollBarThickness(8);

    rebuildVersionRows();
    updateDetailControls();
    updateFooterSummary();
}

void DashboardView::setProjectStatusMessage(const juce::String& message, stemhub::plugin::theme::MessageStatus status)
{
    theme::styleStatusLabel(projectStatusLabel, makeStatusChipText(status), status);
    projectStatusLabel.setTooltip(message);

    theme::styleStatusLabel(actionHintLabel, message.isNotEmpty() ? message : juce::String("Ready."), status);
    const auto isProblem = status == theme::MessageStatus::warning || status == theme::MessageStatus::error;
    actionHintLabel.setColour(juce::Label::textColourId, isProblem ? Theme::kForeground : Theme::kForegroundSubtle);
    actionHintLabel.setFont(theme::bodyFont(11.5f));
    actionHintLabel.setTooltip(message);
}

void DashboardView::setSelectedProjectFilePath(const juce::String& projectFilePath)
{
    selectedProjectFilePath = projectFilePath;
    updateFooterSummary();
    repaint();
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
}

void DashboardView::setBranches(const std::vector<juce::String>& branchNames,
                                const std::vector<juce::String>& branchIds,
                                const juce::String& selectedBranchId)
{
    setMappedComboItems(branchComboBox, comboBranchIds, branchNames, branchIds, selectedBranchId);
}

void DashboardView::setVersions(const std::vector<VersionListItem>& versionItems, const juce::String& versionId)
{
    versions = versionItems;

    const auto isListed = std::any_of(versions.begin(), versions.end(), [&versionId](const auto& version)
    {
        return version.id == versionId;
    });

    // Fall back to the newest version, as the history always did.
    selectedVersionId = isListed ? versionId : (versions.empty() ? juce::String() : versions.front().id);

    rebuildVersionRows();
    updateDetailControls();
    repaint();
}

void DashboardView::setPackagedFiles(const juce::String& rootLabel, const std::vector<juce::String>& relativeFilePaths)
{
    juce::ignoreUnused(rootLabel);
    packagedFileCount = static_cast<int>(relativeFilePaths.size());
    updateFooterSummary();
    repaint();
}

void DashboardView::setActivity(SessionActivity activity)
{
    const auto isIdle = activity == SessionActivity::idle;
    const auto isSaving = activity == SessionActivity::saving;
    const auto isRestoring = activity == SessionActivity::restoring;

    saveChanges.setEnabled(isIdle);
    theme::setButtonBusy(saveChanges, isSaving);
    saveChanges.setButtonText(isSaving ? "Saving..." : "Save snapshot");

    restoreButton.setEnabled(isIdle);
    theme::setButtonBusy(restoreButton, isRestoring);
    restoreButton.setButtonText(isRestoring ? "Restoring..." : "Restore this version");

    syncButton.setEnabled(isIdle);
    branchComboBox.setEnabled(isIdle);

    // A save or restore belongs to this project, and a save to the note being typed.
    backToProjectsButton.setEnabled(!isSaving && !isRestoring);
    commitMessageInput.setEnabled(!isSaving && !isRestoring);
}

juce::String DashboardView::getSelectedBranchId() const
{
    return getMappedComboSelection(branchComboBox, comboBranchIds);
}

const VersionListItem* DashboardView::findSelectedVersion() const
{
    const auto index = indexOfSelectedVersion();
    return index >= 0 ? &versions[static_cast<size_t>(index)] : nullptr;
}

int DashboardView::indexOfSelectedVersion() const
{
    for (size_t i = 0; i < versions.size(); ++i)
        if (versions[i].id == selectedVersionId)
            return static_cast<int>(i);

    return -1;
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
    {
        const auto ruleX = workingCopyBounds.getX() + kTimelineRuleX;
        auto content = workingCopyBounds.withTrimmedLeft(kTimelineGutter);
        auto metaRow = content.removeFromTop(16);
        const juce::Rectangle<float> node { static_cast<float>(ruleX) + 0.5f - 6.0f,
                                            static_cast<float>(metaRow.getCentreY()) - 6.0f, 12.0f, 12.0f };

        g.setColour(Theme::kSurfaceBorder);
        g.fillRect(static_cast<float>(ruleX), node.getBottom(), 1.0f,
                   static_cast<float>(versionListViewport.getY() - static_cast<int>(node.getBottom())));
        g.setColour(Theme::kBackground);
        g.fillRect(node);
        g.setColour(Theme::kAccent);
        g.drawRect(node, 1.5f);

        const auto fileName = selectedProjectFilePath.isNotEmpty() ? juce::File(selectedProjectFilePath).getFileName()
                                                                   : juce::String("No local file yet");
        auto fileText = fileName;
        if (packagedFileCount > 1)
            fileText += metaSeparator() + juce::String(packagedFileCount) + " files";

        const auto metaWidth = static_cast<int>(std::ceil(juce::GlyphArrangement::getStringWidth(theme::labelFont(10.0f),
                                                                                               "WORKING COPY"))) + 2;
        theme::paintMetaText(g, "Working copy", metaRow.removeFromLeft(metaWidth), Theme::kForeground);
        metaRow.removeFromLeft(14);
        g.setColour(Theme::kForegroundSubtle);
        g.setFont(theme::bodyFont(11.5f));
        g.drawText(fileText, metaRow, juce::Justification::centredRight, true);
    }

    // Detail card for the selected version, Paper on Ink like the sign-in card.
    g.setColour(Theme::kPaper);
    g.fillRect(detailBounds);

    auto card = detailBounds.reduced(20);
    card.removeFromBottom(restoreButton.isVisible() ? 30 + 8 + 40 + 14 : 0);

    const auto* version = findSelectedVersion();
    const auto versionIndex = indexOfSelectedVersion();
    const auto versionCount = static_cast<int>(versions.size());

    auto metaRow = card.removeFromTop(16);
    g.setColour(Theme::kAccent);
    g.fillRect(metaRow.removeFromLeft(8).withSizeKeepingCentre(8, 8));
    metaRow.removeFromLeft(8);

    if (version != nullptr && version->isOpenInDaw)
    {
        const auto label = juce::String("In your DAW");
        const auto width = static_cast<int>(theme::tagWidth(label));
        theme::paintTag(g, label, metaRow.removeFromRight(width).withSizeKeepingCentre(width, 18).toFloat(),
                        Theme::kAccent, Theme::kInk);
        metaRow.removeFromRight(8);
    }

    theme::paintMetaText(g,
                         version != nullptr ? "Version " + twoDigits(versionCount - versionIndex) + " / " + twoDigits(versionCount)
                                            : juce::String("Version --"),
                         metaRow, Theme::kInk);

    card.removeFromTop(16);

    if (version == nullptr)
    {
        const auto height = theme::paintDisplayText(g, "NOTHING SAVED YET.", card, 22.0f, Theme::kInk, 2);
        card.removeFromTop(height + 18);
        theme::paintSequencerArt(g, card.removeFromTop(30).toFloat(), "empty-history", 2, 14,
                                 Theme::kInk.withAlpha(0.1f), Theme::kInk.withAlpha(0.2f));
        card.removeFromTop(18);
        g.setColour(Theme::kInkSubtle);
        g.setFont(theme::bodyFont(12.5f));
        g.drawFittedText("Write what changed, then Save snapshot to start this branch's history.",
                         card.removeFromTop(54), juce::Justification::topLeft, 3, 1.0f);
        return;
    }

    const auto title = displayTitleFor(*version).toUpperCase();
    juce::StringArray words;
    words.addTokens(title, " ", "");
    const auto titleSize = theme::fitDisplayFontSize(words, static_cast<float>(card.getWidth()), 22.0f, 15.0f);
    const auto titleHeight = theme::paintDisplayText(g, title, card.withHeight(80), titleSize, Theme::kInk, 3);
    card.removeFromTop(titleHeight + 18);

    theme::paintBlockPattern(g, card.removeFromTop(28).toFloat(), version->id, 18, Theme::kInk, Theme::kAccent);
    card.removeFromTop(16);

    const auto addFact = [&g, &card](const juce::String& key, const juce::String& value, bool mono)
    {
        if (value.isEmpty() || card.getHeight() < 26)
            return;

        auto row = card.removeFromTop(26);
        g.setColour(Theme::kPaperBorder);
        g.fillRect(row.removeFromTop(1));
        theme::paintMetaText(g, key, row.removeFromLeft(58), Theme::kInkSubtle, juce::Justification::centredLeft, 9.5f);
        g.setColour(Theme::kInk);
        g.setFont(mono ? theme::monoFont(11.5f) : theme::mediumFont(12.0f));
        g.drawText(value, row, juce::Justification::centredLeft, true);
    };

    addFact("Saved", formatTimestamp(version->createdAt, true), false);
    addFact("DAW", version->sourceDaw, false);
    addFact("File", version->sourceFilename, false);
    addFact("Size", version->sizeBytes > 0 ? juce::File::descriptionOfSizeInBytes(version->sizeBytes) : juce::String(), false);
    addFact("ID", version->id.substring(0, 8), true);
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
    actionHintLabel.setBounds(statusBar);

    detailBounds = area.removeFromRight(juce::jlimit(220, 280, area.getWidth() * 2 / 5));
    area.removeFromRight(20);

    auto card = detailBounds.reduced(20);
    restoreHintLabel.setBounds(card.removeFromBottom(30));
    card.removeFromBottom(8);
    restoreButton.setBounds(card.removeFromBottom(40));

    auto historyHeader = area.removeFromTop(30);
    const auto captionWidth = static_cast<int>(std::ceil(juce::GlyphArrangement::getStringWidth(theme::labelFont(10.0f),
                                                                                                "BRANCH"))) + 12;
    branchCaptionBounds = historyHeader.removeFromLeft(captionWidth);
    syncButton.setBounds(historyHeader.removeFromRight(64));
    historyHeader.removeFromRight(8);
    branchComboBox.setBounds(historyHeader.removeFromLeft(juce::jmin(170, historyHeader.getWidth())));

    area.removeFromTop(16);
    workingCopyBounds = area.removeFromTop(16 + 8 + 38);
    auto controls = workingCopyBounds.withTrimmedLeft(kTimelineGutter).withTrimmedTop(16 + 8);
    saveChanges.setBounds(controls.removeFromRight(128));
    controls.removeFromRight(8);
    commitMessageInput.setBounds(controls);

    area.removeFromTop(12);
    versionListViewport.setBounds(area);
    layoutVersionRows();
}

void DashboardView::rebuildVersionRows()
{
    versionRows.clear(true);

    if (versions.empty())
    {
        auto* row = new VersionRowComponent();
        VersionRowData placeholder;
        placeholder.isLast = true;
        placeholder.placeholder = true;
        row->setData(placeholder);
        versionRows.add(row);
        versionListContent.addAndMakeVisible(row);
        layoutVersionRows();
        return;
    }

    const auto count = static_cast<int>(versions.size());
    for (int i = 0; i < count; ++i)
    {
        const auto& version = versions[static_cast<size_t>(i)];
        auto* row = new VersionRowComponent();

        VersionRowData data;
        data.version = version;
        data.number = count - i; // history is newest first; V01 is the first save
        data.selected = version.id == selectedVersionId;
        data.isLast = i == count - 1;
        row->setData(std::move(data));
        row->onSelect = [this, id = version.id]
        {
            selectVersionById(id, true);
        };
        versionRows.add(row);
        versionListContent.addAndMakeVisible(row);
    }

    layoutVersionRows();
}

void DashboardView::layoutVersionRows()
{
    const auto listHeight = static_cast<int>(versionRows.size()) * kVersionRowHeight;
    const auto needsScroll = listHeight > versionListViewport.getHeight();
    const auto rowWidth = versionListViewport.getWidth() - (needsScroll ? 12 : 0);

    int y = 0;
    for (auto* row : versionRows)
    {
        row->setBounds(0, y, rowWidth, kVersionRowHeight);
        y += kVersionRowHeight;
    }

    versionListContent.setSize(rowWidth, y);
}

void DashboardView::updateDetailControls()
{
    const auto hasVersion = findSelectedVersion() != nullptr;
    restoreButton.setVisible(hasVersion);
    restoreHintLabel.setVisible(hasVersion);
    repaint();
}

void DashboardView::updateFooterSummary()
{
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
    footerStorageLabel.setTooltip(selectedProjectFilePath);
}

void DashboardView::selectVersionById(const juce::String& versionId, bool triggerChange)
{
    selectedVersionId = versionId;

    for (auto* row : versionRows)
        if (auto* versionRow = dynamic_cast<VersionRowComponent*>(row))
            versionRow->setSelected(versionRow->getVersionId() == selectedVersionId);

    updateDetailControls();

    if (triggerChange)
        invokeIfBound(onVersionSelectionChange);
}
