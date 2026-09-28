#pragma once

#include <JuceHeader.h>

#include "domain/ProjectLink.hpp"

// What the plugin saves in the DAW project: <StemhubState schema="1" projectId=".." branchId=".."
// workingFile=".."/>. The version the working file holds is not saved: a restored copy carries
// the state saved with an older version, which would name the wrong one.
namespace stemhub::pluginstate
{
juce::MemoryBlock encode(const ProjectLink& link);

// Anything missing or unreadable gives an unset link, or empty fields.
ProjectLink decode(const void* data, size_t sizeInBytes);
}
