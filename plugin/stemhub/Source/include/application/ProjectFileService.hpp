#pragma once

#include <JuceHeader.h>
#include <vector>

#include "application/VersionControlUtils.hpp"

namespace stemhub::projectfiles
{
juce::String resolveRestoreProjectName(const std::vector<VersionSummary>& versions,
                                       const juce::String& versionId,
                                       const juce::String& fallbackName);

// A folder under parent that does not exist yet, for restoring versionId: "<name>-<id8>",
// then "<name>-<id8> (2)", ... An earlier restore may hold the user's edits and is never
// reused. The version prefix stays after the last dash, where resolveVersionIdFromProjectPath
// reads it back.
juce::File chooseRestoreFolder(const juce::File& parent,
                               const juce::String& projectName,
                               const juce::String& versionId);

// Where opening a project from the grid restores its latest version when this instance has
// no local copy: <app data>/Stemhub/working-copy.
juce::File getDefaultManagedWorkingCopyFolder();

// <baseFolder>/<project>/<branch>, with ids reduced to safe folder names.
juce::File getManagedWorkingCopyRoot(const juce::File& baseFolder,
                                     const juce::String& projectId,
                                     const juce::String& branchId);

juce::File resolveEffectiveProjectFile(const juce::File& selectedFile,
                                       const juce::File& pendingFile);

bool openInSystem(const juce::File& file);
}
