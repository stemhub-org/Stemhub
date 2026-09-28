#pragma once

#include <vector>

#include <JuceHeader.h>

#include "domain/Branch.hpp"
#include "domain/Project.hpp"
#include "domain/Version.hpp"

// Where restored versions go on disk.
namespace stemhub::restorefolders
{
// What a restored copy of versionId is named after: its project file's name without the
// extension (the history says it), else fallbackName, made safe for a folder name.
[[nodiscard]] juce::String projectName(const std::vector<VersionSummary>& versions,
                                       const juce::String& versionId,
                                       const juce::String& fallbackName);

// A folder under parent that doesn't exist yet, for restoring versionId: "<name>-<id8>", then
// "<name>-<id8> (2)", ... An earlier restore may hold the user's edits and is never reused.
[[nodiscard]] juce::File newFolder(const juce::File& parent, const juce::String& projectName, const juce::String& versionId);

// <baseFolder>/<project name>/<branch name>, where opening a project from the grid restores its
// latest version. Names are made safe for any file system; one that can't be gives way to its id.
[[nodiscard]] juce::File projectRoot(const juce::File& baseFolder, const Project& project, const Branch& branch);
}
