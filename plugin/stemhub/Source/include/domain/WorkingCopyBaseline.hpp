#pragma once

#include <JuceHeader.h>

// The version a local project file holds, plus the file's size and modification time when that
// was recorded, so the plugin can tell later whether the file changed.
struct WorkingCopyBaseline
{
    juce::File file;
    juce::String versionId;
    // -1 when unknown, e.g. when the version was read from a restore folder's name rather than
    // recorded while this plugin wrote or pushed the file.
    juce::int64 sizeBytes { -1 };
    juce::int64 modTimeMs { -1 };

    // For a file this plugin has just pushed or written.
    static WorkingCopyBaseline recordedNow(const juce::File& projectFile, const juce::String& version)
    {
        return { projectFile, version, projectFile.getSize(), projectFile.getLastModificationTime().toMilliseconds() };
    }

    [[nodiscard]] bool isSet() const noexcept
    {
        return versionId.isNotEmpty() && file != juce::File();
    }

    // Size and modification time were recorded, so isUnchanged() can give a real answer.
    [[nodiscard]] bool hasRecordedState() const noexcept
    {
        return isSet() && sizeBytes >= 0;
    }

    [[nodiscard]] bool describes(const juce::File& otherFile) const noexcept
    {
        return isSet() && file == otherFile;
    }

    // True only while the file provably still is that version. An unknown state counts as
    // changed, so a save is never refused because of a guess.
    [[nodiscard]] bool isUnchanged() const
    {
        return hasRecordedState()
            && file.existsAsFile()
            && file.getSize() == sizeBytes
            && file.getLastModificationTime().toMilliseconds() == modTimeMs;
    }
};
