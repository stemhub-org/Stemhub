#include <algorithm>

#include "application/PluginProcessor.hpp"
#include "application/PluginProcessorHelpers.hpp"
#include "application/ProjectFileService.hpp"
#include "application/VersionControlService.hpp"
#include "application/SnapshotBundler.hpp"

using namespace stemhub::processorhelpers;

namespace
{
juce::File findPreviewWavSource(const juce::File& projectRootDirectory)
{
    if (!projectRootDirectory.isDirectory())
        return {};

    juce::Array<juce::File> wavFiles;
    projectRootDirectory.findChildFiles(wavFiles, juce::File::findFiles, true, "*.wav");

    if (wavFiles.isEmpty())
        return {};

    std::sort(wavFiles.begin(), wavFiles.end(), [](const juce::File& lhs, const juce::File& rhs)
    {
        return lhs.getFullPathName() < rhs.getFullPathName();
    });

    return wavFiles[0];
}

juce::File copyPreviewTrackToTemp(const juce::File& sourceFile)
{
    if (!sourceFile.existsAsFile())
        return {};

    const auto tempRoot = juce::File::getSpecialLocation(juce::File::tempDirectory)
                             .getChildFile("stemhub-previews");

    if ((!tempRoot.exists() && !tempRoot.createDirectory()) || !tempRoot.isDirectory())
        return {};

    const auto previewFile = tempRoot.getChildFile("preview_" + juce::Uuid().toString() + ".wav");
    if (previewFile.existsAsFile())
        previewFile.deleteFile();

    if (!sourceFile.copyFileTo(previewFile))
        return {};

    return previewFile;
}

}

