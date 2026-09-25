#pragma once

#include <optional>
#include <vector>

#include <JuceHeader.h>

#include "domain/Branch.hpp"
#include "domain/Project.hpp"
#include "domain/Status.hpp"
#include "domain/User.hpp"
#include "domain/Version.hpp"
#include "domain/WorkingCopyBaseline.hpp"

enum class AuthState
{
    signedOut,
    signingIn,
    signedIn,
    authError
};

enum class UIState
{
    login,
    projectSelection,
    dashboard
};

// What the session is busy with. Failures are reported through the statuses, not as a state.
enum class OperationState
{
    idle,
    loadingProjects,
    committing,
    pulling,
    restoring
};

// Everything the plugin knows about the signed-in user's session. Owned by StemhubSession and
// only touched on the message thread.
struct SessionState
{
    AuthState authState { AuthState::signedOut };
    UIState uiState { UIState::login };
    OperationState operationState { OperationState::idle };

    std::optional<User> currentUser;
    juce::String accessToken;
    std::vector<Project> projects;
    std::optional<Project> selectedProject;
    std::vector<Branch> branches;
    juce::String selectedBranchId;
    juce::String selectedBranchName;
    std::vector<VersionSummary> versionHistory;
    juce::String selectedVersionId;
    // The version loaded in the DAW, as far as the plugin knows.
    juce::String openedVersionId;

    // A project file the user picked, not yet tied to a project.
    juce::File pendingProjectFile;
    // The selected project's working file.
    juce::File selectedProjectFile;
    // What that file holds; set by saves, restores and restore-folder names.
    WorkingCopyBaseline workingCopy;

    Status authStatus;     // login screen
    Status projectsStatus; // project grid
    Status sessionStatus;  // dashboard
};
