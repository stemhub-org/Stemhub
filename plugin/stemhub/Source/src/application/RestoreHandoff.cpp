#include "application/RestoreHandoff.hpp"

namespace stemhub::handoff
{
namespace
{
// Clocks can be adjusted a little between writing and taking.
const juce::RelativeTime kClockTolerance = juce::RelativeTime::minutes(1);

std::optional<RestoreHandoff> parse(const juce::var& json)
{
    if (!json.isObject())
        return {};

    RestoreHandoff handoff;
    handoff.projectId = json.getProperty("project_id", {}).toString();
    handoff.branchId = json.getProperty("branch_id", {}).toString();

    const auto restoredFilePath = json.getProperty("restored_file", {}).toString();
    if (!juce::File::isAbsolutePath(restoredFilePath))
        return {};

    handoff.copy = { juce::File(restoredFilePath),
                     json.getProperty("version_id", {}).toString(),
                     static_cast<juce::int64>(json.getProperty("size_bytes", -1)),
                     static_cast<juce::int64>(json.getProperty("modified_ms", -1)) };
    handoff.createdAt = juce::Time(static_cast<juce::int64>(json.getProperty("created_ms", 0)));

    if (handoff.projectId.isEmpty() || !handoff.copy.isSet())
        return {};

    return handoff;
}
}

void write(const juce::File& location, const RestoreHandoff& handoff)
{
    auto* object = new juce::DynamicObject();
    object->setProperty("project_id", handoff.projectId);
    object->setProperty("branch_id", handoff.branchId);
    object->setProperty("version_id", handoff.copy.versionId);
    object->setProperty("restored_file", handoff.copy.file.getFullPathName());
    object->setProperty("size_bytes", handoff.copy.sizeBytes);
    object->setProperty("modified_ms", handoff.copy.modTimeMs);
    object->setProperty("created_ms", handoff.createdAt.toMilliseconds());

    if (location.getParentDirectory().createDirectory().wasOk())
        location.replaceWithText(juce::JSON::toString(juce::var(object)));
}

std::optional<RestoreHandoff> take(const juce::File& location, const juce::String& projectId, const juce::Time now)
{
    if (!location.existsAsFile())
        return {};

    const auto handoff = parse(juce::JSON::parse(location.loadFileAsString()));
    const auto isRecent = handoff.has_value()
        && handoff->createdAt <= now + kClockTolerance
        && now - handoff->createdAt < kMaxAge;

    if (!isRecent || !handoff->copy.file.existsAsFile())
    {
        location.deleteFile();
        return {};
    }

    if (projectId.isNotEmpty() && handoff->projectId != projectId)
        return {};

    location.deleteFile();
    return handoff;
}
}
