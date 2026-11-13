#include <algorithm>

#include "domain/VersionHistory.hpp"

namespace stemhub::versionhistory
{
void sortNewestFirst(std::vector<VersionSummary>& versions)
{
    // ISO 8601 timestamps in one format sort as text.
    std::sort(versions.begin(), versions.end(), [](const VersionSummary& lhs, const VersionSummary& rhs)
    {
        return lhs.createdAt > rhs.createdAt;
    });
}

juce::String chooseSelected(const std::vector<VersionSummary>& versions, const juce::String& preferredVersionId)
{
    if (versions.empty())
        return {};

    const auto preferred = std::find_if(versions.begin(), versions.end(), [&preferredVersionId](const VersionSummary& version)
    {
        return preferredVersionId.isNotEmpty() && version.id == preferredVersionId;
    });

    return preferred != versions.end() ? preferred->id : versions.front().id;
}
}
