#include <map>

#include "application/SnapshotBundler.hpp"
#include "application/SnapshotFiles.hpp"

namespace
{
    // Backend limit on the files of one version besides the project file (VersionManifestV1.tracks).
    constexpr size_t kMaxManifestTracks = 500;

    juce::String toArchivePath(const juce::File& file, const juce::File& rootDirectory)
    {
        return file.getRelativePathFrom(rootDirectory).replaceCharacter('\\', '/');
    }
}

juce::Result SnapshotBundler::buildManifest(const SnapshotBundleRequest& request,
                                              ContentAddressedManifest& outResult) const
{
    outResult = {};

    if (!request.sourceProjectFile.existsAsFile())
        return juce::Result::fail("Source project file does not exist.");

    const auto rootDirectory = request.sourceProjectFile.getParentDirectory();
    const auto includedFiles = stemhub::snapshotfiles::collect(request.sourceProjectFile);

    // Check names and the file count before hashing anything: hashing a large session takes time.
    for (const auto& file : includedFiles)
    {
        // Keep the folder structure: basenames alone made files from different folders collide.
        const auto relativePath = toArchivePath(file, rootDirectory);
        if (!isSafeManifestPath(relativePath))
            return juce::Result::fail("Can't save \"" + relativePath + "\": rename this file and try again.");
    }

    const auto trackCount = includedFiles.size() - 1;
    if (trackCount > kMaxManifestTracks)
        return juce::Result::fail("This project folder has " + juce::String(static_cast<int>(trackCount))
                                  + " audio files; a version can hold " + juce::String(static_cast<int>(kMaxManifestTracks))
                                  + ". Move the ones this project doesn't use out of its folder.");

    std::vector<ContentAddressedFileEntry> entries;
    entries.reserve(static_cast<size_t>(includedFiles.size()));
    ContentAddressedFileEntry projectEntry;
    bool haveProjectEntry = false;

    for (const auto& file : includedFiles)
    {
        const auto sha = sha256OfFile(file);
        if (sha.isEmpty())
            return juce::Result::fail("Failed to hash file: " + file.getFullPathName());

        ContentAddressedFileEntry entry;
        entry.file = file;
        entry.sha256 = sha;
        entry.sizeBytes = file.getSize();
        entry.filename = toArchivePath(file, rootDirectory);
        entry.isProjectFile = (file == request.sourceProjectFile);

        if (entry.isProjectFile)
        {
            projectEntry = entry;
            haveProjectEntry = true;
        }
        else
        {
            entries.push_back(entry);
        }
    }

    if (!haveProjectEntry)
        return juce::Result::fail("Project file was not included in the discovered set.");

    juce::DynamicObject::Ptr projectFileObj = new juce::DynamicObject();
    projectFileObj->setProperty("sha256", projectEntry.sha256);
    projectFileObj->setProperty("size_bytes", projectEntry.sizeBytes);
    projectFileObj->setProperty("filename", projectEntry.filename);

    juce::Array<juce::var> tracks;
    tracks.ensureStorageAllocated(static_cast<int>(entries.size()));
    for (const auto& e : entries)
    {
        juce::DynamicObject::Ptr t = new juce::DynamicObject();
        t->setProperty("sha256", e.sha256);
        t->setProperty("size_bytes", e.sizeBytes);
        t->setProperty("filename", e.filename);
        t->setProperty("name", e.file.getFileNameWithoutExtension());
        tracks.add(juce::var(t.get()));
    }

    juce::DynamicObject::Ptr manifest = new juce::DynamicObject();
    manifest->setProperty("manifest_version", 1);
    manifest->setProperty("source_daw", request.sourceDaw);
    manifest->setProperty("source_project_filename", projectEntry.file.getFileName());
    manifest->setProperty("project_file", juce::var(projectFileObj.get()));
    manifest->setProperty("tracks", juce::var(tracks));

    outResult.manifestJson = juce::var(manifest.get());
    // Combine project + tracks so callers can iterate every blob to upload.
    outResult.entries.reserve(entries.size() + 1);
    outResult.entries.push_back(projectEntry);
    for (auto& e : entries)
        outResult.entries.push_back(std::move(e));

    return juce::Result::ok();
}

namespace
{
    constexpr int kMaxManifestPathLength = 255; // ManifestBlobRef.filename limit on the backend

    bool isSha256Hex(const juce::String& value)
    {
        return value.length() == 64 && value.containsOnly("0123456789abcdef");
    }

