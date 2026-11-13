#pragma once

#include <functional>

#include <JuceHeader.h>

#include "ui/ViewModels.hpp"

// A tile of the project grid.
class GridTile : public juce::Component
{
public:
    static constexpr int kHeight = 176;
    static constexpr int kGap = 12;
    static constexpr int kPadding = 16;

    // How many columns the tile spans.
    int span { 1 };
};

// Creates a project from a project file: shows the file, or asks for one. Its buttons belong to
// the grid, which lays them out over the tile.
class NewProjectTile final : public GridTile
{
public:
    NewProjectTile();

    // Empty when there is no file yet.
    void setFile(const juce::String& path);
    void paint(juce::Graphics& g) override;

private:
    juce::String filePath;
};

// A project. A click opens it, and so do Return and Space once the tile has the keyboard focus.
class ProjectTile final : public GridTile
{
public:
    ProjectTile(ProjectListItem projectToShow, int indexOnGrid, bool isSelected);

    void setSelected(bool isSelected);
    [[nodiscard]] const juce::String& getProjectId() const noexcept { return project.id; }

    void paint(juce::Graphics& g) override;
    void mouseEnter(const juce::MouseEvent&) override { repaint(); }
    void mouseExit(const juce::MouseEvent&) override { repaint(); }
    void mouseUp(const juce::MouseEvent& event) override;
    bool keyPressed(const juce::KeyPress& key) override;
    void focusGained(FocusChangeType) override { repaint(); }
    void focusLost(FocusChangeType) override { repaint(); }
    void enablementChanged() override;
    std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler() override;

    std::function<void()> onOpen;

private:
    void open();

    ProjectListItem project;
    int index { 0 };
    bool selected { false };
};

// Stands in for projects while they load, or says why the grid is empty.
class PlaceholderTile final : public GridTile
{
public:
    PlaceholderTile(juce::String titleText, juce::String messageText, juce::String patternSeed);

    void paint(juce::Graphics& g) override;

private:
    juce::String title;
    juce::String message;
    juce::String seed;
};
