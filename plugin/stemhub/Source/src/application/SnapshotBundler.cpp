#include <algorithm>

#include "application/SnapshotBundler.hpp"

namespace
{
    juce::String toArchivePath(const juce::File& file, const juce::File& rootDirectory)
    {
        return file.getRelativePathFrom(rootDirectory).replaceCharacter('\\', '/');
    }

    bool isSourceFileWithinRoot(const juce::File& sourceFile, const juce::File& rootDirectory)
    {
        if (!sourceFile.existsAsFile() || !rootDirectory.isDirectory())
            return false;

        const auto sourceParent = sourceFile.getParentDirectory();
        return sourceParent == rootDirectory || sourceFile.isAChildOf(rootDirectory);
    }
}

juce::Result SnapshotBundler::bundleProject(const SnapshotBundleRequest& request,
                                                  SnapshotBundleResult& outResult) const
{
    outResult = {};

    if (!request.sourceProjectFile.existsAsFile())
        return juce::Result::fail("Source project file does not exist.");

    if (!request.projectRootDirectory.isDirectory())
        return juce::Result::fail("Project root directory does not exist.");

    if (!isSourceFileWithinRoot(request.sourceProjectFile, request.projectRootDirectory))
        return juce::Result::fail("Project file must be inside the selected project root directory.");

    juce::Array<juce::File> discoveredFiles;
    request.projectRootDirectory.findChildFiles(discoveredFiles, juce::File::findFiles, true);

    if (discoveredFiles.isEmpty())
        return juce::Result::fail("Project root directory does not contain files to bundle.");

    std::sort(discoveredFiles.begin(), discoveredFiles.end(), [](const juce::File& lhs, const juce::File& rhs)
    {
        return lhs.getFullPathName() < rhs.getFullPathName();
    });

    const auto tempRoot = juce::File::getSpecialLocation(juce::File::tempDirectory)
                              .getChildFile("stemhub-snapshots");

    if ((!tempRoot.exists() && !tempRoot.createDirectory()) || !tempRoot.isDirectory())
        return juce::Result::fail("Failed to prepare temporary folder for snapshot export.");

    const auto bundleId = juce::Uuid().toString();
    const auto manifestFile = tempRoot.getChildFile("manifest_" + bundleId + ".json");
    const auto bundleFile = tempRoot.getChildFile("snapshot_" + bundleId + ".zip");

    juce::Array<juce::var> manifestFiles;
    manifestFiles.ensureStorageAllocated(discoveredFiles.size());

    juce::ZipFile::Builder zipBuilder;

    for (const auto& file : discoveredFiles)
    {
        if (!file.existsAsFile())
            continue;

        const auto archivePath = toArchivePath(file, request.projectRootDirectory);
        if (archivePath.isEmpty())
            continue;

        zipBuilder.addFile(file, 9, archivePath);

        juce::DynamicObject::Ptr fileEntry = new juce::DynamicObject();
        fileEntry->setProperty("relative_path", archivePath);
        fileEntry->setProperty("size_bytes", file.getSize());
        manifestFiles.add(juce::var(fileEntry.get()));
    }

    if (manifestFiles.isEmpty())
        return juce::Result::fail("Project root directory does not contain files to bundle.");

    juce::DynamicObject::Ptr manifestObject = new juce::DynamicObject();
    manifestObject->setProperty("schema_version", 1);
    manifestObject->setProperty("created_at_utc", juce::Time::getCurrentTime().toISO8601(true));
    manifestObject->setProperty("source_daw", request.sourceDaw);
    manifestObject->setProperty("project_name", request.sourceProjectFile.getFileNameWithoutExtension());
    manifestObject->setProperty("flp_relative_path", toArchivePath(request.sourceProjectFile, request.projectRootDirectory));
    manifestObject->setProperty("file_count", manifestFiles.size());
    manifestObject->setProperty("files", juce::var(manifestFiles));

    const auto manifest = juce::var(manifestObject.get());
    const auto manifestJson = juce::JSON::toString(manifest, true);
    if (!manifestFile.replaceWithText(manifestJson))
        return juce::Result::fail("Failed to write snapshot manifest.");

    zipBuilder.addFile(manifestFile, 9, "manifest.json");

    juce::FileOutputStream output(bundleFile);
    if (!output.openedOk())
    {
        manifestFile.deleteFile();
        return juce::Result::fail("Failed to create snapshot bundle file.");
    }

    if (!zipBuilder.writeToStream(output, nullptr))
    {
        manifestFile.deleteFile();
        bundleFile.deleteFile();
        return juce::Result::fail("Failed to write snapshot bundle archive.");
    }

    output.flush();
    manifestFile.deleteFile();

    outResult.bundleFile = bundleFile;
    outResult.manifest = manifest;
    return juce::Result::ok();
}