StemhubAudioProcessor::StemhubAudioProcessor(std::unique_ptr<IProjectApi> apiClientProvider)
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor(BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput("Input", juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       )
#endif
{
    apiClient = std::move(apiClientProvider);
    if (apiClient == nullptr)
        apiClient = std::make_unique<ApiClient>();

    versionControlService.setApiClient(*apiClient);
}

StemhubAudioProcessor::StemhubAudioProcessor()
    : StemhubAudioProcessor(std::make_unique<ApiClient>())
{
}

StemhubAudioProcessor::~StemhubAudioProcessor()
{
    cancelPendingUpdate();
}

void StemhubAudioProcessor::enqueueBackgroundTask(std::function<BackgroundJobPayload()> taskFactory)
{
    backgroundJobs.enqueue(std::move(taskFactory), [this]()
    {
        triggerAsyncUpdate();
    });
}

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
    const juce::String& accessToken) const
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

    const auto versionsResult = versionControlService.fetchVersionHistory(selectedBranch.id, accessToken);
    if (versionsResult.ok() && versionsResult.value.has_value())
    {
        result.versions = std::move(*versionsResult.value);
        sortVersionHistoryNewestFirst(result.versions);
        result.selectedVersionId = chooseSelectedVersionId(result.versions, {});

        juce::File autoRestoredFile;
        const auto autoRestoreMessage = stemhub::projectfiles::tryRestoreLatestVersionToCache(
            result.versions,
            projectIt->id,
            selectedBranch.id,
            versionControlService,
            autoRestoredFile);
        if (autoRestoredFile.existsAsFile())
            result.projectFile = autoRestoredFile;
        else if (autoRestoreMessage.isNotEmpty())
            result.activeProjectStatusMessage = autoRestoreMessage;
    }

    const juce::String projectReadyMessage = localProjectFile.existsAsFile()
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
        if (result.projectFile.existsAsFile())
        {
            result.activeProjectStatusMessage = "Project ready. Latest version restored locally: "
                + result.projectFile.getFileName();
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
    result.selectedVersionId = chooseSelectedVersionId(result.versions, preferredVersionId);

    const auto projectId = selectedProject ? selectedProject->id : juce::String();
    if (projectId.isNotEmpty())
    {
        juce::File autoRestoredFile;
        const auto autoRestoreMessage = stemhub::projectfiles::tryRestoreLatestVersionToCache(
            result.versions,
            projectId,
            branchId,
            versionControlService,
            autoRestoredFile);
        if (autoRestoredFile.existsAsFile())
            result.projectFile = autoRestoredFile;
        else if (autoRestoreMessage.isNotEmpty())
            result.activeProjectStatusMessage = autoRestoreMessage;
    }

    if (result.versions.empty())
    {
        result.activeProjectStatusMessage = "Loaded branch \"" + branchName + "\". No versions yet.";
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

StemhubAudioProcessor::PushVersionJobResult StemhubAudioProcessor::performPushVersionRequest(
    const juce::File& projectFile,
    const juce::File& projectRootDirectory,
    const std::optional<Project>& project,
    const juce::String& branchId,
    const juce::String& commitMessage,
    const juce::String& dawName)
{
    PushVersionJobResult result;

    if (!hasProjectAndBranchSelected(project, branchId))
    {
        result.errorMessage = "Choose or create a project before saving.";
        return result;
    }

    if (!projectFile.existsAsFile())
    {
        result.errorMessage = "Choose a project file before saving.";
        return result;
    }

    if (!projectRootDirectory.isDirectory())
    {
        result.errorMessage = "Choose a valid project file before saving.";
        return result;
    }

    ProjectVersionContext context;
    context.projectId = project->id;
    context.branchId = branchId;
    context.lastVersionId = versionControlService.getLastVersionId();
    versionControlService.setCurrentProjectContext(context);

    PushVersionRequest request;
    request.branchId = branchId;
    request.commitMessage = commitMessage;
    request.dawName = dawName;

    const auto sourcePreview = findPreviewWavSource(projectRootDirectory);
    juce::File previewTrackFile = copyPreviewTrackToTemp(sourcePreview);

    SnapshotBundleRequest bundleRequest;
    bundleRequest.sourceProjectFile = projectFile;
    bundleRequest.sourceDaw = dawName;
    bundleRequest.projectRootDirectory = projectRootDirectory;
    bundleRequest.previewTrackFile = previewTrackFile;

    SnapshotBundler bundle;
    SnapshotBundleResult bundleOutput;

    const auto bundleStatus = bundle.bundleProject(bundleRequest, bundleOutput);
    if (bundleStatus.failed())
    {
        result.errorMessage = bundleStatus.getErrorMessage();
        if (previewTrackFile.existsAsFile())
            previewTrackFile.deleteFile();
        return result;
    }

    request.localProjectFile = bundleOutput.bundleFile;
    request.sourceProjectFilename = projectFile.getFileName();
    request.snapshotManifest = bundleOutput.manifest;

    const auto pushResult = versionControlService.pushVersion(request);
    if (pushResult.failed())
    {
        result.errorMessage = pushResult.getErrorMessage();
        if (bundleOutput.bundleFile.existsAsFile())
            bundleOutput.bundleFile.deleteFile();
        if (previewTrackFile.existsAsFile())
            previewTrackFile.deleteFile();
        return result;
    }

    const auto pushedVersionId = versionControlService.getLastVersionId();

    if (bundleOutput.bundleFile.existsAsFile())
        bundleOutput.bundleFile.deleteFile();
    if (previewTrackFile.existsAsFile())
        previewTrackFile.deleteFile();

    result.pushedVersionId = pushedVersionId;
    result.activeProjectStatusMessage = "Version pushed successfully.";
    return result;
}

StemhubAudioProcessor::RestoreVersionJobResult StemhubAudioProcessor::performRestoreVersionRequest(
    const juce::String& versionId,
    const juce::File& destinationFile) const
{
    RestoreVersionJobResult result;

    if (versionId.isEmpty())
    {
        result.errorMessage = "Select a version before restoring.";
        return result;
    }

    if (destinationFile.isDirectory())
    {
        result.errorMessage = "Select a valid destination file for restore.";
        return result;
    }

    const auto restoreResult = versionControlService.restoreVersion(
        versionId,
        destinationFile,
        versionControlService.getAccessToken());
    if (restoreResult.failed())
    {
        result.errorMessage = restoreResult.getErrorMessage();
        return result;
    }

    const auto restoreDirectory = destinationFile.getParentDirectory().getChildFile(
        destinationFile.getFileNameWithoutExtension());
    if (restoreDirectory.exists())
    {
        if (!restoreDirectory.deleteRecursively())
        {
            result.errorMessage = "Failed to clear previous restore folder at " + restoreDirectory.getFullPathName();
            return result;
        }
    }

    juce::File restoredProjectFile;
    const auto extractResult = stemhub::projectfiles::resolveRestoreResult(destinationFile, restoreDirectory, restoredProjectFile);
    if (extractResult.failed())
    {
        result.errorMessage = extractResult.getErrorMessage();
        return result;
    }

    result.restoredProjectFile = std::move(restoredProjectFile);
    result.activeProjectStatusMessage = "Version restored successfully: " + result.restoredProjectFile.getFileName();
    return result;
}
