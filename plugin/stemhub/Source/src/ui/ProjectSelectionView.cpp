#include <algorithm>

#include "ui/ProjectSelectionView.hpp"
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
    searchInput.setTitle("Search projects");
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
    bindFilter(filterAllButton, ProjectFilter::all);
    bindFilter(filterPrivateButton, ProjectFilter::privateOnly);
    bindFilter(filterPublicButton, ProjectFilter::publicOnly);
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

void ProjectSelectionView::show(const ProjectGridModel& model)
{
    setAccountName(model.accountName);
    setNewProjectFile(model.newProjectFilePath);
    setStatus(model.status);
    setProjects(model.projects, model.selectedProjectId);
    setActivity(model.activity);
}

void ProjectSelectionView::setStatus(const Status& status)
{
    if (status == shownStatus)
        return;

    shownStatus = status;
    const auto messageStatus = theme::messageStatusFor(status.severity);
    theme::styleStatusLabel(statusLabel, status.text, messageStatus);
    const auto isProblem = messageStatus == theme::MessageStatus::warning || messageStatus == theme::MessageStatus::error;
    statusLabel.setColour(juce::Label::textColourId, isProblem ? Theme::kForeground : Theme::kForegroundSubtle);
    statusLabel.setFont(theme::bodyFont(12.0f));
    statusLabel.setTooltip(status.text);
    statusLabel.setVisible(status.text.isNotEmpty());

    // While the first list loads, placeholder tiles hold its place.
    const auto wasLoading = isLoadingProjects;
    isLoadingProjects = messageStatus == theme::MessageStatus::loading;
    if (wasLoading != isLoadingProjects)
        rebuildProjectTiles();

    resized();
}

void ProjectSelectionView::setProjects(const std::vector<ProjectListItem>& projects, const juce::String& projectId)
{
    const auto listChanged = projects != allProjects;
    if (listChanged)
        allProjects = projects;

    const auto isListed = [this](const juce::String& id)
    {
        return std::any_of(allProjects.begin(), allProjects.end(), [&id](const auto& project) { return project.id == id; });
    };

    const auto nextSelection = projectId.isNotEmpty() ? projectId
                                                      : (isListed(selectedProjectId) ? selectedProjectId : juce::String());

    // Tiles are rebuilt only when the list changes; a new selection is shown in place.
    if (listChanged)
    {
        selectedProjectId = nextSelection;
        rebuildProjectTiles();
    }
    else if (nextSelection != selectedProjectId)
    {
        selectProject(nextSelection);
    }
}

void ProjectSelectionView::setNewProjectFile(const juce::String& path)
{
    if (path == newProjectFilePath)
        return;

    newProjectFilePath = path;
    if (newProjectTile != nullptr)
        newProjectTile->setFile(newProjectFilePath);

    updateNewProjectControls();
}

void ProjectSelectionView::setAccountName(const juce::String& accountName)
{
    if (accountLabel.getText() == accountName.toUpperCase())
        return;

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
                                                                  : uiformat::twoDigits(static_cast<int>(allProjects.size())),
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
        const auto tileWidth = (width - GridTile::kGap * (columns - 1)) / columns;
        int column = 0;
        int row = 0;

        for (auto* tile : projectTiles)
        {
            const auto span = juce::jlimit(1, columns, tile->span);
            if (column + span > columns)
            {
                column = 0;
                ++row;
            }

            if (apply)
                tile->setBounds(column * (tileWidth + GridTile::kGap),
                                row * (GridTile::kHeight + GridTile::kGap),
                                tileWidth * span + GridTile::kGap * (span - 1),
                                GridTile::kHeight);

            column += span;
        }

        const auto rows = projectTiles.isEmpty() ? 0 : row + 1;
        return rows * GridTile::kHeight + juce::jmax(0, rows - 1) * GridTile::kGap;
    };

    auto width = projectGridViewport.getWidth();
    if (layoutWithWidth(width, false) > projectGridViewport.getHeight())
        width -= 12;

    const auto height = layoutWithWidth(width, true);
    projectGridContent.setSize(width, height);

    auto inner = newProjectTile != nullptr ? newProjectTile->getBounds().reduced(GridTile::kPadding) : juce::Rectangle<int>();
    const auto bottomRow = inner.removeFromBottom(16);
    inner.removeFromBottom(8);
    const auto buttonRow = inner.removeFromBottom(34);

    if (newProjectFilePath.isNotEmpty())
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

    newProjectTile = new NewProjectTile();
    newProjectTile->setFile(newProjectFilePath);
    projectTiles.add(newProjectTile);
    projectGridContent.addAndMakeVisible(newProjectTile);

    const auto query = searchInput.getText().trim().toLowerCase();

    for (size_t i = 0; i < allProjects.size(); ++i)
    {
        const auto& project = allProjects[i];

        if ((activeProjectFilter == ProjectFilter::privateOnly && project.isPublic)
            || (activeProjectFilter == ProjectFilter::publicOnly && !project.isPublic))
            continue;

        if (query.isNotEmpty()
            && !project.name.toLowerCase().contains(query)
            && !project.description.toLowerCase().contains(query)
            && !project.category.toLowerCase().contains(query))
            continue;

        auto* tile = new ProjectTile(project, static_cast<int>(i), project.id == selectedProjectId);
        tile->onOpen = [this, id = project.id]
        {
            selectProject(id);
            if (onOpenProject != nullptr)
                onOpenProject(id);
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
                auto* skeleton = new PlaceholderTile({}, {}, "loading-" + juce::String(i));
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
            else if (activeProjectFilter != ProjectFilter::all && !allProjects.empty())
            {
                title = activeProjectFilter == ProjectFilter::publicOnly ? "NO PUBLIC PROJECTS." : "NO PRIVATE PROJECTS.";
                message = "Switch to All to see every project on this account.";
            }

            auto* empty = new PlaceholderTile(title, message, "no-projects");
            empty->span = 2;
            projectTiles.add(empty);
            projectGridContent.addAndMakeVisible(empty);
        }
    }

    layoutProjectGrid();
    repaint();
}

void ProjectSelectionView::updateProjectFilterButtons()
{
    theme::styleTabButton(filterAllButton, activeProjectFilter == ProjectFilter::all);
    theme::styleTabButton(filterPrivateButton, activeProjectFilter == ProjectFilter::privateOnly);
    theme::styleTabButton(filterPublicButton, activeProjectFilter == ProjectFilter::publicOnly);
}

void ProjectSelectionView::updateNewProjectControls()
{
    if (newProjectFilePath.isNotEmpty())
    {
        theme::stylePrimaryButton(createProjectButton);
        createProjectButton.setButtonText("Create project  +");
        createProjectButton.setTooltip("Create a StemHub project from this project file.");
        createProjectButton.setVisible(true);

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
        chooseProjectFileButton.setButtonText("Choose project file  " + theme::arrowRight());
        chooseProjectFileButton.getProperties().set("underlined", false);
        chooseProjectFileButton.getProperties().set("stemhubAlignLeft", false);
    }

    chooseProjectFileButton.setTooltip("Pick the .flp or .als project file to save versions from.");
    layoutProjectGrid();
}

void ProjectSelectionView::selectProject(const juce::String& projectId)
{
    selectedProjectId = projectId;

    for (auto* tile : projectTiles)
        if (auto* projectTile = dynamic_cast<ProjectTile*>(tile))
            projectTile->setSelected(projectTile->getProjectId() == selectedProjectId);
}
