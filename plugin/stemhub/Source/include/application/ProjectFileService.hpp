#pragma once

#include <JuceHeader.h>
#include <vector>

#include "application/VersionControlService.hpp"
#include "network/ApiUtils.hpp"

namespace stemhub::projectfiles
{
juce::String resolveRestoreProjectName(const std::vector<VersionSummary>& versions,
                                       const juce::String& versionId,
                                       const juce::String& fallbackName);

juce::Result resolveRestoreResult(const juce::File& snapshotZipFile,
                                  const juce::File& restoreDirectory,
                                  juce::File& restoredProjectFile);

juce::File resolveEffectiveProjectFile(const juce::File& selectedFile,
                                       const juce::File& pendingFile);

bool openInSystem(const juce::File& file);

juce::String tryRestoreLatestVersionToCache(const std::vector<VersionSummary>& versions,
                                            const juce::String& projectId,
                                            const juce::String& branchId,
                                            const VersionControlService& versionControlService,
                                            juce::File& restoredProjectFile);
}

