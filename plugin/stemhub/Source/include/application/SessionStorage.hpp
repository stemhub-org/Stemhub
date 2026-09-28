#pragma once

#include <memory>

#include <JuceHeader.h>

#include "application/CredentialStore.hpp"

// What a session keeps on disk beyond its own lifetime, shared by every plugin instance on the
// machine. Tests point it at a temporary folder.
struct SessionStorage
{
    std::shared_ptr<CredentialStore> credentials;
    // See RestoreHandoff.hpp.
    juce::File restoreHandoffFile;
    // Where opening a project from the grid restores its latest version.
    juce::File managedWorkingCopyFolder;

    // <app data>/Stemhub for the current user.
    static SessionStorage forCurrentUser();
};
