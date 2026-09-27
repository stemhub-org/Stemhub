#include <algorithm>
#include <array>
#include <map>

#include "application/SnapshotFiles.hpp"
#include "network/ApiTypes.hpp"

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

bool isRestoredCopy(const juce::File& folder)
{
    return folder.getChildFile(kRestoredCopyMarker).existsAsFile();
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

// Feeds a file to juce::SHA256, which asks for 64 bytes at a time: reading the file itself that
// way took a system call per 64 bytes. The file is read in large blocks instead, and the stream
// ends early, once per block, when the job running it is asked to stop.
class HashInput final : public juce::InputStream
{
public:
    explicit HashInput(juce::InputStream& sourceToRead)
        : source(sourceToRead, kBlockBytes)
    {
    }

    juce::int64 getTotalLength() override { return source.getTotalLength(); }
    bool isExhausted() override { return stopped || source.isExhausted(); }
    juce::int64 getPosition() override { return source.getPosition(); }
    bool setPosition(juce::int64 newPosition) override { return source.setPosition(newPosition); }

    int read(void* destBuffer, int maxBytesToRead) override
    {
        bytesSinceCheck += maxBytesToRead;
        if (bytesSinceCheck >= kBlockBytes)
        {
            bytesSinceCheck = 0;
            stopped = stopped || isJobCancelled();
        }

        return stopped ? 0 : source.read(destBuffer, maxBytesToRead);
    }

    [[nodiscard]] bool wasStopped() const noexcept { return stopped; }

private:
    static constexpr int kBlockBytes = 1 << 20;

    juce::BufferedInputStream source;
    int bytesSinceCheck { kBlockBytes }; // checks before the first block too
    bool stopped { false };
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
        // A large folder takes a while to walk: a job asked to stop gets nothing.
        if (isJobCancelled())
            return {};

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

juce::String sha256OfFile(const juce::File& file)
{
    juce::FileInputStream fileStream(file);
    if (!fileStream.openedOk())
        return {};

    HashInput input(fileStream);
    const auto hash = juce::SHA256(input).toHexString();
    return input.wasStopped() ? juce::String() : hash;
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
