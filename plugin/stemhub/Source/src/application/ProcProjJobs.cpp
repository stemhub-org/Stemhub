#include <algorithm>

#include "application/PluginProcessor.hpp"
#include "application/PluginProcessorHelpers.hpp"
#include "application/ProjectFileService.hpp"
#include "application/SnapshotBundler.hpp"

using namespace stemhub::processorhelpers;

StemhubAudioProcessor::AuthRequestResult StemhubAudioProcessor::performSignInRequest(
    const juce::String& email,
    const juce::String& password) const
{
    AuthRequestResult result;

    auto loginResult = apiClient->login(email, password);
    if (!loginResult.ok())
    {
        result.authErrorMessage = loginResult.error ? loginResult.error->message
                                                    : "Failed to sign in.";
        return result;
    }

    const auto token = loginResult.value->accessToken;
    auto userResult = apiClient->fetchCurrentUser(token);
    if (!userResult.ok() || !userResult.value->isValid())
    {
        result.authErrorMessage = userResult.error ? userResult.error->message
                                                   : "Failed to load your user profile.";
        return result;
    }

    result.token = token;
    result.user = std::move(userResult.value);

    auto projectsResult = apiClient->fetchProjects(token);
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

    return result;
}

StemhubAudioProcessor::AuthRequestResult StemhubAudioProcessor::performRestoreCachedSessionRequest(
    const juce::String& token) const
{
    AuthRequestResult result;
    result.fromCachedSession = true;
    result.token = token;

    const auto userResult = apiClient->fetchCurrentUser(token);
    if (!userResult.ok() || !userResult.value->isValid())
    {
        result.authErrorMessage = "Cached session is no longer valid.";
        return result;
    }

    result.user = std::move(userResult.value);

    auto projectsResult = apiClient->fetchProjects(token);
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

    return result;
}

StemhubAudioProcessor::ProjectActivationJobResult StemhubAudioProcessor::performOpenProjectRequest(
    const juce::String& projectId,
    const juce::File& localProjectFile,
    const std::vector<Project>& availableProjects,
    const juce::String& accessToken,
    const bool restoreLatestIfSafe,
    const LocalCopyState& localCopy,
    const juce::File& managedWorkingCopyBase) const
{
    ProjectActivationJobResult result;
    result.projectFile = localProjectFile;

    if (projectId.isEmpty())
    {
        result.errorMessage = "Choose a project before continuing.";
        return result;
    }

    const auto projectIt = std::find_if(availableProjects.begin(), availableProjects.end(), [&projectId](const Project& project)
    {
        return project.id == projectId;
    });

    if (projectIt == availableProjects.end())
    {
        result.errorMessage = "The selected project is no longer available.";
        return result;
    }

    const auto branchesResult = apiClient->fetchBranches(projectId, accessToken);
    if (!branchesResult.ok() || !branchesResult.value.has_value() || branchesResult.value->empty())
    {
        result.errorMessage = branchesResult.error ? branchesResult.error->message
                                                   : "No workspaces found for this project.";
        return result;
    }

    const auto& fetchedBranches = *branchesResult.value;
    result.branches = fetchedBranches;
    const auto branchIt = std::find_if(fetchedBranches.begin(), fetchedBranches.end(), [](const Branch& branch)
    {
        return branch.name == "main";
    });
    const auto& selectedBranch = branchIt != fetchedBranches.end() ? *branchIt : fetchedBranches.front();

    result.selectedProject = *projectIt;
    result.branchId = selectedBranch.id;
    result.branchName = selectedBranch.name;

    VersionControlService versionService(*apiClient);
    versionService.setAccessToken(accessToken);

    const auto versionsResult = versionService.fetchVersionHistory(selectedBranch.id, accessToken);
    if (!versionsResult.ok() || !versionsResult.value.has_value())
    {
        result.activeProjectStatusMessage = versionsResult.error ? versionsResult.error->message
                                                                 : "Project ready, but failed to load version history.";
        return result;
    }

    result.versions = *versionsResult.value;
    sortVersionHistoryNewestFirst(result.versions);
    const auto hintedVersionId = resolveVersionIdFromProjectPath(localProjectFile, result.versions);
    result.selectedVersionId = chooseSelectedVersionId(result.versions, hintedVersionId);
    if (hintedVersionId.isNotEmpty())
        result.workingVersionId = hintedVersionId;

    const auto hasLocalCopy = localProjectFile.existsAsFile();
    const auto usingLocalFileMessage = hasLocalCopy
        ? "Project ready. Using local project file: " + localProjectFile.getFileName()
        : juce::String("Project ready. Choose a local project file before saving.");

    if (result.versions.empty())
    {
        result.activeProjectStatusMessage = (hasLocalCopy ? usingLocalFileMessage : juce::String("Project ready."))
            + " No versions yet.";
        return result;
    }

    if (!restoreLatestIfSafe)
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
        // A file the user linked themselves: nothing says which version it holds, so it stays.
        if (localCopy.baseVersionId.isEmpty())
        {
            result.activeProjectStatusMessage = usingLocalFileMessage;
            return result;
        }

        if (!localCopy.isUnchanged)
        {
            result.activeProjectStatusMessage = "Local changes are not saved to StemHub yet, so the latest version "
                                                "was not restored. Save them, or restore a version into a new folder.";
            return result;
        }

        if (localCopy.baseVersionId == latestVersion.id)
        {
            result.activeProjectStatusMessage = "Project ready. Your local copy is the latest version.";
            return result;
        }
    }

    // No local copy, or an unchanged copy of an older version: bring in the latest one. It goes
    // to a new folder, so nothing on disk is replaced.
    const auto restoreFolder = stemhub::projectfiles::chooseRestoreFolder(
        stemhub::projectfiles::getManagedWorkingCopyRoot(managedWorkingCopyBase, projectIt->id, selectedBranch.id),
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
    result.workingVersionId = latestVersion.id;
    result.selectedVersionId = latestVersion.id;
    result.didRestoreLatest = true;
    result.restoredFileSizeBytes = restoredProjectFile.getSize();
    result.restoredFileModTimeMs = restoredProjectFile.getLastModificationTime().toMilliseconds();
    result.activeProjectStatusMessage = "Project ready. Latest version restored: " + restoredProjectFile.getFileName();
    return result;
}

