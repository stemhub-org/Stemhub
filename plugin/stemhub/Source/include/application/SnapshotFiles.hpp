#pragma once

#include <vector>

#include <JuceHeader.h>

// Which files a save takes. The push and the dashboard's count both use this one rule.
namespace stemhub::snapshotfiles
{
// The project file first, then the audio and MIDI files in its folder and subfolders, sorted by
// path. Left out: hidden files and dot-files, "Backup" folders, and copies this plugin restored
// there (a "<name>-<version>" folder holding a DAW project), which are projects of their own.
std::vector<juce::File> collect(const juce::File& projectFile);

// What a save of projectFile takes, for the dashboard.
struct Summary
{
    juce::File projectFile;
    int fileCount { 0 };
    juce::int64 totalBytes { 0 };
};

// Lists the folder, so it belongs on a background thread.
Summary summarize(const juce::File& projectFile);

[[nodiscard]] bool isDawProjectFile(const juce::File& file);

// "FL Studio" for .flp, "Ableton Live" for .als, empty otherwise.
[[nodiscard]] juce::String dawNameFor(const juce::File& projectFile);
}
