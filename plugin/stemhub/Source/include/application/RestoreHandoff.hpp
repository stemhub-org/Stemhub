#pragma once

#include <optional>

#include <JuceHeader.h>

#include "domain/WorkingCopyBaseline.hpp"

// A restored copy the DAW is about to open as a project of its own. The plugin instance that
// restored it writes this just before asking the DAW to open the file. The instance the DAW then
// loads with that project takes it, once, and so knows which file it works on and which version
// that file holds; the restoring instance keeps its own file.
struct RestoreHandoff
{
    juce::String projectId;
    juce::String branchId;
    // The restored project file and its version, recorded right after the restore.
    WorkingCopyBaseline copy;
    juce::Time createdAt;
};

namespace stemhub::handoff
{
// A hand-off nobody took within this time is dropped: the DAW didn't open the copy.
inline const juce::RelativeTime kMaxAge = juce::RelativeTime::minutes(10);

void write(const juce::File& location, const RestoreHandoff& handoff);

// The waiting hand-off for projectId (for any project when projectId is empty), if it is recent
// and its file is still there. Taking it deletes it; a stale or unreadable one is deleted too,
// and one for another project is left for its instance.
std::optional<RestoreHandoff> take(const juce::File& location,
                                   const juce::String& projectId,
                                   juce::Time now = juce::Time::getCurrentTime());
}
