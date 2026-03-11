#include <algorithm>

#include "application/PluginProcessor.hpp"
#include "application/PluginProcessorHelpers.hpp"
#include "application/ProjectFileService.hpp"

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
    const bool preferRemoteLatest) const
{
    ProjectActivationJobResult result;
    auto effectiveLocalProjectFile = localProjectFile;
    result.projectFile = effectiveLocalProjectFile;

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
                                                   : "No branches found for this project.";
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

    if (stemhub::projectfiles::isManagedRestoreCacheFile(effectiveLocalProjectFile))
    {
        juce::File localizedWorkingFile;
        const auto localizeResult = stemhub::projectfiles::materializeRestoredSnapshotAsWorkingCopy(
            effectiveLocalProjectFile,
            projectIt->id,
            selectedBranch.id,
            localizedWorkingFile);
        if (localizeResult.wasOk())
        {
            juce::Logger::writeToLog("[Restore] OpenProject -> localized managed restore file to working copy: "
                                     + localizedWorkingFile.getFullPathName());
            effectiveLocalProjectFile = localizedWorkingFile;
            result.projectFile = effectiveLocalProjectFile;
        }
        else
        {
            juce::Logger::writeToLog("[Restore] OpenProject -> failed to localize managed restore file: "
                                     + localizeResult.getErrorMessage());
        }
    }

    const auto versionsResult = versionControlService.fetchVersionHistory(selectedBranch.id, accessToken);
    bool didAutoRestoreLatest = false;
    if (versionsResult.ok() && versionsResult.value.has_value())
    {
        result.versions = std::move(*versionsResult.value);
        sortVersionHistoryNewestFirst(result.versions);
        const auto hintedVersionId = resolveVersionIdFromProjectPath(effectiveLocalProjectFile, result.versions);
        result.selectedVersionId = chooseSelectedVersionId(result.versions, hintedVersionId);
        juce::Logger::writeToLog("[Restore] OpenProject -> hintedVersionId="
                                 + hintedVersionId
                                 + ", selectedVersionId="
                                 + result.selectedVersionId);
        if (hintedVersionId.isNotEmpty())
            result.workingVersionId = hintedVersionId;

        const auto hasLocalProjectFile = effectiveLocalProjectFile.existsAsFile();
        const auto hasExplicitLocalVersionHint = hasVersionHintInProjectPath(effectiveLocalProjectFile);
        const auto isLocalWorkingCopyClean = hasLocalProjectFile
            ? hasCleanWorkingCopy(effectiveLocalProjectFile)
            : true;
        const auto shouldAutoRestoreLatest = preferRemoteLatest
            || !hasLocalProjectFile
            || (!hasExplicitLocalVersionHint && isLocalWorkingCopyClean);
        if (shouldAutoRestoreLatest)
        {
            juce::File autoRestoredFile;
            const auto autoRestoreMessage = stemhub::projectfiles::tryRestoreLatestVersionToCache(
                result.versions,
                projectIt->id,
                selectedBranch.id,
                versionControlService,
                autoRestoredFile);
            if (autoRestoredFile.existsAsFile())
            {
                juce::File localWorkingFile;
                const auto localizeResult = stemhub::projectfiles::materializeRestoredSnapshotAsWorkingCopy(
                    autoRestoredFile,
                    projectIt->id,
                    selectedBranch.id,
                    localWorkingFile);
                result.projectFile = localizeResult.wasOk() ? localWorkingFile : autoRestoredFile;
                didAutoRestoreLatest = true;
                result.workingVersionId = result.selectedVersionId;
                if (localizeResult.failed() && result.activeProjectStatusMessage.isEmpty())
                    result.activeProjectStatusMessage = "Latest version restored, but failed to prepare local working copy: "
                        + localizeResult.getErrorMessage();
            }
            else if (autoRestoreMessage.isNotEmpty())
            {
                result.activeProjectStatusMessage = autoRestoreMessage;
            }
        }
    }

    const juce::String projectReadyMessage = effectiveLocalProjectFile.existsAsFile()
        ? juce::String("Project ready.")
        : juce::String("Project ready. Choose a local project file before saving.");

    if (!versionsResult.ok())
    {
        result.activeProjectStatusMessage = versionsResult.error ? versionsResult.error->message
                                                                 : "Project ready, but failed to load version history.";
    }
    else if (result.versions.empty())
    {
        result.activeProjectStatusMessage = projectReadyMessage + " No versions yet.";
    }
    else
    {
        if (didAutoRestoreLatest && result.projectFile.existsAsFile())
        {
            result.activeProjectStatusMessage = "Project ready. Latest version restored locally: "
                + result.projectFile.getFileName();
        }
        else if (effectiveLocalProjectFile.existsAsFile())
        {
            result.activeProjectStatusMessage = "Project ready. Using local project file: "
                + effectiveLocalProjectFile.getFileName();
        }
        else if (result.activeProjectStatusMessage.isEmpty())
        {
            result.activeProjectStatusMessage = "Project ready. Loaded "
                + juce::String(static_cast<int>(result.versions.size()))
                + " version(s).";
        }
    }

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
                                                   : "Project created but no branch was returned.";
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
        result.activeProjectStatusMessage = "Project created and main branch selected. No versions yet.";
    }
    else
    {
        result.activeProjectStatusMessage = "Project created and main branch selected.";
    }

    return result;
}

