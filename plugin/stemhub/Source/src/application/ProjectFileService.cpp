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
}

namespace stemhub::projectfiles
{
juce::String resolveRestoreProjectName(const std::vector<VersionSummary>& versions,
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

juce::File chooseRestoreFolder(const juce::File& parent,
                               const juce::String& projectName,
                               const juce::String& versionId)
{
    const auto folderName = juce::File::createLegalFileName(
        projectName + "-" + versionId.substring(0, juce::jmin(8, versionId.length())));

    auto folder = parent.getChildFile(folderName);
    for (int copyNumber = 2; folder.exists(); ++copyNumber)
        folder = parent.getChildFile(folderName + " (" + juce::String(copyNumber) + ")");

    return folder;
}

juce::File getRestoredProjectRoot(const juce::File& baseFolder, const Project& project, const Branch& branch)
{
    const auto folderName = [](const juce::String& name, const juce::String& id, const juce::String& fallback)
    {
        const auto legalName = juce::File::createLegalFileName(name).trim().trimCharactersAtStart(".").trimCharactersAtEnd(".");
        return legalName.isNotEmpty() ? legalName : sanitizePathSegment(id, fallback);
    };

    return baseFolder.getChildFile(folderName(project.name, project.id, "project"))
                     .getChildFile(folderName(branch.name, branch.id, "branch"));
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

bool openInSystem(const juce::File& file)
{
    if (!file.existsAsFile())
        return false;

    if (file.startAsProcess())
        return true;

   #if JUCE_MAC
    // Arguments go as they are: no quoting to get wrong with spaces or quotes in the path.
    juce::ChildProcess openProcess;
    return openProcess.start(juce::StringArray { "open", file.getFullPathName() });
   #else
    return false;
   #endif
}
}
