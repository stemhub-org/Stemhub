#include <algorithm>
#include <vector>

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

std::vector<juce::File> collectFilesByExtension(const juce::File& rootDirectory, const juce::String& extension)
{
    std::vector<juce::File> matched;
    if (!rootDirectory.isDirectory())
        return matched;

    juce::Array<juce::File> allFiles;
    rootDirectory.findChildFiles(allFiles, juce::File::findFiles, true, "*");

    const auto normalizedExtension = extension.toLowerCase();
    for (const auto& file : allFiles)
    {
        auto fileExtension = file.getFileExtension().toLowerCase();
        if (fileExtension.startsWith("."))
            fileExtension = fileExtension.substring(1);
        if (fileExtension == normalizedExtension)
            matched.push_back(file);
    }

    std::sort(matched.begin(), matched.end(), [](const juce::File& lhs, const juce::File& rhs)
    {
        return lhs.getFullPathName().compareNatural(rhs.getFullPathName()) < 0;
    });

    return matched;
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

juce::File buildWorkingCopyRoot(const juce::String& projectId, const juce::String& branchId)
{
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("Stemhub")
        .getChildFile("working-copy")
        .getChildFile(sanitizePathSegment(projectId, "project"))
        .getChildFile(sanitizePathSegment(branchId, "branch"))
        .getChildFile("current");
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
    juce::Logger::writeToLog("[Restore] ProjectFileService -> resolveRestoreProjectName versionId=" + versionId
                             + ", fallback=" + fallbackName);
    const auto it = std::find_if(versions.begin(), versions.end(), [&versionId](const VersionSummary& version)
    {
        return version.id == versionId;
    });

    if (it != versions.end() && it->sourceProjectFilename.isNotEmpty())
    {
        const auto sourceProjectName = juce::File(it->sourceProjectFilename).getFileNameWithoutExtension();
        if (sourceProjectName.isNotEmpty())
        {
            juce::Logger::writeToLog("[Restore] ProjectFileService -> found sourceProjectFilename="
                                     + it->sourceProjectFilename);
            return sourceProjectName;
        }
    }

    if (it == versions.end())
        juce::Logger::writeToLog("[Restore] ProjectFileService -> version not found in provided history");

    return fallbackName.isNotEmpty() ? fallbackName : "restored-project";
}

juce::Result resolveRestoreResult(const juce::File& snapshotZipFile,
                                  const juce::File& restoreDirectory,
                                  juce::File& restoredProjectFile)
{
    juce::Logger::writeToLog("[Restore] ProjectFileService -> resolveRestoreResult zip="
                             + snapshotZipFile.getFullPathName()
                             + ", restoreDir="
                             + restoreDirectory.getFullPathName());
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
    juce::Logger::writeToLog("[Restore] ProjectFileService -> archive uncompressed");

    const auto manifestFile = restoreDirectory.getChildFile("manifest.json");
    if (manifestFile.existsAsFile())
    {
        const auto manifestText = manifestFile.loadFileAsString();
        const auto manifestObject = juce::JSON::parse(manifestText).getDynamicObject();
        if (manifestObject == nullptr)
            return juce::Result::fail("Snapshot manifest is invalid.");

        const auto sourceProjectPath = manifestObject->getProperty("flp_relative_path").toString();
        const auto fallbackPath = manifestObject->getProperty("source_project_filename").toString();
        juce::String normalizedSourceProjectPath = sourceProjectPath.replaceCharacter('\\', '/').trim();
        juce::String normalizedFallbackProjectPath = fallbackPath.replaceCharacter('\\', '/').trim();
        if (normalizedSourceProjectPath.startsWith("/"))
            normalizedSourceProjectPath = normalizedSourceProjectPath.substring(1);
        if (normalizedSourceProjectPath.startsWith("./"))
            normalizedSourceProjectPath = normalizedSourceProjectPath.substring(2);
        if (normalizedFallbackProjectPath.startsWith("/"))
            normalizedFallbackProjectPath = normalizedFallbackProjectPath.substring(1);
        if (normalizedFallbackProjectPath.startsWith("./"))
            normalizedFallbackProjectPath = normalizedFallbackProjectPath.substring(2);

        restoredProjectFile = !normalizedSourceProjectPath.isEmpty()
            ? restoreDirectory.getChildFile(normalizedSourceProjectPath)
            : restoreDirectory.getChildFile(normalizedFallbackProjectPath);
        juce::Logger::writeToLog("[Restore] ProjectFileService -> manifest sourceProjectPath="
                                 + normalizedSourceProjectPath
                                 + ", fallbackPath="
                                 + normalizedFallbackProjectPath);
    }

    if (!restoredProjectFile.existsAsFile())
    {
        const auto flpFiles = collectFilesByExtension(restoreDirectory, "flp");
        if (!flpFiles.empty())
        {
            juce::Logger::writeToLog("[Restore] ProjectFileService -> fallback FLP file=" + flpFiles.front().getFileName());
            restoredProjectFile = flpFiles.front();
        }
    }

    if (!restoredProjectFile.existsAsFile())
    {
        const auto alsFiles = collectFilesByExtension(restoreDirectory, "als");
        if (!alsFiles.empty())
        {
            juce::Logger::writeToLog("[Restore] ProjectFileService -> fallback ALS file=" + alsFiles.front().getFileName());
            restoredProjectFile = alsFiles.front();
        }
    }

    if (!restoredProjectFile.existsAsFile())
    {
        juce::Logger::writeToLog("[Restore] ProjectFileService -> no compatible project file found in restore directory");
        return juce::Result::fail("Restored snapshot did not produce a project file.");
    }

    juce::Logger::writeToLog("[Restore] ProjectFileService -> final restored file="
                             + restoredProjectFile.getFullPathName()
                             + ", size="
                             + juce::String(restoredProjectFile.getSize()));

    return juce::Result::ok();
}

juce::Result materializeRestoredSnapshotAsWorkingCopy(const juce::File& restoredProjectFile,
                                                      const juce::String& projectId,
                                                      const juce::String& branchId,
                                                      juce::File& workingProjectFile)
{
    workingProjectFile = juce::File();

    if (!restoredProjectFile.existsAsFile())
        return juce::Result::fail("Restored project file is missing.");

    const auto restoredRootDirectory = restoredProjectFile.getParentDirectory();
    if (!restoredRootDirectory.isDirectory())
        return juce::Result::fail("Restored project folder is missing.");

    auto relativeProjectPath = restoredProjectFile.getRelativePathFrom(restoredRootDirectory).replaceCharacter('\\', '/');
    if (relativeProjectPath.isEmpty())
        relativeProjectPath = restoredProjectFile.getFileName();

    const auto workingRoot = buildWorkingCopyRoot(projectId, branchId);
    if (workingRoot.exists() && !workingRoot.deleteRecursively())
        return juce::Result::fail("Failed to clear previous local working copy.");

    if (!workingRoot.createDirectory())
        return juce::Result::fail("Failed to create local working copy folder.");

    juce::Array<juce::File> restoredFiles;
    restoredRootDirectory.findChildFiles(restoredFiles, juce::File::findFiles, true, "*");

    for (const auto& file : restoredFiles)
    {
        const auto relativePath = file.getRelativePathFrom(restoredRootDirectory).replaceCharacter('\\', '/');
        if (relativePath.isEmpty())
            continue;

        const auto destinationFile = workingRoot.getChildFile(relativePath);
        const auto destinationParent = destinationFile.getParentDirectory();
        if ((!destinationParent.exists() && !destinationParent.createDirectory()) || !destinationParent.isDirectory())
            return juce::Result::fail("Failed to create local working copy subfolder.");

        if (destinationFile.existsAsFile() && !destinationFile.deleteFile())
            return juce::Result::fail("Failed to overwrite file in local working copy.");

        if (!file.copyFileTo(destinationFile))
            return juce::Result::fail("Failed to copy restored files into local working copy.");
    }

    workingProjectFile = workingRoot.getChildFile(relativeProjectPath);
    if (!workingProjectFile.existsAsFile())
        return juce::Result::fail("Local working copy project file is missing.");

    juce::Logger::writeToLog("[Restore] ProjectFileService -> materialized local working copy file="
                             + workingProjectFile.getFullPathName());

    return juce::Result::ok();
}

juce::File resolveEffectiveProjectFile(const juce::File& selectedFile,
                                       const juce::File& pendingFile)
{
    if (pendingFile.existsAsFile())
        return pendingFile;

    if (selectedFile.existsAsFile())
        return selectedFile;

    return {};
}

bool isManagedRestoreCacheFile(const juce::File& file)
{
    if (!file.existsAsFile())
        return false;

    const auto cacheRoot = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                               .getChildFile("Stemhub")
                               .getChildFile("restore-cache");
    return file.isAChildOf(cacheRoot);
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