StemhubAudioProcessor::BranchHistoryJobResult StemhubAudioProcessor::performFetchBranchHistoryRequest(
    const juce::String& branchId,
    const juce::String& branchName,
    const juce::String& preferredVersionId,
    const juce::String& accessToken) const
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
    const auto effectiveProjectFile = stemhub::projectfiles::resolveEffectiveProjectFile(selectedProjectFile, pendingProjectFile);
    const auto hintedVersionId = resolveVersionIdFromProjectPath(effectiveProjectFile, result.versions);
    result.selectedVersionId = chooseSelectedVersionId(result.versions, preferredVersionId.isNotEmpty()
                                                                       ? preferredVersionId
                                                                       : hintedVersionId);
    if (hintedVersionId.isNotEmpty())
        result.workingVersionId = hintedVersionId;

    const auto projectId = selectedProject ? selectedProject->id : juce::String();
    const auto isWorkingCopyClean = !effectiveProjectFile.existsAsFile()
        ? true
        : hasCleanWorkingCopy(effectiveProjectFile);
    const auto hasExplicitVersionHint = hasVersionHintInProjectPath(effectiveProjectFile);
    const auto hasPreferredVersion = preferredVersionId.isNotEmpty();
    juce::Logger::writeToLog("[Restore] BranchHistory -> hintedVersionId="
                             + hintedVersionId
                             + ", selectedVersionId="
                             + result.selectedVersionId
                             + ", preferredVersionId="
                             + preferredVersionId
                             + ", workingVersionId="
                             + result.workingVersionId);
    const auto shouldAutoRestoreLatest = !hasExplicitVersionHint
        && isWorkingCopyClean
        && !hasPreferredVersion;
    juce::Logger::writeToLog("[Restore] BranchHistory -> isWorkingCopyClean="
                             + juce::String(isWorkingCopyClean ? "true" : "false")
                             + ", hasExplicitVersionHint="
                             + juce::String(hasExplicitVersionHint ? "true" : "false")
                             + ", hasPreferredVersion="
                             + juce::String(hasPreferredVersion ? "true" : "false")
                             + ", shouldAutoRestoreLatest="
                             + juce::String(shouldAutoRestoreLatest ? "true" : "false"));
    if (projectId.isNotEmpty() && shouldAutoRestoreLatest)
    {
        juce::File autoRestoredFile;
        const auto autoRestoreMessage = stemhub::projectfiles::tryRestoreLatestVersionToCache(
            result.versions,
            projectId,
            branchId,
            versionControlService,
            autoRestoredFile);
        if (autoRestoredFile.existsAsFile())
        {
            juce::File localWorkingFile;
            const auto localizeResult = stemhub::projectfiles::materializeRestoredSnapshotAsWorkingCopy(
                autoRestoredFile,
                projectId,
                branchId,
                localWorkingFile);
            result.projectFile = localizeResult.wasOk() ? localWorkingFile : autoRestoredFile;
            result.workingVersionId = result.selectedVersionId;
            if (localizeResult.failed() && result.activeProjectStatusMessage.isEmpty())
                result.activeProjectStatusMessage = "Latest version restored, but failed to prepare local working copy: "
                    + localizeResult.getErrorMessage();
        }
        else if (autoRestoreMessage.isNotEmpty())
            result.activeProjectStatusMessage = autoRestoreMessage;
    }

    if (result.versions.empty())
    {
        result.activeProjectStatusMessage = "Loaded branch \"" + branchName + "\". No versions yet.";
    }
    else if (!isWorkingCopyClean && effectiveProjectFile.existsAsFile())
    {
        result.activeProjectStatusMessage = "Branch \"" + branchName + "\" has updates. Pull did not overwrite your local file. "
            "Use Restore to checkout the latest snapshot.";
    }
    else if (result.projectFile.existsAsFile())
    {
        result.activeProjectStatusMessage = "Loaded latest version for branch \"" + branchName + "\".";
    }
    else if (result.activeProjectStatusMessage.isEmpty())
    {
        result.activeProjectStatusMessage = "Loaded "
            + juce::String(static_cast<int>(result.versions.size()))
            + " version(s) for branch \"" + branchName + "\".";
    }

    return result;
}
