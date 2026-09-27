#pragma once

#include <vector>

#include <JuceHeader.h>

// Which files a save takes. The push and the dashboard's count both use this one rule.
namespace stemhub::snapshotfiles
{
// Marks a folder this plugin restored a version into: a project of its own, which a save of the
// project around it leaves out. Being a dot-file, it is never saved itself.
inline constexpr const char* kRestoredCopyMarker = ".stemhub-restored";

// The project file first, then the audio and MIDI files in its folder and subfolders, sorted by
// path. Left out: hidden files and dot-files, "Backup" folders, and copies this plugin restored
// there (folders with a kRestoredCopyMarker). Empty when the job running it is asked to stop.
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

// Lowercase hex SHA-256 of a file's bytes. Empty when the file can't be read, or when the job
// running it is asked to stop partway.
[[nodiscard]] juce::String sha256OfFile(const juce::File& file);

// The DAW project files the plugin works with, for a file chooser: "*.flp;*.als".
[[nodiscard]] juce::String projectFilePattern();

// "FL Studio" for .flp, "Ableton Live" for .als, empty otherwise.
[[nodiscard]] juce::String dawNameFor(const juce::File& projectFile);
}
