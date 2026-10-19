#include <algorithm>

#include "application/RestoreFolders.hpp"

namespace
{
// Letters, digits, '-', '_' and '.', with anything else turned into single dashes.
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

    while (output.contains("--"))
        output = output.replace("--", "-");
    output = output.trimCharactersAtStart("-").trimCharactersAtEnd("-");
    return output.isNotEmpty() ? output : fallback;
}

juce::String folderNameFor(const juce::String& name, const juce::String& id, const juce::String& fallback)
{
    const auto legalName = juce::File::createLegalFileName(name).trim().trimCharactersAtStart(".").trimCharactersAtEnd(".");
    return legalName.isNotEmpty() ? legalName : sanitizePathSegment(id, fallback);
}
}

namespace stemhub::restorefolders
{
juce::String projectName(const std::vector<VersionSummary>& versions,
                         const juce::String& versionId,
                         const juce::String& fallbackName)
{
    const auto it = std::find_if(versions.begin(), versions.end(), [&versionId](const VersionSummary& version)
    {
        return version.id == versionId;
    });

    if (it != versions.end() && it->sourceProjectFilename.isNotEmpty())
    {
        // Server-provided and possibly a path: keep the last segment, without its extension.
        const auto sourceProjectName = juce::File::createLegalFileName(
            it->sourceProjectFilename.replaceCharacter('\\', '/')
                .fromLastOccurrenceOf("/", false, false)
                .upToLastOccurrenceOf(".", false, false)).trim();
        if (sourceProjectName.isNotEmpty())
            return sourceProjectName;
    }

    const auto legalFallbackName = juce::File::createLegalFileName(fallbackName).trim();
    return legalFallbackName.isNotEmpty() ? legalFallbackName : "restored-project";
}

juce::File newFolder(const juce::File& parent, const juce::String& projectName, const juce::String& versionId)
{
    const auto folderName = juce::File::createLegalFileName(
        projectName + "-" + versionId.substring(0, juce::jmin(8, versionId.length())));

    auto folder = parent.getChildFile(folderName);
    for (int copyNumber = 2; folder.exists(); ++copyNumber)
        folder = parent.getChildFile(folderName + " (" + juce::String(copyNumber) + ")");

    return folder;
}

juce::File projectRoot(const juce::File& baseFolder, const Project& project, const Branch& branch)
{
    return baseFolder.getChildFile(folderNameFor(project.name, project.id, "project"))
                     .getChildFile(folderNameFor(branch.name, branch.id, "branch"));
}
}
