#include <algorithm>

#include "application/ProjectFileService.hpp"

namespace
{
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
}

namespace stemhub::projectfiles
{
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

bool openInSystem(const juce::File& file)
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

