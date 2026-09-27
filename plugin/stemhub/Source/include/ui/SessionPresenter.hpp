#pragma once

#include <JuceHeader.h>

#include "domain/SessionState.hpp"
#include "ui/ViewModels.hpp"

// What each screen shows, worked out from the session's state: the UI's rules, kept apart from
// its widgets so the tests can check them. It remembers one thing between two calls: the last
// saved version it has seen, to tell the editor once when a save has spent the note.
class SessionPresenter
{
public:
    // Facts about files the screens mention, which only the disk can tell.
    struct FileFacts
    {
        // The file the grid would create a project from (StemhubSession::getProjectFileForGrid).
        juce::File newProjectFile;
        bool workingFileExists { false };
    };

    // savedVersionSeen: the session's lastSavedVersionId when the editor opens. That save's note
    // is gone with the previous editor.
    explicit SessionPresenter(juce::String savedVersionSeen = {});

    [[nodiscard]] SessionModel present(const SessionState& state, const FileFacts& files);

    [[nodiscard]] static SessionActivity activityFor(OperationState operation) noexcept;

private:
    juce::String lastSeenSavedVersionId;
};
