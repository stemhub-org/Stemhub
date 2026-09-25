#include <algorithm>

#include "application/UseCases.hpp"
#include "application/SessionHelpers.hpp"
#include "application/ProjectFileService.hpp"
#include "application/SnapshotBundler.hpp"
#include "application/SnapshotFiles.hpp"
#include "application/SnapshotSync.hpp"

using namespace stemhub::sessionhelpers;

namespace stemhub::usecases
{
namespace
{
void loadProjects(const IProjectApi& api, const juce::String& token, AuthRequestResult& result)
{
    auto projectsResult = api.fetchProjects(token);
    if (projectsResult.ok())
    {
        result.projects = std::move(*projectsResult.value);
        result.projectsStatus = result.projects.empty()
            ? Status::warning("No projects found.")
            : Status::info("Loaded " + juce::String(static_cast<int>(result.projects.size())) + " project(s).");
    }
    else
    {
        result.projectsStatus = Status::error(projectsResult.errorMessage("Failed to load projects."));
    }
}

// Records a failed call on a job result: its message, and whether the session is over.
template <typename Result, typename T>
Result& failWith(Result& result, const ApiResult<T>& call, const juce::String& fallback)
{
    result.errorMessage = call.errorMessage(fallback);
    result.sessionExpired = call.isUnauthorized();
    return result;
}

// The preferred branch when it still exists, else "main" when the project has one, else its
// first branch.
const Branch& chooseBranch(const std::vector<Branch>& branches, const juce::String& preferredBranchId)
{
    const auto preferredIt = std::find_if(branches.begin(), branches.end(), [&preferredBranchId](const Branch& branch)
    {
        return preferredBranchId.isNotEmpty() && branch.id == preferredBranchId;
    });
    if (preferredIt != branches.end())
        return *preferredIt;

    const auto mainIt = std::find_if(branches.begin(), branches.end(), [](const Branch& branch)
    {
        return branch.name == "main";
    });
    return mainIt != branches.end() ? *mainIt : branches.front();
}

}

AuthRequestResult signIn(const IProjectApi& api, const SignInInput& input)
{
    AuthRequestResult result;

    auto loginResult = api.login(input.email, input.password);
    if (!loginResult.ok())
    {
        result.authErrorMessage = loginResult.errorMessage("Failed to sign in.");
        return result;
    }

    const auto token = loginResult.value->accessToken;
    auto userResult = api.fetchCurrentUser(token);
    if (!userResult.ok())
    {
        result.authErrorMessage = userResult.errorMessage("Failed to load your user profile.");
        return result;
    }

    result.token = token;
    result.user = std::move(userResult.value);
    loadProjects(api, token, result);
    return result;
}

AuthRequestResult restoreSession(const IProjectApi& api, const RestoreSessionInput& input)
{
    AuthRequestResult result;
    result.fromSavedSession = true;
    result.token = input.token;

    auto userResult = api.fetchCurrentUser(input.token);
    if (!userResult.ok())
    {
        // Only a refused token ends the saved session. Offline, or with the server down, it is
        // kept so the next attempt can use it.
        result.sessionExpired = userResult.isUnauthorized() || userResult.error->kind == ApiError::Kind::forbidden;
        result.authErrorMessage = result.sessionExpired ? juce::String("Saved session expired. Please sign in again.")
                                                        : userResult.errorMessage("Couldn't restore your session.");
        return result;
    }

    result.user = std::move(userResult.value);
    loadProjects(api, input.token, result);
    return result;
}

ProjectActivationJobResult openProject(const IProjectApi& api, const OpenProjectInput& input)
{
    ProjectActivationJobResult result;
    result.projectFile = input.localProjectFile;

    if (input.projectId.isEmpty())
    {
        result.errorMessage = "Choose a project before continuing.";
        return result;
    }

    const auto projectIt = std::find_if(input.availableProjects.begin(),
                                        input.availableProjects.end(),
                                        [&input](const Project& project) { return project.id == input.projectId; });
    if (projectIt == input.availableProjects.end())
    {
        result.errorMessage = "The selected project is no longer available.";
        return result;
    }

    const auto branchesResult = api.fetchBranches(input.projectId, input.token);
    if (!branchesResult.ok())
        return failWith(result, branchesResult, "Failed to load workspaces.");
    if (branchesResult.value->empty())
    {
        result.errorMessage = "No workspaces found for this project.";
        return result;
    }

    result.branches = *branchesResult.value;
    const auto selectedBranch = chooseBranch(result.branches, input.preferredBranchId);
    result.selectedProject = *projectIt;
    result.branchId = selectedBranch.id;
    result.branchName = selectedBranch.name;

    const auto versionsResult = api.fetchVersions(selectedBranch.id, input.token);
    if (!versionsResult.ok())
    {
        result.sessionExpired = versionsResult.isUnauthorized();
        result.status = Status::warning(versionsResult.errorMessage("Project ready, but failed to load version history."));
        return result;
    }

    result.versions = *versionsResult.value;
    sortVersionHistoryNewestFirst(result.versions);

    const auto& localFile = input.localProjectFile;
    const auto& localCopy = input.localCopy;
    const auto hintedVersionId = resolveVersionIdFromProjectPath(localFile, result.versions);
    result.selectedVersionId = chooseSelectedVersionId(result.versions, hintedVersionId);

    // What the local file holds: what this instance recorded, else what its folder name says.
    if (localCopy.describes(localFile))
        result.workingCopy = localCopy;
    else if (hintedVersionId.isNotEmpty())
        result.workingCopy = { localFile, hintedVersionId };

    const auto hasLocalCopy = localFile.existsAsFile();
    const auto usingLocalFileMessage = hasLocalCopy
        ? "Project ready. Using local project file: " + localFile.getFileName()
        : juce::String("Project ready. Choose a local project file before saving.");

    if (result.versions.empty())
    {
        result.status = Status::success((hasLocalCopy ? usingLocalFileMessage : juce::String("Project ready."))
                                        + " No versions yet.");
        return result;
    }

    if (!input.restoreLatestIfSafe)
    {
        result.status = Status::success(hasLocalCopy ? usingLocalFileMessage
                                                     : "Project ready. Loaded "
                                                           + juce::String(static_cast<int>(result.versions.size()))
                                                           + " version(s).");
        return result;
    }

    const auto& latestVersion = result.versions.front();
    if (hasLocalCopy)
    {
        // Nothing certain is known about this file (the user linked it, or it was restored in
        // another session), so it stays as it is.
        if (!localCopy.describes(localFile) || !localCopy.hasRecordedState())
        {
            result.status = Status::success(usingLocalFileMessage);
            return result;
        }

        if (!localCopy.isUnchanged())
        {
            result.status = Status::warning("Local changes are not saved to StemHub yet, so the latest version "
                                            "was not restored. Save them, or restore a version into a new folder.");
            return result;
        }

        if (localCopy.versionId == latestVersion.id)
        {
            result.status = Status::success("Project ready. Your local copy is the latest version.");
            return result;
        }
    }

    // No local copy, or an unchanged copy of an older version: bring in the latest one. It goes
    // to a new folder, so nothing on disk is replaced, and opens as a DAW project of its own.
    const auto restoreFolder = stemhub::projectfiles::chooseRestoreFolder(
        stemhub::projectfiles::getManagedWorkingCopyRoot(input.managedWorkingCopyFolder, projectIt->id, selectedBranch.id),
        stemhub::projectfiles::resolveRestoreProjectName(result.versions, latestVersion.id, projectIt->name),
        latestVersion.id);

    const auto restored = stemhub::snapshots::restoreSnapshot(api, input.token, { projectIt->id, latestVersion.id, restoreFolder });
    if (!restored.ok())
    {
        result.sessionExpired = restored.isUnauthorized();
        result.status = Status::warning("Project ready, but the latest version couldn't be restored: "
                                        + restored.errorMessage("unknown error"));
        return result;
    }

    const auto& restoredProjectFile = *restored.value;
    result.restoredCopy = WorkingCopyBaseline::recordedNow(restoredProjectFile, latestVersion.id);
    result.selectedVersionId = latestVersion.id;
    result.status = Status::success("Project ready. Latest version restored to "
                                    + restoredProjectFile.getParentDirectory().getFullPathName() + ".");
    return result;
}

ProjectActivationJobResult createProject(const IProjectApi& api, const CreateProjectInput& input)
{
    ProjectActivationJobResult result;
    result.projectFile = input.localProjectFile;
    result.refreshProjects = true;

    const auto projectName = input.localProjectFile.existsAsFile()
        ? input.localProjectFile.getFileNameWithoutExtension()
        : juce::String();

    if (projectName.isEmpty())
    {
        result.errorMessage = "Choose a project file first.";
        return result;
    }

    const auto createdProject = api.createProject(projectName, input.token);
    if (!createdProject.ok())
        return failWith(result, createdProject, "Failed to create project.");

    auto projectsResult = api.fetchProjects(input.token);
    if (projectsResult.ok())
        result.projects = std::move(*projectsResult.value);

    const auto branchesResult = api.fetchBranches(createdProject.value->id, input.token);
    if (!branchesResult.ok())
        return failWith(result, branchesResult, "Project created, but its workspaces couldn't be loaded.");
    if (branchesResult.value->empty())
    {
        result.errorMessage = "Project created but no workspace was returned.";
        return result;
    }

    result.branches = *branchesResult.value;
    const auto selectedBranch = chooseBranch(result.branches, {});
    result.selectedProject = *createdProject.value;
    result.branchId = selectedBranch.id;
    result.branchName = selectedBranch.name;

    const auto versionsResult = api.fetchVersions(selectedBranch.id, input.token);
    if (versionsResult.ok())
    {
        result.versions = *versionsResult.value;
        sortVersionHistoryNewestFirst(result.versions);
        result.selectedVersionId = chooseSelectedVersionId(result.versions, {});
    }

    if (!versionsResult.ok())
    {
        result.sessionExpired = versionsResult.isUnauthorized();
        result.status = Status::warning(versionsResult.errorMessage("Project created, but failed to load version history."));
    }
    else if (result.versions.empty())
    {
        result.status = Status::success("Project created and main workspace selected. No versions yet.");
    }
    else
    {
        result.status = Status::success("Project created and main workspace selected.");
    }

    return result;
}

BranchHistoryJobResult fetchHistory(const IProjectApi& api, const FetchHistoryInput& input)
{
    BranchHistoryJobResult result;
    result.branchId = input.branchId;
    result.branchName = input.branchName;

    const auto versionsResult = api.fetchVersions(input.branchId, input.token);
    if (!versionsResult.ok())
        return failWith(result, versionsResult, "Failed to load version history.");

    result.versions = *versionsResult.value;
    sortVersionHistoryNewestFirst(result.versions);
    const auto hintedVersionId = resolveVersionIdFromProjectPath(input.localProjectFile, result.versions);
    result.selectedVersionId = chooseSelectedVersionId(result.versions, input.preferredVersionId.isNotEmpty()
                                                                            ? input.preferredVersionId
                                                                            : hintedVersionId);

    result.status = Status::success(result.versions.empty()
                                        ? "Loaded workspace \"" + input.branchName + "\". No versions yet."
                                        : "Loaded " + juce::String(static_cast<int>(result.versions.size()))
                                              + " version(s) for workspace \"" + input.branchName + "\".");
    return result;
}

PushVersionJobResult pushVersion(const IProjectApi& api, const PushInput& input)
{
    PushVersionJobResult result;

    if (input.projectId.isEmpty() || input.branchId.isEmpty())
    {
        result.errorMessage = "Choose or create a project before saving.";
        return result;
    }
    if (!input.projectFile.existsAsFile())
    {
        result.errorMessage = "Choose a project file before saving.";
        return result;
    }
    if (input.commitMessage.trim().length() > kMaxSaveNoteLength)
    {
        result.errorMessage = "Save notes are limited to " + juce::String(kMaxSaveNoteLength) + " characters.";
        return result;
    }

    // Taken before hashing: if the DAW saves again meanwhile, the next save sees a change.
    const auto sizeBeforeHashing = input.projectFile.getSize();
    const auto modTimeBeforeHashing = input.projectFile.getLastModificationTime().toMilliseconds();

    SnapshotBundleRequest bundleRequest;
    bundleRequest.sourceProjectFile = input.projectFile;
    bundleRequest.sourceDaw = stemhub::snapshotfiles::dawNameFor(input.projectFile);

    ContentAddressedManifest manifest;
    const auto manifestStatus = SnapshotBundler().buildManifest(bundleRequest, manifest);
    if (manifestStatus.failed())
    {
        result.errorMessage = manifestStatus.getErrorMessage();
        return result;
    }

    stemhub::snapshots::PushRequest pushRequest;
    pushRequest.projectId = input.projectId;
    pushRequest.branchId = input.branchId;
    pushRequest.commitMessage = input.commitMessage.trim().isNotEmpty() ? input.commitMessage.trim()
                                                                       : juce::String(kDefaultSaveNote);
    pushRequest.parentVersionId = input.parentVersionId;
    pushRequest.manifest = std::move(manifest);

    const auto pushed = stemhub::snapshots::pushSnapshot(api, input.token, pushRequest);
    if (!pushed.ok())
        return failWith(result, pushed, "Failed to save the version.");

    result.pushedVersionId = pushed.value->id;
    result.pushedCopy = { input.projectFile, result.pushedVersionId, sizeBeforeHashing, modTimeBeforeHashing };

    auto versionsResult = api.fetchVersions(input.branchId, input.token);
    if (versionsResult.ok())
    {
        sortVersionHistoryNewestFirst(*versionsResult.value);
        result.refreshedVersions = std::move(*versionsResult.value);
        result.status = Status::success("Version saved successfully.");
    }
    else
    {
        result.status = Status::warning("Version saved. Sync to see it in the history ("
                                        + versionsResult.errorMessage("history unavailable") + ").");
    }

    return result;
}

RestoreVersionJobResult restoreVersion(const IProjectApi& api, const RestoreInput& input)
{
    RestoreVersionJobResult result;
    result.restoredVersionId = input.versionId;

    if (input.versionId.isEmpty())
    {
        result.errorMessage = "Select a version before restoring.";
        return result;
    }
    if (input.projectId.isEmpty())
    {
        result.errorMessage = "Choose a project before restoring.";
        return result;
    }

    const auto restored = stemhub::snapshots::restoreSnapshot(api, input.token, { input.projectId, input.versionId, input.destinationFolder });
    if (!restored.ok())
        return failWith(result, restored, "Failed to restore the version.");

    const auto& restoredProjectFile = *restored.value;
    result.restoredProjectFile = restoredProjectFile;
    result.restoredCopy = WorkingCopyBaseline::recordedNow(restoredProjectFile, input.versionId);
    result.status = Status::success("Version restored to " + restoredProjectFile.getParentDirectory().getFullPathName() + ".");
    return result;
}
}
