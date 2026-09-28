#include <map>

#include "domain/Manifest.hpp"

namespace stemhub::manifest
{
namespace
{
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

juce::var toJson(const FileRef& file, bool withName)
{
    auto* object = new juce::DynamicObject();
    object->setProperty("sha256", file.sha256);
    object->setProperty("size_bytes", file.sizeBytes);
    object->setProperty("filename", file.path);
    if (withName)
        object->setProperty("name", file.path.fromLastOccurrenceOf("/", false, false).upToLastOccurrenceOf(".", false, false));

    return juce::var(object);
}

bool readFileRef(const juce::var& value, FileRef& out)
{
    if (!value.isObject())
        return false;

    out.sha256 = value.getProperty("sha256", {}).toString().toLowerCase();
    out.path = value.getProperty("filename", {}).toString();
    out.sizeBytes = static_cast<juce::int64>(value.getProperty("size_bytes", -1));
    return isSha256Hex(out.sha256) && out.path.isNotEmpty() && out.sizeBytes >= 0;
}

juce::String describePath(const juce::String& path)
{
    const auto printable = path.retainCharacters(
        "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 ._-/()");
    return "\"" + (printable.length() > 80 ? printable.substring(0, 80) + "..." : printable) + "\"";
}

juce::int64 sizeOf(const juce::var& fileRef)
{
    return juce::jmax<juce::int64>(0, static_cast<juce::int64>(fileRef.getProperty("size_bytes", 0)));
}
}

juce::var toJson(const Manifest& manifest)
{
    juce::Array<juce::var> tracks;
    tracks.ensureStorageAllocated(static_cast<int>(manifest.tracks.size()));
    for (const auto& track : manifest.tracks)
        tracks.add(toJson(track, true));

    auto* json = new juce::DynamicObject();
    json->setProperty("manifest_version", 1);
    json->setProperty("source_daw", manifest.sourceDaw);
    json->setProperty("source_project_filename", manifest.projectFile.path.fromLastOccurrenceOf("/", false, false));
    json->setProperty("project_file", toJson(manifest.projectFile, false));
    json->setProperty("tracks", tracks);
    return juce::var(json);
}

juce::Result fromJson(const juce::var& json, Manifest& out)
{
    out = {};

    if (!json.isObject())
        return juce::Result::fail("The version's file list is not a JSON object.");

    const auto version = static_cast<int>(json.getProperty("manifest_version", 0));
    if (version != 1)
        return juce::Result::fail("This version's file list has an unsupported format (" + juce::String(version) + ").");

    out.sourceDaw = json.getProperty("source_daw", {}).toString();

    if (!readFileRef(json.getProperty("project_file", {}), out.projectFile))
        return juce::Result::fail("This version's project file entry is missing or malformed.");

    std::vector<FileRef> candidates;
    if (const auto* tracks = json.getProperty("tracks", {}).getArray())
    {
        for (const auto& trackJson : *tracks)
        {
            FileRef track;
            if (!readFileRef(trackJson, track))
                return juce::Result::fail("This version's file list has a malformed entry.");
            candidates.push_back(std::move(track));
        }
    }

    // One file per path. The same content listed twice is written once.
    std::map<juce::String, juce::String> hashByPath;
    juce::String error;
    // False when the path was taken already: by the same file, or by another one (an error).
    const auto addPath = [&hashByPath, &error](const FileRef& file)
    {
        if (!isSafePath(file.path))
        {
            error = "This version contains an unsafe file path " + describePath(file.path) + " and was not restored.";
            return false;
        }

        const auto [existing, isNew] = hashByPath.emplace(file.path.toLowerCase(), file.sha256);
        if (!isNew && existing->second != file.sha256)
            error = "This version lists two different files at " + describePath(file.path) + " and was not restored.";

        return isNew;
    };

    addPath(out.projectFile);
    for (size_t index = 0; index < candidates.size() && error.isEmpty(); ++index)
        if (addPath(candidates[index]) && error.isEmpty())
            out.tracks.push_back(std::move(candidates[index]));

    return error.isEmpty() ? juce::Result::ok() : juce::Result::fail(error);
}

bool isSafePath(const juce::String& path)
{
    if (path.isEmpty() || path.length() > kMaxPathLength)
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

juce::int64 totalSize(const juce::var& json)
{
    if (!json.isObject())
        return 0;

    auto total = sizeOf(json.getProperty("project_file", {}));
    if (const auto* tracks = json.getProperty("tracks", {}).getArray())
        for (const auto& track : *tracks)
            total += sizeOf(track);

    return total;
}
}
