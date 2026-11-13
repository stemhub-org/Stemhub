#pragma once

#include <memory>

#include <JuceHeader.h>

#include "application/CredentialStore.hpp"
#include "application/WorkingCopyIndex.hpp"

// What a session keeps on disk beyond its own lifetime, shared by every plugin instance on the
// machine. Tests point it at a temporary folder.
struct SessionStorage
{
    std::shared_ptr<CredentialStore> credentials;
    // See RestoreHandoff.hpp.
    juce::File restoreHandoffFile;
    // Where opening a project from the grid restores its latest version.
    juce::File restoredProjectsFolder;
    // Which version each local project file holds.
    WorkingCopyIndex workingCopies;

    // The current user's folders: see AppFolders.hpp.
    static SessionStorage forCurrentUser();
};
