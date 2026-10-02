#pragma once

#include <functional>
#include <vector>

#include <JuceHeader.h>

#include "ui/PluginTheme.hpp"
#include "ui/ProjectTiles.hpp"
#include "ui/ViewModels.hpp"

// The project grid: the account's projects, a tile to create one from a project file, a search
// box and filters. Tiles are rebuilt only when the list or the filter changes.
class ProjectSelectionView : public juce::Component
{
public:
    ProjectSelectionView();

    void show(const ProjectGridModel& model);

    void resized() override;
    void paint(juce::Graphics& g) override;

    std::function<void()> onChooseProjectFile;
    std::function<void(const juce::String& projectId)> onOpenProject;
    std::function<void()> onCreateProject;
    std::function<void()> onSignOut;

private:
    enum class ProjectFilter { all, privateOnly, publicOnly };

    void setStatus(const Status& status);
    void setProjects(const std::vector<ProjectListItem>& projects, const juce::String& selectedProjectId);
    void setNewProjectFile(const juce::String& path);
    void setAccountName(const juce::String& accountName);
    void setActivity(SessionActivity activity);

    void rebuildProjectTiles();
    void layoutProjectGrid();
    void updateProjectFilterButtons();
    void updateNewProjectControls();
    void selectProject(const juce::String& projectId);

    std::vector<ProjectListItem> allProjects;
    juce::String selectedProjectId;
    juce::String newProjectFilePath;
    Status shownStatus;
    bool isLoadingProjects { false };
    ProjectFilter activeProjectFilter { ProjectFilter::all };
    juce::Label titleLabel;
    juce::Label accountLabel;
    juce::Label statusLabel;
    juce::TextEditor searchInput;
    juce::TextButton filterAllButton { "All" };
    juce::TextButton filterPrivateButton { "Private" };
    juce::TextButton filterPublicButton { "Public" };
    juce::Viewport projectGridViewport;
    juce::Component projectGridContent;
    juce::OwnedArray<GridTile> projectTiles;
    // The first tile.
    NewProjectTile* newProjectTile { nullptr };
    juce::TextButton chooseProjectFileButton { "Choose file" };
    juce::TextButton createProjectButton { "Create project" };
    juce::TextButton signOutButton { "Sign out" };

    juce::Rectangle<int> headerLogoBounds;
    juce::Rectangle<int> projectCountBounds;
    int headerDividerY { 0 };
};
