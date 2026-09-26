#pragma once

#include <JuceHeader.h>

// The StemHub project a DAW project belongs to. Each plugin instance saves it in its DAW project
// (see application/PluginState.hpp), so two DAW projects never share one.
struct ProjectLink
{
    juce::String projectId;
    juce::String branchId;
    // The DAW project file that saves push. It may have moved since.
    juce::File workingFile;

    [[nodiscard]] bool isSet() const noexcept { return projectId.isNotEmpty(); }

    bool operator==(const ProjectLink& other) const noexcept
    {
        return projectId == other.projectId && branchId == other.branchId && workingFile == other.workingFile;
    }

    bool operator!=(const ProjectLink& other) const noexcept { return !(*this == other); }
};
