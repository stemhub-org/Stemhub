#include <algorithm>
#include <mutex>
#include <vector>

#include "application/Log.hpp"
#include "application/WorkingCopyIndex.hpp"

namespace
{
using Entry = WorkingCopyIndex::Entry;

// Enough for every project someone works on; the least recently recorded files drop out first.
constexpr size_t kMaxEntries = 1000;

// juce::InterProcessLock only keeps other processes out on macOS and Linux, not other threads.
std::mutex& inProcessMutex()
{
    static std::mutex mutex;
    return mutex;
}

juce::var toJson(const Entry& entry)
{
    auto* object = new juce::DynamicObject();
    object->setProperty("file", entry.copy.file.getFullPathName());
    object->setProperty("project_id", entry.projectId);
    object->setProperty("branch_id", entry.branchId);
    object->setProperty("version_id", entry.copy.versionId);
    object->setProperty("size_bytes", entry.copy.sizeBytes);
    object->setProperty("modified_ms", entry.copy.modTimeMs);
    return juce::var(object);
}

std::optional<Entry> fromJson(const juce::var& json)
{
    const auto path = json.getProperty("file", {}).toString();
    if (!juce::File::isAbsolutePath(path))
        return {};

    Entry entry;
    entry.projectId = json.getProperty("project_id", {}).toString();
    entry.branchId = json.getProperty("branch_id", {}).toString();
    entry.copy = { juce::File(path),
                   json.getProperty("version_id", {}).toString(),
                   static_cast<juce::int64>(json.getProperty("size_bytes", -1)),
                   static_cast<juce::int64>(json.getProperty("modified_ms", -1)) };

    if (entry.projectId.isEmpty() || !entry.copy.isSet())
        return {};

    return entry;
}

std::vector<Entry> readAll(const juce::File& location)
{
    std::vector<Entry> entries;
    if (!location.existsAsFile())
        return entries;

    // Kept alive while its array is read.
    const auto json = juce::JSON::parse(location.loadFileAsString());
    if (const auto* files = json["files"].getArray())
        for (const auto& file : *files)
            if (auto entry = fromJson(file))
                entries.push_back(std::move(*entry));

    return entries;
}
}

WorkingCopyIndex::WorkingCopyIndex(juce::File indexFile)
    : location(std::move(indexFile))
{
}

std::optional<Entry> WorkingCopyIndex::find(const juce::File& file) const
{
    if (location == juce::File() || file == juce::File())
        return {};

    // Writes replace the whole file at once, so reading needs no lock between processes.
    const std::lock_guard<std::mutex> lock(inProcessMutex());
    for (auto& entry : readAll(location))
        if (entry.copy.file == file)
            return std::move(entry);

    return {};
}

void WorkingCopyIndex::record(const Entry& entry) const
{
    if (location == juce::File() || !entry.copy.isSet())
        return;

    const std::lock_guard<std::mutex> lock(inProcessMutex());
    juce::InterProcessLock otherProcesses("StemhubWorkingCopyIndex");
    const juce::InterProcessLock::ScopedLockType otherProcessesLock(otherProcesses);

    // Files that are missing now stay recorded: they may be on a drive that isn't plugged in.
    auto entries = readAll(location);
    entries.erase(std::remove_if(entries.begin(), entries.end(), [&entry](const Entry& recorded)
    {
        return recorded.copy.file == entry.copy.file;
    }), entries.end());

    entries.insert(entries.begin(), entry);
    if (entries.size() > kMaxEntries)
        entries.resize(kMaxEntries);

    juce::Array<juce::var> files;
    for (const auto& recorded : entries)
        files.add(toJson(recorded));

    auto* root = new juce::DynamicObject();
    root->setProperty("files", files);

    if (!location.getParentDirectory().createDirectory().wasOk()
        || !location.replaceWithText(juce::JSON::toString(juce::var(root))))
        stemhub::log::warning("Couldn't record which version " + entry.copy.file.getFullPathName() + " holds in "
                              + location.getFullPathName());
}