    // Device names Windows resolves anywhere in a path ("NUL.wav" included).
    bool isReservedWindowsName(const juce::String& segment)
    {
        static const juce::StringArray reservedNames {
            "CON", "PRN", "AUX", "NUL",
            "COM1", "COM2", "COM3", "COM4", "COM5", "COM6", "COM7", "COM8", "COM9",
            "LPT1", "LPT2", "LPT3", "LPT4", "LPT5", "LPT6", "LPT7", "LPT8", "LPT9"
        };

        return reservedNames.contains(segment.upToFirstOccurrenceOf(".", false, false).trimEnd(), true);
    }

    bool readBlobRef(const juce::var& value, ParsedManifestEntry& outEntry)
    {
        auto* obj = value.getDynamicObject();
        if (obj == nullptr)
            return false;

        outEntry.sha256 = obj->getProperty("sha256").toString().toLowerCase();
        outEntry.filename = obj->getProperty("filename").toString();
        outEntry.sizeBytes = static_cast<juce::int64>(obj->getProperty("size_bytes"));
        return isSha256Hex(outEntry.sha256) && outEntry.filename.isNotEmpty();
    }

    juce::String describeManifestPath(const juce::String& path)
    {
        const auto printable = path.retainCharacters(
            "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 ._-/()");
        return "\"" + (printable.length() > 80 ? printable.substring(0, 80) + "..." : printable) + "\"";
    }
}

juce::String SnapshotBundler::sha256OfFile(const juce::File& file)
{
    juce::FileInputStream stream(file);
    if (!stream.openedOk())
        return {};

    return juce::SHA256(stream).toHexString();
}

bool SnapshotBundler::isSafeManifestPath(const juce::String& path)
{
    if (path.isEmpty() || path.length() > kMaxManifestPathLength)
        return false;

    // Backslashes and colons turn into separators, drive letters or streams on Windows.
    if (path.startsWithChar('/') || path.containsAnyOf("\\:"))
        return false;

    for (const auto character : path)
        if (character < 0x20 || character == 0x7f)
            return false;

    juce::StringArray segments;
    segments.addTokens(path, "/", {});

    for (const auto& segment : segments)
    {
        // Empty, "." and ".." segments navigate. Windows also drops trailing dots and spaces,
        // so "name." and "name " would land on "name".
        if (segment.isEmpty() || segment.endsWithChar('.') || segment.endsWithChar(' ')
            || isReservedWindowsName(segment))
            return false;
    }

    return true;
}

juce::Result SnapshotBundler::parseManifest(const juce::var& manifestJson,
                                              ParsedManifest& outResult)
{
    outResult = {};

    auto* root = manifestJson.getDynamicObject();
    if (root == nullptr)
        return juce::Result::fail("Manifest is not a JSON object.");

    const auto version = static_cast<int>(root->getProperty("manifest_version"));
    if (version != 1)
        return juce::Result::fail("Unsupported manifest_version: " + juce::String(version));

    outResult.manifestVersion = version;
    outResult.sourceDaw = root->getProperty("source_daw").toString();
    outResult.sourceProjectFilename = root->getProperty("source_project_filename").toString();

    ParsedManifestEntry projectEntry;
    if (!readBlobRef(root->getProperty("project_file"), projectEntry))
        return juce::Result::fail("Manifest project_file is missing or malformed.");
    projectEntry.isProjectFile = true;

    std::vector<ParsedManifestEntry> candidates;
    candidates.push_back(std::move(projectEntry));

    const auto tracksVar = root->getProperty("tracks");
    if (tracksVar.isArray())
    {
        for (const auto& trackVar : *tracksVar.getArray())
        {
            ParsedManifestEntry entry;
            if (!readBlobRef(trackVar, entry))
                return juce::Result::fail("Manifest track entry is malformed.");
            candidates.push_back(std::move(entry));
        }
    }

    // One file per path. Paths are compared case-insensitively because macOS and Windows
    // file systems are; the same content listed twice is written once.
    std::map<juce::String, juce::String> hashByPath;
    for (auto& entry : candidates)
    {
        if (!isSafeManifestPath(entry.filename))
            return juce::Result::fail("This version contains an unsafe file path "
                                      + describeManifestPath(entry.filename) + " and was not restored.");

        const auto key = entry.filename.toLowerCase();
        if (const auto existing = hashByPath.find(key); existing != hashByPath.end())
        {
            if (existing->second != entry.sha256)
                return juce::Result::fail("This version lists two different files at "
                                          + describeManifestPath(entry.filename) + " and was not restored.");
            continue;
        }

        hashByPath.emplace(key, entry.sha256);
        outResult.entries.push_back(std::move(entry));
    }

    return juce::Result::ok();
}
