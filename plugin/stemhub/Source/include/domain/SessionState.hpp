#pragma once

#include <algorithm>
#include <optional>
#include <vector>

#include <JuceHeader.h>

#include "domain/Branch.hpp"
#include "domain/Project.hpp"
#include "domain/ProjectLink.hpp"
#include "domain/Status.hpp"
#include "domain/User.hpp"
#include "domain/Version.hpp"
#include "domain/WorkingCopyBaseline.hpp"

// The screen a signed-in user sees. Signed out, the plugin shows the login screen.
enum class UIState
{
    projectSelection,
    dashboard
};

// The job the session is busy with; there is one at a time. Failures are reported through the
// statuses, not as a state.
enum class OperationState
{
    idle,
    signingIn,
    loadingProjects,
    committing,
    pulling,
    restoring
};

// Everything the plugin knows about the signed-in user's session. Owned by StemhubSession and
// only touched on the message thread.
struct SessionState
{
    UIState uiState { UIState::projectSelection };
    OperationState operationState { OperationState::idle };

    // Both set while signed in.
    std::optional<User> currentUser;
    juce::String accessToken;

    // The StemHub project of the DAW project this instance lives in: saved there by the
    // processor, and kept across sign-outs.
    ProjectLink link;

    std::vector<Project> projects;
    std::optional<Project> selectedProject;
    std::vector<Branch> branches;
    juce::String selectedBranchId;
    std::vector<VersionSummary> versionHistory;
    juce::String selectedVersionId;
    // The version loaded in the DAW, as far as the plugin knows.
    juce::String openedVersionId;
    // The version the last save created: a save that ends any other way doesn't change it.
    juce::String lastSavedVersionId;

    // A DAW project file chosen on the project grid, for the next project opened or created there.
    juce::File chosenProjectFile;
    // The file the open project saves from, and the one its link names. It may be missing (on a
    // drive that isn't plugged in, say) and stays until the user picks another.
    juce::File workingFile;
    // What that file holds, as recorded by the last save or restore of it.
    WorkingCopyBaseline workingCopy;

    Status authStatus;     // login screen
    Status projectsStatus; // project grid
    Status sessionStatus;  // dashboard

    [[nodiscard]] bool isSignedIn() const noexcept { return currentUser.has_value(); }

    // A project and one of its workspaces are open.
    [[nodiscard]] bool hasOpenProject() const noexcept { return selectedProject.has_value() && selectedBranchId.isNotEmpty(); }

    // The open workspace, or null.
    [[nodiscard]] const Branch* selectedBranch() const
    {
        const auto it = std::find_if(branches.begin(), branches.end(), [this](const Branch& branch)
        {
            return branch.id == selectedBranchId;
        });
        return it != branches.end() ? &*it : nullptr;
    }
};
