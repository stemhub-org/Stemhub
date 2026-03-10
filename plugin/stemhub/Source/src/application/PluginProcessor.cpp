#include <algorithm>
#include <type_traits>

#include "application/PluginProcessor.hpp"
#include "application/SessionCache.hpp"
#include "ui/PluginEditor.hpp"
#include "application/VersionControlService.hpp"
#include "application/SnapshotBundler.hpp"

namespace
{
template <typename ResultType>
bool hasError(const ResultType& result)
{
    return !result.errorMessage.isEmpty();
}

void sortVersionHistoryNewestFirst(std::vector<VersionSummary>& versions)
{
    std::sort(versions.begin(), versions.end(), [](const VersionSummary& lhs, const VersionSummary& rhs)
    {
        return lhs.createdAt > rhs.createdAt;
    });
}

juce::String chooseSelectedVersionId(const std::vector<VersionSummary>& versions,
                                     const juce::String& preferredVersionId)
{
    if (versions.empty())
        return {};

    if (preferredVersionId.isNotEmpty())
    {
        const auto it = std::find_if(versions.begin(), versions.end(), [&preferredVersionId](const VersionSummary& version)
        {
            return version.id == preferredVersionId;
        });

        if (it != versions.end())
            return it->id;
    }

    return versions.front().id;
}

juce::String resolveRestoreProjectName(const std::vector<VersionSummary>& versions,
                                      const juce::String& versionId,
                                      const juce::String& fallbackName)
{
    const auto it = std::find_if(versions.begin(), versions.end(), [&versionId](const VersionSummary& version)
    {
        return version.id == versionId;
    });

    if (it != versions.end() && it->sourceProjectFilename.isNotEmpty())
    {
        const auto sourceProjectName = juce::File(it->sourceProjectFilename).getFileNameWithoutExtension();
        if (sourceProjectName.isNotEmpty())
            return sourceProjectName;
    }

    return fallbackName.isNotEmpty() ? fallbackName : "restored-project";
}

bool hasProjectAndBranchSelected(const std::optional<Project>& project, const juce::String& branchId)
{
    return project.has_value() && branchId.isNotEmpty();
}

ProjectVersionContext makeProjectVersionContext(const std::optional<Project>& project,
                                                const juce::String& branchId,
                                                const std::vector<VersionSummary>& versions)
{
    ProjectVersionContext context;
    context.projectId = project ? project->id : juce::String();
    context.branchId = branchId;
    context.lastVersionId = versions.empty() ? juce::String() : versions.front().id;
    return context;
}

juce::Result resolveRestoreResult(const juce::File& snapshotZipFile,
                                 const juce::File& restoreDirectory,
                                 juce::File& restoredProjectFile)
{
    if (!snapshotZipFile.existsAsFile())
        return juce::Result::fail("Downloaded snapshot file is missing.");

    if (restoreDirectory.existsAsFile())
        return juce::Result::fail("Restore path must be a folder, but a file already exists there.");

    if (!restoreDirectory.createDirectory())
        return juce::Result::fail("Failed to create folder to restore snapshot.");

    juce::ZipFile snapshotZip(snapshotZipFile);
    const auto uncompressResult = snapshotZip.uncompressTo(restoreDirectory);
    if (uncompressResult.failed())
        return uncompressResult;

    const auto manifestFile = restoreDirectory.getChildFile("manifest.json");
    if (manifestFile.existsAsFile())
    {
        const auto manifestText = manifestFile.loadFileAsString();
        const auto manifestObject = juce::JSON::parse(manifestText).getDynamicObject();
        if (manifestObject == nullptr)
            return juce::Result::fail("Snapshot manifest is invalid.");

        const auto sourceProjectPath = manifestObject->getProperty("flp_relative_path").toString();
        const auto fallbackPath = manifestObject->getProperty("source_project_filename").toString();
        restoredProjectFile = !sourceProjectPath.isEmpty()
            ? restoreDirectory.getChildFile(sourceProjectPath)
            : restoreDirectory.getChildFile(fallbackPath);
    }

    if (!restoredProjectFile.existsAsFile())
    {
        juce::Array<juce::File> flpFiles;
        restoreDirectory.findChildFiles(flpFiles, juce::File::findFiles, true, "*.flp");
        if (!flpFiles.isEmpty())
            restoredProjectFile = flpFiles[0];
    }

    if (!restoredProjectFile.existsAsFile())
    {
        juce::Array<juce::File> alsFiles;
        restoreDirectory.findChildFiles(alsFiles, juce::File::findFiles, true, "*.als");
        if (!alsFiles.isEmpty())
            restoredProjectFile = alsFiles[0];
    }

    if (!restoredProjectFile.existsAsFile())
        return juce::Result::fail("Restored snapshot did not produce a project file.");

    return juce::Result::ok();
}

juce::File resolveEffectiveProjectFile(const juce::File& selectedFile,
                                       const juce::File& pendingFile)
{
    if (selectedFile.existsAsFile())
        return selectedFile;

    if (pendingFile.existsAsFile())
        return pendingFile;

    return {};
}

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

bool openFileInSystem(const juce::File& file)
{
    if (!file.existsAsFile())
        return false;

    if (file.startAsProcess())
        return true;

   #if JUCE_MAC
    juce::ChildProcess openProcess;
    const auto escapedPath = file.getFullPathName().replace("\"", "\\\"");
    return openProcess.start("open \"" + escapedPath + "\"");
   #else
    return false;
   #endif
}

juce::String sanitizePathSegment(const juce::String& value, const juce::String& fallback)
{
    juce::String output;
    for (auto ch : value)
    {
        const auto isAllowed = juce::CharacterFunctions::isLetterOrDigit(ch)
            || ch == '-'
            || ch == '_'
            || ch == '.';
        output += isAllowed ? juce::String::charToString(ch) : "-";
    }

    output = output.trim().replace("--", "-");
    while (output.contains("--"))
        output = output.replace("--", "-");
    output = output.trimCharactersAtStart("-").trimCharactersAtEnd("-");
    return output.isNotEmpty() ? output : fallback;
}

juce::File buildAutoRestoreCacheRoot(const juce::String& projectId, const juce::String& branchId)
{
    auto root = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                    .getChildFile("Stemhub")
                    .getChildFile("restore-cache")
                    .getChildFile(sanitizePathSegment(projectId, "project"))
                    .getChildFile(sanitizePathSegment(branchId, "branch"));
    return root;
}

juce::File buildAutoRestoreZipPath(const juce::String& projectId,
                                   const juce::String& branchId,
                                   const juce::String& versionId)
{
    const auto shortVersion = versionId.substring(0, juce::jmin(8, versionId.length()));
    auto root = buildAutoRestoreCacheRoot(projectId, branchId);
    return root.getChildFile("version-" + sanitizePathSegment(shortVersion, "latest") + ".zip");
}

juce::String tryRestoreLatestVersionToCache(const std::vector<VersionSummary>& versions,
                                            const juce::String& projectId,
                                            const juce::String& branchId,
                                            const VersionControlService& versionControlService,
                                            juce::File& restoredProjectFile)
{
    restoredProjectFile = juce::File();

    if (versions.empty())
        return {};

    const auto& latest = versions.front();
    if (!latest.hasArtifact || latest.id.isEmpty())
        return {};

    const auto zipPath = buildAutoRestoreZipPath(projectId, branchId, latest.id);
    const auto cacheRoot = zipPath.getParentDirectory();
    if ((!cacheRoot.exists() && !cacheRoot.createDirectory()) || !cacheRoot.isDirectory())
        return "Project ready, but failed to prepare local restore cache.";

    const auto restoreResult = versionControlService.restoreVersion(
        latest.id,
        zipPath,
        versionControlService.getAccessToken());
    if (restoreResult.failed())
        return "Project ready, but failed to auto-restore latest version: " + restoreResult.getErrorMessage();

    const auto restoreDirectory = zipPath.getParentDirectory().getChildFile(zipPath.getFileNameWithoutExtension());
    if (restoreDirectory.exists() && !restoreDirectory.deleteRecursively())
        return "Project ready, but failed to clear previous local restore cache.";

    auto extractResult = resolveRestoreResult(zipPath, restoreDirectory, restoredProjectFile);
    if (extractResult.failed())
        return "Project ready, but failed to extract latest version locally: " + extractResult.getErrorMessage();

    return {};
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
        const auto autoRestoreMessage = tryRestoreLatestVersionToCache(
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
        const auto autoRestoreMessage = tryRestoreLatestVersionToCache(
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
    const auto extractResult = resolveRestoreResult(destinationFile, restoreDirectory, restoredProjectFile);
    if (extractResult.failed())
    {
        result.errorMessage = extractResult.getErrorMessage();
        return result;
    }

    result.restoredProjectFile = std::move(restoredProjectFile);
    result.activeProjectStatusMessage = "Version restored successfully: " + result.restoredProjectFile.getFileName();
    return result;
}

void StemhubAudioProcessor::applyAuthRequestResult(AuthRequestResult result)
{
    const auto fromCachedSession = result.fromCachedSession;

    if (result.authErrorMessage.isNotEmpty())
    {
        if (fromCachedSession)
        {
            stemhub::sessioncache::clear();
            currentUser.reset();
            access_tkn.clear();
            versionControlService.clearAccessToken();
            authErrorMessage.clear();
            sessionState = {};
            sendChangeMessage();
            return;
        }

        authErrorMessage = result.authErrorMessage;
        setAuthState(AuthState::authError);
        return;
    }

    authErrorMessage.clear();
    projectSelectionStatusMessage = std::move(result.projectSelectionStatusMessage);
    activeProjectStatusMessage.clear();
    projects = std::move(result.projects);
    access_tkn = std::move(result.token);
    stemhub::sessioncache::saveAccessToken(access_tkn);
    versionControlService.setAccessToken(access_tkn);
    signIn(std::move(*result.user));

    if (fromCachedSession)
        requestRestoreCachedProjectContext();
}

void StemhubAudioProcessor::applyProjectActivationResult(ProjectActivationJobResult result)
{
    if (hasError(result))
    {
        setOperationState(OperationState::error);
        projectSelectionStatusMessage = result.errorMessage;
        return;
    }

    if (result.refreshProjects)
    {
        const auto count = static_cast<int>(result.projects.size());
        projects = std::move(result.projects);
        projectSelectionStatusMessage = count == 0 ? "No projects found."
                                                   : "Loaded " + juce::String(count) + " project(s).";
    }

    branches = std::move(result.branches);
    versionHistory = std::move(result.versions);
    selectedVersionId = chooseSelectedVersionId(versionHistory, result.selectedVersionId);
    selectProject(*result.selectedProject, result.branchId, result.branchName,
            std::move(result.projectFile));

    if (result.shouldAutoOpenLocalFile && selectedProjectFile.existsAsFile() && !openFileInSystem(selectedProjectFile))
    {
        setOperationState(OperationState::error);
        activeProjectStatusMessage = "Project loaded, but failed to open local project file: "
            + selectedProjectFile.getFullPathName();
        return;
    }

    setOperationState(OperationState::idle);
    activeProjectStatusMessage = std::move(result.activeProjectStatusMessage);
}

void StemhubAudioProcessor::applyBranchHistoryResult(BranchHistoryJobResult result)
{
    if (hasError(result))
    {
        setOperationState(OperationState::error);
        activeProjectStatusMessage = result.errorMessage;
        return;
    }

    selectedBranchId = std::move(result.branchId);
    selectedBranchName = std::move(result.branchName);
    versionHistory = std::move(result.versions);
    selectedVersionId = chooseSelectedVersionId(versionHistory, result.selectedVersionId);
    versionControlService.setCurrentProjectContext(makeProjectVersionContext(selectedProject,
                                                                             selectedBranchId,
                                                                             versionHistory));
    if (result.projectFile.existsAsFile())
    {
        selectedProjectFile = result.projectFile;
        pendingProjectFile = selectedProjectFile;

        if (!openFileInSystem(selectedProjectFile))
        {
            setOperationState(OperationState::error);
            activeProjectStatusMessage = "Branch loaded, but failed to open local project file: "
                + selectedProjectFile.getFullPathName();
            return;
        }
    }

    setOperationState(OperationState::idle);
    activeProjectStatusMessage = std::move(result.activeProjectStatusMessage);
}

void StemhubAudioProcessor::applyPushVersionResult(PushVersionJobResult result)
{
    if (hasError(result))
    {
        setOperationState(OperationState::error);
        activeProjectStatusMessage = result.errorMessage;
        return;
    }

    versionControlService.setLastVersionId(std::move(result.pushedVersionId));
    setOperationState(OperationState::idle);
    activeProjectStatusMessage = result.activeProjectStatusMessage.isNotEmpty()
        ? result.activeProjectStatusMessage
        : "Version pushed successfully.";
}

void StemhubAudioProcessor::applyRestoreVersionResult(RestoreVersionJobResult result)
{
    if (hasError(result))
    {
        setOperationState(OperationState::error);
        activeProjectStatusMessage = result.errorMessage;
        return;
    }

    if (result.restoredProjectFile.existsAsFile())
    {
        selectedProjectFile = result.restoredProjectFile;
        pendingProjectFile = selectedProjectFile;
        if (!openFileInSystem(selectedProjectFile))
        {
            setOperationState(OperationState::error);
            activeProjectStatusMessage = "Version restored, but failed to open project file: "
                + selectedProjectFile.getFullPathName();
            return;
        }
    }

    setOperationState(OperationState::idle);
    activeProjectStatusMessage = result.activeProjectStatusMessage;
}

void StemhubAudioProcessor::applyBackgroundResult(BackgroundJobResult result)
{
    std::visit([this](auto&& payload)
    {
        using Payload = std::decay_t<decltype(payload)>;

        if constexpr (std::is_same_v<Payload, AuthRequestResult>)
            applyAuthRequestResult(std::move(payload));
        else if constexpr (std::is_same_v<Payload, ProjectActivationJobResult>)
            applyProjectActivationResult(std::move(payload));
        else if constexpr (std::is_same_v<Payload, BranchHistoryJobResult>)
            applyBranchHistoryResult(std::move(payload));
        else if constexpr (std::is_same_v<Payload, PushVersionJobResult>)
            applyPushVersionResult(std::move(payload));
        else if constexpr (std::is_same_v<Payload, RestoreVersionJobResult>)
            applyRestoreVersionResult(std::move(payload));
    }, std::move(result.payload));
}

void StemhubAudioProcessor::signIn(User newUser) noexcept
{
    currentUser = std::move(newUser);
    sessionState.authState = AuthState::signedIn;
    sessionState.uiState = UIState::projectSelection;
    sessionState.operationState = OperationState::idle;
}

void StemhubAudioProcessor::signOut() noexcept
{
    backgroundJobs.invalidateSession();
    currentUser.reset();
    access_tkn.clear();
    versionControlService.clearAccessToken();
    stemhub::sessioncache::clear();
    authErrorMessage.clear();
    projectSelectionStatusMessage.clear();
    activeProjectStatusMessage.clear();
    projects.clear();
    branches.clear();
    versionHistory.clear();
    selectedVersionId.clear();
    clearSelectedProject();
    pendingProjectFile = juce::File();
    selectedProjectFile = juce::File();
    sessionState = {};
    sendChangeMessage();
}

void StemhubAudioProcessor::setAuthState(AuthState newAuthState) noexcept
{
    sessionState.authState = newAuthState;

    if (newAuthState != AuthState::signedIn)
    {
        sessionState.uiState = UIState::login;
        sessionState.operationState = OperationState::idle;
    }
}

void StemhubAudioProcessor::setUIState(UIState newUIState) noexcept
{
    sessionState.uiState = sessionState.authState == AuthState::signedIn ? newUIState : UIState::login;
}

void StemhubAudioProcessor::setOperationState(OperationState newOperationState) noexcept
{
    sessionState.operationState = sessionState.authState == AuthState::signedIn ? newOperationState
                                                                                : OperationState::idle;
}

void StemhubAudioProcessor::setProjectSelectionStatusMessage(juce::String message)
{
    projectSelectionStatusMessage = std::move(message);
    sendChangeMessage();
}

void StemhubAudioProcessor::setActiveProjectStatusMessage(juce::String message)
{
    activeProjectStatusMessage = std::move(message);
    sendChangeMessage();
}

void StemhubAudioProcessor::setPendingProjectFile(const juce::File& file)
{
    pendingProjectFile = file;
    sendChangeMessage();
}

void StemhubAudioProcessor::selectProject(Project project, juce::String branchId, juce::String branchName, juce::File projectFile)
{
    versionControlService.clearProjectContext();
    selectedProject = std::move(project);
    selectedBranchId = std::move(branchId);
    selectedBranchName = std::move(branchName);
    selectedProjectFile = std::move(projectFile);
    pendingProjectFile = selectedProjectFile;
    if (selectedProject.has_value())
        stemhub::sessioncache::saveProjectId(selectedProject->id);
    versionControlService.setCurrentProjectContext(makeProjectVersionContext(selectedProject,
                                                                             selectedBranchId,
                                                                             versionHistory));
    sessionState.uiState = UIState::dashboard;
    sendChangeMessage();
}

void StemhubAudioProcessor::clearSelectedProject() noexcept
{
    selectedProject.reset();
    selectedBranchId.clear();
    selectedBranchName.clear();
    selectedVersionId.clear();
    selectedProjectFile = juce::File();
    branches.clear();
    versionHistory.clear();
    versionControlService.clearProjectContext();
    activeProjectStatusMessage.clear();
}


void StemhubAudioProcessor::requestSignIn(const juce::String& email, const juce::String& password)
{
    if (sessionState.authState == AuthState::signingIn)
        return;

    setAuthState(AuthState::signingIn);
    authErrorMessage.clear();
    projectSelectionStatusMessage.clear();
    activeProjectStatusMessage.clear();
    projects.clear();
    branches.clear();
    versionHistory.clear();
    selectedVersionId.clear();
    sendChangeMessage();

    enqueueBackgroundTask([this, email, password]() -> BackgroundJobPayload
    {
        return performSignInRequest(email, password);
    });
}

void StemhubAudioProcessor::requestRestoreCachedSession()
{
    if (didAttemptCachedSessionRestore)
        return;

    didAttemptCachedSessionRestore = true;
    if (sessionState.authState == AuthState::signedIn || sessionState.authState == AuthState::signingIn)
        return;

    const auto cachedToken = stemhub::sessioncache::loadAccessToken().trim();
    if (cachedToken.isEmpty())
        return;

    setAuthState(AuthState::signingIn);
    authErrorMessage.clear();
    projectSelectionStatusMessage = "Restoring session...";
    activeProjectStatusMessage.clear();
    sendChangeMessage();

    enqueueBackgroundTask([this, token = cachedToken]() -> BackgroundJobPayload
    {
        return performRestoreCachedSessionRequest(token);
    });
}

void StemhubAudioProcessor::requestRestoreCachedProjectContext()
{
    if (selectedProject.has_value() || access_tkn.isEmpty() || projects.empty())
        return;

    const auto cachedProjectId = stemhub::sessioncache::loadProjectId().trim();
    if (cachedProjectId.isEmpty())
        return;

    const auto projectIt = std::find_if(projects.begin(), projects.end(), [&cachedProjectId](const Project& project)
    {
        return project.id == cachedProjectId;
    });
    if (projectIt == projects.end())
        return;

    setOperationState(OperationState::loadingProjects);
    projectSelectionStatusMessage = "Restoring last opened project...";
    sendChangeMessage();

    const auto projectsSnapshot = projects;
    const auto token = access_tkn;
    enqueueBackgroundTask([this, cachedProjectId, projectsSnapshot, token]() -> BackgroundJobPayload
    {
        auto result = performOpenProjectRequest(cachedProjectId, {}, projectsSnapshot, token);
        result.shouldAutoOpenLocalFile = false;
        return result;
    });
}

void StemhubAudioProcessor::requestOpenProject(juce::String projectId, juce::File localProjectFile)
{
    setOperationState(OperationState::loadingProjects);
    sendChangeMessage();

    const auto projectsSnapshot = projects;
    const auto token = access_tkn;
    enqueueBackgroundTask([this,
                           requestedProjectId = std::move(projectId),
                           requestedProjectFile = std::move(localProjectFile),
                           projectsSnapshot,
                           token]() -> BackgroundJobPayload
    {
        return performOpenProjectRequest(requestedProjectId, requestedProjectFile, projectsSnapshot, token);
    });
}

void StemhubAudioProcessor::requestCreateProject(juce::File localProjectFile)
{
    setOperationState(OperationState::loadingProjects);
    sendChangeMessage();

    const auto token = access_tkn;
    enqueueBackgroundTask([this,
                           requestedProjectFile = std::move(localProjectFile),
                           token]() -> BackgroundJobPayload
    {
        return performCreateProjectRequest(requestedProjectFile, token);
    });
}

void StemhubAudioProcessor::requestSelectBranch(juce::String branchId)
{
    if (!selectedProject.has_value())
    {
        setOperationState(OperationState::error);
        setActiveProjectStatusMessage("Choose a project before selecting a branch.");
        return;
    }

    const auto branchIt = std::find_if(branches.begin(), branches.end(), [&branchId](const Branch& branch)
    {
        return branch.id == branchId;
    });

    if (branchIt == branches.end())
    {
        setOperationState(OperationState::error);
        setActiveProjectStatusMessage("Selected branch is no longer available.");
        return;
    }

    setOperationState(OperationState::pulling);
    setActiveProjectStatusMessage("Loading branch history...");

    const auto token = access_tkn;
    const auto branchName = branchIt->name;
    enqueueBackgroundTask([this,
                           requestedBranchId = std::move(branchId),
                           requestedBranchName = std::move(branchName),
                           token]() -> BackgroundJobPayload
    {
        return performFetchBranchHistoryRequest(requestedBranchId, requestedBranchName, {}, token);
    });
}

void StemhubAudioProcessor::requestRefreshVersionHistory()
{
    if (!hasProjectAndBranchSelected(selectedProject, selectedBranchId))
    {
        setOperationState(OperationState::error);
        setActiveProjectStatusMessage("Choose a project and branch before refreshing history.");
        return;
    }

    const auto branchNameIt = std::find_if(branches.begin(), branches.end(), [this](const Branch& branch)
    {
        return branch.id == selectedBranchId;
    });
    const auto branchName = branchNameIt != branches.end() ? branchNameIt->name : selectedBranchName;

    setOperationState(OperationState::pulling);
    setActiveProjectStatusMessage("Refreshing version history...");

    const auto token = access_tkn;
    const auto branchId = selectedBranchId;
    const auto preferredVersionId = selectedVersionId;
    enqueueBackgroundTask([this,
                           branchId,
                           branchName,
                           preferredVersionId,
                           token]() -> BackgroundJobPayload
    {
        return performFetchBranchHistoryRequest(branchId, branchName, preferredVersionId, token);
    });
}

void StemhubAudioProcessor::requestPushVersion(juce::String commitMessage, juce::String dawName)
{
    setOperationState(OperationState::committing);
    sendChangeMessage();

    const auto projectFile = resolveEffectiveProjectFile(selectedProjectFile, pendingProjectFile);
    const auto projectRootDirectory = projectFile.existsAsFile() ? projectFile.getParentDirectory() : juce::File();
    const auto project = selectedProject;
    const auto branchId = selectedBranchId;
    enqueueBackgroundTask([this,
                           selectedFile = std::move(projectFile),
                           selectedProjectRoot = std::move(projectRootDirectory),
                           project,
                           selectedBranch = std::move(branchId),
                           requestedCommitMessage = std::move(commitMessage),
                           requestedDawName = std::move(dawName)]() mutable -> BackgroundJobPayload
    {
        return performPushVersionRequest(selectedFile,
                                         selectedProjectRoot,
                                         project,
                                         selectedBranch,
                                         requestedCommitMessage,
                                         requestedDawName);
    });
}

void StemhubAudioProcessor::requestRestoreVersion(const juce::String& versionId, const juce::File& projectFolder)
{
    if (!hasProjectAndBranchSelected(selectedProject, selectedBranchId))
    {
        setOperationState(OperationState::error);
        setActiveProjectStatusMessage("Choose a project before restoring.");
        return;
    }

    if (!projectFolder.isDirectory())
    {
        setOperationState(OperationState::error);
        setActiveProjectStatusMessage("Choose a valid restore destination folder.");
        return;
    }

    if (versionId.isEmpty())
    {
        setOperationState(OperationState::error);
        setActiveProjectStatusMessage("Select a version before restoring.");
        return;
    }

    const auto projectName = selectedProject ? selectedProject->name : juce::String();
    const auto restoredProjectBase = resolveRestoreProjectName(versionHistory, versionId, projectName);
    const auto fileName = restoredProjectBase + "-" + versionId.substring(0, juce::jmin(8, versionId.length())) + ".zip";
    const auto restoreFile = projectFolder.getChildFile(fileName);

    setOperationState(OperationState::pulling);
    setActiveProjectStatusMessage("Restoring selected version...");
    sendChangeMessage();

    const auto requestedVersionId = versionId;
    const auto requestedRestoreFile = restoreFile;
    enqueueBackgroundTask([this,
                           requestedVersionId,
                           requestedRestoreFile]() -> BackgroundJobPayload
    {
        return performRestoreVersionRequest(requestedVersionId, requestedRestoreFile);
    });
}

void StemhubAudioProcessor::setSelectedVersionId(juce::String versionId)
{
    selectedVersionId = std::move(versionId);
    sendChangeMessage();
}

void StemhubAudioProcessor::handleAsyncUpdate()
{
    const auto didApply = backgroundJobs.flushResults([this](auto&& result)
    {
        applyBackgroundResult(std::forward<decltype(result)>(result));
    });

    if (didApply)
        sendChangeMessage();
}

const juce::String StemhubAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool StemhubAudioProcessor::acceptsMidi() const
{
#if JucePlugin_WantsMidiInput
    return true;
#else
    return false;
#endif
}

bool StemhubAudioProcessor::producesMidi() const
{
#if JucePlugin_ProducesMidiOutput
    return true;
#else
    return false;
#endif
}

bool StemhubAudioProcessor::isMidiEffect() const
{
#if JucePlugin_IsMidiEffect
    return true;
#else
    return false;
#endif
}

double StemhubAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int StemhubAudioProcessor::getNumPrograms()
{
    return 1;
}

int StemhubAudioProcessor::getCurrentProgram()
{
    return 0;
}

void StemhubAudioProcessor::setCurrentProgram(int index)
{
    juce::ignoreUnused(index);
}

const juce::String StemhubAudioProcessor::getProgramName(int index)
{
    juce::ignoreUnused(index);
    return {};
}

void StemhubAudioProcessor::changeProgramName(int index, const juce::String& newName)
{
    juce::ignoreUnused(index, newName);
}

void StemhubAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused(sampleRate, samplesPerBlock);
}

void StemhubAudioProcessor::releaseResources()
{
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool StemhubAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
#if JucePlugin_IsMidiEffect
    juce::ignoreUnused(layouts);
    return true;
#else
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

#if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
#endif

    return true;
#endif
}
#endif

template <typename SampleType>
static void clearExtraOutputChannels(juce::AudioProcessor& processor, juce::AudioBuffer<SampleType>& buffer)
{
    const auto totalNumInputChannels = processor.getTotalNumInputChannels();
    const auto totalNumOutputChannels = processor.getTotalNumOutputChannels();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, buffer.getNumSamples());
}

void StemhubAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    juce::ignoreUnused(midiMessages);
    clearExtraOutputChannels(*this, buffer);
}

void StemhubAudioProcessor::processBlock(juce::AudioBuffer<double>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    juce::ignoreUnused(midiMessages);
    clearExtraOutputChannels(*this, buffer);
}

bool StemhubAudioProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor* StemhubAudioProcessor::createEditor()
{
    return new StemhubAudioProcessorEditor(*this);
}

void StemhubAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    juce::ignoreUnused(destData);
}

void StemhubAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    juce::ignoreUnused(data, sizeInBytes);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new StemhubAudioProcessor();
}
