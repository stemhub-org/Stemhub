#pragma once

#include <JuceHeader.h>
#include <vector>

#include "domain/Branch.hpp"
#include "domain/Project.hpp"
#include "domain/Version.hpp"

namespace stemhub::projectfiles
{
juce::String resolveRestoreProjectName(const std::vector<VersionSummary>& versions,
                                       const juce::String& versionId,
                                       const juce::String& fallbackName);

// A folder under parent that does not exist yet, for restoring versionId: "<name>-<id8>",
// then "<name>-<id8> (2)", ... An earlier restore may hold the user's edits and is never
// reused.
juce::File chooseRestoreFolder(const juce::File& parent,
                               const juce::String& projectName,
                               const juce::String& versionId);

// <baseFolder>/<project name>/<branch name>, where opening a project from the grid restores its
// latest version. Names are made safe for any file system; an empty one falls back to its id.
juce::File getRestoredProjectRoot(const juce::File& baseFolder, const Project& project, const Branch& branch);

juce::File resolveEffectiveProjectFile(const juce::File& selectedFile,
                                       const juce::File& pendingFile);

bool openInSystem(const juce::File& file);
}
