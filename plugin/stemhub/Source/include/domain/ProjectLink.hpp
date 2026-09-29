#pragma once

#include <JuceHeader.h>

// The StemHub project a project file belongs to. Each plugin instance saves it in its project file
// (see application/PluginState.hpp), so two project files never share one.
struct ProjectLink
{
    juce::String projectId;
    juce::String branchId;
    // The working copy: the project file saves upload. It may have moved since.
    juce::File workingFile;

    [[nodiscard]] bool isSet() const noexcept { return projectId.isNotEmpty(); }

    bool operator==(const ProjectLink& other) const noexcept
    {
        return projectId == other.projectId && branchId == other.branchId && workingFile == other.workingFile;
    }

    bool operator!=(const ProjectLink& other) const noexcept { return !(*this == other); }
};
