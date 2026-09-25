#include <algorithm>

#include "application/UseCases.hpp"
#include "application/PluginProcessorHelpers.hpp"
#include "application/ProjectFileService.hpp"
#include "application/SnapshotBundler.hpp"
#include "application/VersionControlService.hpp"

using namespace stemhub::processorhelpers;

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
        result.projectSelectionStatusMessage = result.projects.empty()
            ? "No projects found."
            : "Loaded " + juce::String(static_cast<int>(result.projects.size())) + " project(s).";
    }
    else
    {
        result.projectSelectionStatusMessage = projectsResult.error ? projectsResult.error->message
                                                                    : "Failed to load projects.";
    }
}

// "main" when the project has one, otherwise its first branch.
const Branch& chooseDefaultBranch(const std::vector<Branch>& branches)
{
    const auto branchIt = std::find_if(branches.begin(), branches.end(), [](const Branch& branch)
    {
        return branch.name == "main";
    });
    return branchIt != branches.end() ? *branchIt : branches.front();
}

VersionControlService makeVersionService(const IProjectApi& api, const juce::String& token)
{
    VersionControlService service(api);
    service.setAccessToken(token);
    return service;
}
}

AuthRequestResult signIn(const IProjectApi& api, const SignInInput& input)
{
    AuthRequestResult result;

    auto loginResult = api.login(input.email, input.password);
    if (!loginResult.ok())
    {
        result.authErrorMessage = loginResult.error ? loginResult.error->message
                                                    : "Failed to sign in.";
        return result;
    }

    const auto token = loginResult.value->accessToken;
    auto userResult = api.fetchCurrentUser(token);
    if (!userResult.ok() || !userResult.value->isValid())
    {
        result.authErrorMessage = userResult.error ? userResult.error->message
                                                   : "Failed to load your user profile.";
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
    result.fromCachedSession = true;
    result.token = input.token;

    auto userResult = api.fetchCurrentUser(input.token);
    if (!userResult.ok() || !userResult.value->isValid())
    {
        result.authErrorMessage = "Cached session is no longer valid.";
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
    if (!branchesResult.ok() || !branchesResult.value.has_value() || branchesResult.value->empty())
    {
        result.errorMessage = branchesResult.error ? branchesResult.error->message
                                                   : "No workspaces found for this project.";
        return result;
    }

    result.branches = *branchesResult.value;
    const auto selectedBranch = chooseDefaultBranch(result.branches);
    result.selectedProject = *projectIt;
    result.branchId = selectedBranch.id;
    result.branchName = selectedBranch.name;

    auto versionService = makeVersionService(api, input.token);
    const auto versionsResult = versionService.fetchVersionHistory(selectedBranch.id, input.token);
    if (!versionsResult.ok() || !versionsResult.value.has_value())
    {
        result.activeProjectStatusMessage = versionsResult.error ? versionsResult.error->message
                                                                 : "Project ready, but failed to load version history.";
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
        result.activeProjectStatusMessage = (hasLocalCopy ? usingLocalFileMessage : juce::String("Project ready."))
            + " No versions yet.";
        return result;
    }

    if (!input.restoreLatestIfSafe)
    {
        result.activeProjectStatusMessage = hasLocalCopy ? usingLocalFileMessage
                                                         : "Project ready. Loaded "
                                                               + juce::String(static_cast<int>(result.versions.size()))
                                                               + " version(s).";
        return result;
    }

    const auto& latestVersion = result.versions.front();
    if (hasLocalCopy)
    {
        // Nothing certain is known about this file (the user linked it, or it was restored in
        // another session), so it stays as it is.
        if (!localCopy.describes(localFile) || !localCopy.hasRecordedState())
        {
            result.activeProjectStatusMessage = usingLocalFileMessage;
            return result;
        }

        if (!localCopy.isUnchanged())
        {
            result.activeProjectStatusMessage = "Local changes are not saved to StemHub yet, so the latest version "
                                                "was not restored. Save them, or restore a version into a new folder.";
            return result;
        }

        if (localCopy.versionId == latestVersion.id)
        {
            result.activeProjectStatusMessage = "Project ready. Your local copy is the latest version.";
            return result;
        }
    }

    // No local copy, or an unchanged copy of an older version: bring in the latest one. It goes
    // to a new folder, so nothing on disk is replaced.
    const auto restoreFolder = stemhub::projectfiles::chooseRestoreFolder(
        stemhub::projectfiles::getManagedWorkingCopyRoot(input.managedWorkingCopyFolder, projectIt->id, selectedBranch.id),
        stemhub::projectfiles::resolveRestoreProjectName(result.versions, latestVersion.id, projectIt->name),
        latestVersion.id);

    juce::File restoredProjectFile;
    const auto restoreStatus = versionService.restoreVersionFromManifest(projectIt->id,
                                                                         latestVersion.id,
                                                                         restoreFolder,
                                                                         restoredProjectFile);
    if (restoreStatus.failed())
    {
        result.activeProjectStatusMessage = "Project ready, but the latest version couldn't be restored: "
            + restoreStatus.getErrorMessage();
        return result;
    }

    result.projectFile = restoredProjectFile;
    result.workingCopy = WorkingCopyBaseline::recordedNow(restoredProjectFile, latestVersion.id);
    result.selectedVersionId = latestVersion.id;
    result.didRestoreLatest = true;
    result.activeProjectStatusMessage = "Project ready. Latest version restored: " + restoredProjectFile.getFileName();
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
    if (!createdProject.ok() || !createdProject.value.has_value())
    {
        result.errorMessage = createdProject.error ? createdProject.error->message
                                                   : "Failed to create project.";
        return result;
    }

    auto projectsResult = api.fetchProjects(input.token);
    if (projectsResult.ok() && projectsResult.value.has_value())
        result.projects = std::move(*projectsResult.value);

    const auto branchesResult = api.fetchBranches(createdProject.value->id, input.token);
    if (!branchesResult.ok() || !branchesResult.value.has_value() || branchesResult.value->empty())
    {
        result.errorMessage = branchesResult.error ? branchesResult.error->message
                                                   : "Project created but no workspace was returned.";
        return result;
    }

    result.branches = *branchesResult.value;
    const auto selectedBranch = chooseDefaultBranch(result.branches);
    result.selectedProject = *createdProject.value;
    result.branchId = selectedBranch.id;
    result.branchName = selectedBranch.name;

    const auto versionsResult = makeVersionService(api, input.token).fetchVersionHistory(selectedBranch.id, input.token);
    if (versionsResult.ok() && versionsResult.value.has_value())
    {
        result.versions = *versionsResult.value;
        sortVersionHistoryNewestFirst(result.versions);
        result.selectedVersionId = chooseSelectedVersionId(result.versions, {});
    }

    if (!versionsResult.ok())
    {
        result.activeProjectStatusMessage = versionsResult.error ? versionsResult.error->message
                                                                 : "Project created, but failed to load version history.";
    }
    else if (result.versions.empty())
    {
        result.activeProjectStatusMessage = "Project created and main workspace selected. No versions yet.";
    }
    else
    {
        result.activeProjectStatusMessage = "Project created and main workspace selected.";
    }

    return result;
}

BranchHistoryJobResult fetchHistory(const IProjectApi& api, const FetchHistoryInput& input)
{
    BranchHistoryJobResult result;
    result.branchId = input.branchId;
    result.branchName = input.branchName;

    const auto versionsResult = makeVersionService(api, input.token).fetchVersionHistory(input.branchId, input.token);
    if (!versionsResult.ok() || !versionsResult.value.has_value())
    {
        result.errorMessage = versionsResult.error ? versionsResult.error->message
                                                   : "Failed to load version history.";
        return result;
    }

    result.versions = *versionsResult.value;
    sortVersionHistoryNewestFirst(result.versions);
    const auto hintedVersionId = resolveVersionIdFromProjectPath(input.localProjectFile, result.versions);
    result.selectedVersionId = chooseSelectedVersionId(result.versions, input.preferredVersionId.isNotEmpty()
                                                                            ? input.preferredVersionId
                                                                            : hintedVersionId);

    result.activeProjectStatusMessage = result.versions.empty()
        ? "Loaded workspace \"" + input.branchName + "\". No versions yet."
        : "Loaded " + juce::String(static_cast<int>(result.versions.size()))
              + " version(s) for workspace \"" + input.branchName + "\".";
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

    // Taken before hashing: if the DAW saves again meanwhile, the next save sees a change.
    const auto sizeBeforeHashing = input.projectFile.getSize();
    const auto modTimeBeforeHashing = input.projectFile.getLastModificationTime().toMilliseconds();

    SnapshotBundleRequest bundleRequest;
    bundleRequest.sourceProjectFile = input.projectFile;
    bundleRequest.projectRootDirectory = input.projectFile.getParentDirectory();
    bundleRequest.sourceDaw = input.dawName;

    ContentAddressedManifest manifest;
    const auto manifestStatus = SnapshotBundler().buildManifest(bundleRequest, manifest);
    if (manifestStatus.failed())
    {
        result.errorMessage = manifestStatus.getErrorMessage();
        return result;
    }

    PushVersionRequest pushRequest;
    pushRequest.projectId = input.projectId;
    pushRequest.branchId = input.branchId;
    pushRequest.commitMessage = input.commitMessage;
    pushRequest.parentVersionId = input.parentVersionId;
    pushRequest.manifest = std::move(manifest);

    auto versionService = makeVersionService(api, input.token);
    const auto pushStatus = versionService.pushVersion(pushRequest);
    if (pushStatus.failed())
    {
        result.errorMessage = pushStatus.getErrorMessage();
        return result;
    }

    result.pushedVersionId = versionService.getLastVersionId();
    result.pushedCopy = { input.projectFile, result.pushedVersionId, sizeBeforeHashing, modTimeBeforeHashing };

    auto versionsResult = versionService.fetchVersionHistory(input.branchId, input.token);
    if (versionsResult.ok() && versionsResult.value.has_value())
    {
        sortVersionHistoryNewestFirst(*versionsResult.value);
        result.refreshedVersions = std::move(*versionsResult.value);
        result.activeProjectStatusMessage = "Version saved successfully.";
    }
    else
    {
        result.activeProjectStatusMessage = "Version saved. Sync to see it in the history ("
            + (versionsResult.error ? versionsResult.error->message : juce::String("history unavailable")) + ").";
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
    // The request picked a folder that did not exist. Anything there now is someone else's.
    if (input.destinationFolder.exists())
    {
        result.errorMessage = "The restore folder already exists: " + input.destinationFolder.getFullPathName();
        return result;
    }

    juce::File restoredProjectFile;
    const auto status = makeVersionService(api, input.token)
                            .restoreVersionFromManifest(input.projectId, input.versionId, input.destinationFolder, restoredProjectFile);
    if (status.failed())
    {
        result.errorMessage = status.getErrorMessage();
        return result;
    }

    result.restoredProjectFile = restoredProjectFile;
    result.restoredCopy = WorkingCopyBaseline::recordedNow(restoredProjectFile, input.versionId);
    result.activeProjectStatusMessage = "Version restored successfully: " + restoredProjectFile.getFileName();
    return result;
}
}