StemhubAudioProcessor::ProjectActivationJobResult StemhubAudioProcessor::performCreateProjectRequest(
    const juce::File& localProjectFile,
    const juce::String& accessToken) const
{
    ProjectActivationJobResult result;
    result.projectFile = localProjectFile;
    result.refreshProjects = true;

    const auto projectName = localProjectFile.existsAsFile()
        ? localProjectFile.getFileNameWithoutExtension()
        : juce::String();

    if (projectName.isEmpty())
    {
        result.errorMessage = "Choose a project file first.";
        return result;
    }

    const auto createdProject = apiClient->createProject(projectName, accessToken);
    if (!createdProject.ok() || !createdProject.value.has_value())
    {
        result.errorMessage = createdProject.error ? createdProject.error->message
                                                   : "Failed to create project.";
        return result;
    }

    auto projectsResult = apiClient->fetchProjects(accessToken);
    if (projectsResult.ok() && projectsResult.value.has_value())
        result.projects = std::move(*projectsResult.value);

    const auto branchesResult = apiClient->fetchBranches(createdProject.value->id, accessToken);
    if (!branchesResult.ok() || !branchesResult.value.has_value() || branchesResult.value->empty())
    {
        result.errorMessage = branchesResult.error ? branchesResult.error->message
                                                   : "Project created but no workspace was returned.";
        return result;
    }

    const auto& fetchedBranches = *branchesResult.value;
    result.branches = fetchedBranches;
    const auto branchIt = std::find_if(fetchedBranches.begin(), fetchedBranches.end(), [](const Branch& branch)
    {
        return branch.name == "main";
    });
    const auto& selectedBranch = branchIt != fetchedBranches.end() ? *branchIt : fetchedBranches.front();

    result.selectedProject = *createdProject.value;
    result.branchId = selectedBranch.id;
    result.branchName = selectedBranch.name;

    const auto versionsResult = versionControlService.fetchVersionHistory(selectedBranch.id, accessToken);
    if (versionsResult.ok() && versionsResult.value.has_value())
    {
        result.versions = std::move(*versionsResult.value);
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

StemhubAudioProcessor::BranchHistoryJobResult StemhubAudioProcessor::performFetchBranchHistoryRequest(
    const juce::String& branchId,
    const juce::String& branchName,
    const juce::String& preferredVersionId,
    const juce::String& accessToken,
    const juce::File& localProjectFile) const
{
    BranchHistoryJobResult result;
    result.branchId = branchId;
    result.branchName = branchName;

    const auto versionsResult = versionControlService.fetchVersionHistory(branchId, accessToken);
    if (!versionsResult.ok() || !versionsResult.value.has_value())
    {
        result.errorMessage = versionsResult.error ? versionsResult.error->message
                                                   : "Failed to load version history.";
        return result;
    }

    result.versions = std::move(*versionsResult.value);
    sortVersionHistoryNewestFirst(result.versions);
    const auto hintedVersionId = resolveVersionIdFromProjectPath(localProjectFile, result.versions);
    result.selectedVersionId = chooseSelectedVersionId(result.versions, preferredVersionId.isNotEmpty()
                                                                       ? preferredVersionId
                                                                       : hintedVersionId);
    if (hintedVersionId.isNotEmpty())
        result.workingVersionId = hintedVersionId;

    result.activeProjectStatusMessage = result.versions.empty()
        ? "Loaded workspace \"" + branchName + "\". No versions yet."
        : "Loaded " + juce::String(static_cast<int>(result.versions.size()))
              + " version(s) for workspace \"" + branchName + "\".";
    return result;
}
