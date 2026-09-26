#include <algorithm>
#include <array>
#include <map>

#include "application/SnapshotFiles.hpp"
#include "application/SessionHelpers.hpp"

namespace stemhub::snapshotfiles
{
namespace
{
constexpr std::array<const char*, 9> kAssetExtensions { "wav", "mp3", "flac", "ogg", "aiff", "aif", "m4a", "mid", "midi" };

// Dot-files are left out on every system, not only where the OS hides them: macOS writes
// "._kick.wav" companions onto drives that Windows then sees as ordinary files.
bool isDotFile(const juce::File& file)
{
    return file.getFileName().startsWithChar('.');
}

bool isAsset(const juce::File& file)
{
    return std::any_of(kAssetExtensions.begin(), kAssetExtensions.end(), [&file](const char* extension)
    {
        return file.hasFileExtension(extension);
    });
}

// A folder this plugin restored a version into: its name ends in the version's id prefix.
bool isRestoredCopy(const juce::File& folder)
{
    if (stemhub::sessionhelpers::extractVersionPrefixFromPathPart(folder.getFileName()).isEmpty())
        return false;

    for (const auto& entry : juce::RangedDirectoryIterator(folder, false, "*", juce::File::findFiles))
        if (isDawProjectFile(entry.getFile()))
            return true;

    return false;
}

class FolderRules
{
public:
    explicit FolderRules(juce::File rootFolder) : root(std::move(rootFolder)) {}

    bool isLeftOut(const juce::File& folder)
    {
        for (auto current = folder; current != root && current.isAChildOf(root); current = current.getParentDirectory())
        {
            const auto [cached, isNew] = leftOutByPath.try_emplace(current.getFullPathName(), false);
            if (isNew)
                cached->second = isDotFile(current) || current.getFileName().equalsIgnoreCase("backup")
                              || isRestoredCopy(current);

            if (cached->second)
                return true;
        }

        return false;
    }

private:
    juce::File root;
    std::map<juce::String, bool> leftOutByPath;
};
}

std::vector<juce::File> collect(const juce::File& projectFile)
{
    std::vector<juce::File> files;
    if (!projectFile.existsAsFile())
        return files;

    const auto root = projectFile.getParentDirectory();
    FolderRules rules(root);

    for (const auto& entry : juce::RangedDirectoryIterator(root,
                                                           true,
                                                           "*",
                                                           juce::File::findFiles | juce::File::ignoreHiddenFiles,
                                                           juce::File::FollowSymlinks::noCycles))
    {
        const auto file = entry.getFile();
        if (file != projectFile && isAsset(file) && !isDotFile(file) && !rules.isLeftOut(file.getParentDirectory()))
            files.push_back(file);
    }

    std::sort(files.begin(), files.end(), [](const juce::File& lhs, const juce::File& rhs)
    {
        return lhs.getFullPathName() < rhs.getFullPathName();
    });
    files.insert(files.begin(), projectFile);
    return files;
}

Summary summarize(const juce::File& projectFile)
{
    Summary summary { projectFile };
    for (const auto& file : collect(projectFile))
    {
        ++summary.fileCount;
        summary.totalBytes += file.getSize();
    }

    return summary;
}

bool isDawProjectFile(const juce::File& file)
{
    return file.hasFileExtension("flp") || file.hasFileExtension("als");
}

juce::String dawNameFor(const juce::File& projectFile)
{
    if (projectFile.hasFileExtension("flp"))
        return "FL Studio";

    if (projectFile.hasFileExtension("als"))
        return "Ableton Live";

    return {};
}
}
