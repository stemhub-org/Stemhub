#pragma once

#include <optional>

#include <JuceHeader.h>

#include "domain/WorkingCopyBaseline.hpp"

// Which version each DAW project file on this machine holds, as far as the plugin knows. A file
// is recorded when the plugin saves it as a version or restores a version into it, with its size
// and modification time then. The record outlives the plugin instance: when the DAW reopens the
// project, the next save still builds on the version the file came from, and an unchanged file is
// still known to be unchanged.
//
// One JSON file shared by every instance on the machine, so any thread may call it: writes are
// serialised, between processes too, and replace the file atomically.
class WorkingCopyIndex
{
public:
    struct Entry
    {
        juce::String projectId;
        juce::String branchId;
        WorkingCopyBaseline copy;
    };

    // An index with no file keeps nothing.
    explicit WorkingCopyIndex(juce::File indexFile = {});

    // What was recorded for file, if anything.
    [[nodiscard]] std::optional<Entry> find(const juce::File& file) const;

    // Replaces what was recorded for entry.copy.file. Only the files recorded most recently are
    // kept (1000 of them).
    void record(const Entry& entry) const;

private:
    juce::File location;
};
