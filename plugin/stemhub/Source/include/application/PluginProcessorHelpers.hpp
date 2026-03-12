#pragma once

#include <algorithm>
#include <optional>
#include <vector>

#include <JuceHeader.h>

#include "application/VersionControlService.hpp"
#include "network/ApiUtils.hpp"

namespace stemhub::processorhelpers
{
template <typename ResultType>
bool hasError(const ResultType& result)
{
    return !result.errorMessage.isEmpty();
}

inline void sortVersionHistoryNewestFirst(std::vector<VersionSummary>& versions)
{
    std::sort(versions.begin(), versions.end(), [](const VersionSummary& lhs, const VersionSummary& rhs)
    {
        return lhs.createdAt > rhs.createdAt;
    });
}

inline juce::String chooseSelectedVersionId(const std::vector<VersionSummary>& versions,
                                            const juce::String& preferredVersionId)
{
    if (versions.empty())
        return {};

    if (preferredVersionId.isNotEmpty())
    {
        const auto it = std::find_if(versions.begin(), versions.end(), [&preferredVersionId](const VersionSummary& version)
        {
            return version.id == preferredVersionId;
        });

        if (it != versions.end())
            return it->id;
    }

    return versions.front().id;
}

inline juce::String extractVersionPrefixFromPathPart(const juce::String& value)
{
    const auto lastDash = value.lastIndexOfChar('-');
    if (lastDash < 0 || lastDash + 8 >= value.length())
        return {};

    const auto candidate = value.substring(lastDash + 1, lastDash + 9).toLowerCase();
    for (int i = 0; i < candidate.length(); ++i)
    {
        const auto ch = candidate[i];
        const auto isHexChar = (ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f');
        if (!isHexChar)
            return {};
    }

    return candidate;
}

inline bool hasVersionHintInProjectPath(const juce::File& projectFile)
{
    if (!projectFile.existsAsFile())
        return false;

    const auto fileHint = extractVersionPrefixFromPathPart(projectFile.getFileNameWithoutExtension());
    const auto parentHint = extractVersionPrefixFromPathPart(projectFile.getParentDirectory().getFileName());
    return fileHint.isNotEmpty() || parentHint.isNotEmpty();
}

inline juce::String resolveVersionIdFromProjectPath(const juce::File& projectFile,
                                                    const std::vector<VersionSummary>& versions)
{
    if (!projectFile.existsAsFile() || versions.empty())
        return {};

    const juce::String pathParts[] = {
        projectFile.getFileNameWithoutExtension(),
        projectFile.getParentDirectory().getFileName()
    };

    for (const auto& part : pathParts)
    {
        const auto prefix = extractVersionPrefixFromPathPart(part);
        if (prefix.isEmpty())
            continue;

        const auto it = std::find_if(versions.begin(), versions.end(), [&prefix](const VersionSummary& version)
        {
            if (version.id.length() < 8)
                return false;

            return version.id.substring(0, 8).compareIgnoreCase(prefix) == 0;
        });

        if (it != versions.end())
            return it->id;
    }

    return {};
}

inline bool hasProjectAndBranchSelected(const std::optional<Project>& project, const juce::String& branchId)
{
    return project.has_value() && branchId.isNotEmpty();
}

inline ProjectVersionContext makeProjectVersionContext(const std::optional<Project>& project,
                                                       const juce::String& branchId,
                                                       const std::vector<VersionSummary>& versions)
{
    ProjectVersionContext context;
    context.projectId = project ? project->id : juce::String();
    context.branchId = branchId;
    context.lastVersionId = versions.empty() ? juce::String() : versions.front().id;
    return context;
}
}
