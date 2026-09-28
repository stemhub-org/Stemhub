#pragma once

#include <functional>
#include <vector>

#include <JuceHeader.h>

#include "ui/ViewModels.hpp"

// A workspace's history as a timeline, newest first: one row per version, on a rule with a node
// for each. A click selects a row; so do Return and Space once it has the keyboard focus, and the
// arrow keys move to the next row. Scrolls when it outgrows its bounds.
class VersionTimeline : public juce::Component
{
public:
    // The rule sits in a gutter left of every row; the dashboard lines its working-copy row up
    // with it.
    static constexpr int kGutter = 28;
    static constexpr int kRuleX = 9;
    static constexpr int kRowHeight = 50;

    VersionTimeline();
    ~VersionTimeline() override;

    // Rows are rebuilt only when the versions change; a new selection is shown in place. A
    // selection the list doesn't have falls back to the newest version.
    void setVersions(const std::vector<VersionListItem>& versionItems, const juce::String& selectedVersionId);

    [[nodiscard]] const std::vector<VersionListItem>& getVersions() const noexcept { return versions; }
    [[nodiscard]] const juce::String& getSelectedVersionId() const noexcept { return selectedId; }

    void resized() override;

    // The user picked another version.
    std::function<void(const juce::String& versionId)> onSelect;

private:
    class Row;

    void rebuildRows();
    void layoutRows();
    void select(const juce::String& versionId, bool byUser);
    // Moves the selection, and the keyboard focus, from row index by step rows.
    void step(int index, int stepBy);
    void scrollToShow(int index);

    std::vector<VersionListItem> versions;
    juce::String selectedId;
    juce::Viewport viewport;
    juce::Component content;
    juce::OwnedArray<Row> rows;
};
