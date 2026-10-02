#include "ui/UiFormat.hpp"

namespace stemhub::uiformat
{
namespace
{
bool isKnown(const juce::Time& time)
{
    return time.toMilliseconds() > 0;
}
}

juce::String middleDot() { return juce::String::fromUTF8("\xc2\xb7"); }
juce::String ellipsis() { return juce::String::fromUTF8("\xe2\x80\xa6"); }
juce::String metaSeparator() { return "  " + middleDot() + "  "; }

juce::String twoDigits(const int value)
{
    return juce::String(value).paddedLeft('0', 2);
}

juce::String relativeTime(const juce::Time time, const juce::Time now)
{
    if (!isKnown(time))
        return {};

    const auto seconds = (now - time).inSeconds();
    if (seconds < 60.0)
        return "Just now";
    if (seconds < 3600.0)
        return juce::String(static_cast<int>(seconds / 60.0)) + " min ago";
    if (seconds < 86400.0)
        return juce::String(static_cast<int>(seconds / 3600.0)) + " h ago";
    if (seconds < 2.0 * 86400.0)
        return "Yesterday";
    if (seconds < 7.0 * 86400.0)
        return juce::String(static_cast<int>(seconds / 86400.0)) + " days ago";

    return time.formatted("%d %b %Y");
}

juce::String timestamp(const juce::Time time, const bool withYear)
{
    if (!isKnown(time))
        return "Unknown time";

    return time.formatted(withYear ? "%a %d %b %Y, %H:%M" : "%a %d %b, %H:%M");
}

juce::String versionTitle(const VersionListItem& version)
{
    return version.isUntitled ? juce::String("Untitled version") : version.message.trim();
}

juce::String slug(const juce::String& text)
{
    juce::String result;
    bool previousWasDash = false;

    for (const auto character : text.toLowerCase())
    {
        if ((character >= 'a' && character <= 'z') || (character >= '0' && character <= '9'))
        {
            result += juce::String::charToString(character);
            previousWasDash = false;
        }
        else if (!previousWasDash)
        {
            result += "-";
            previousWasDash = true;
        }
    }

    return result.trimCharactersAtStart("-").trimCharactersAtEnd("-");
}

juce::String workingCopySummary(const bool hasWorkingCopy, const int fileCount, const juce::int64 totalBytes)
{
    if (!hasWorkingCopy)
        return "No working copy";

    if (fileCount < 0)
        return "Counting files" + ellipsis();

    return juce::String(fileCount) + (fileCount == 1 ? " file" : " files") + metaSeparator()
         + juce::File::descriptionOfSizeInBytes(totalBytes);
}

juce::String statusChipText(const Status::Severity severity)
{
    switch (severity)
    {
        case Status::Severity::progress: return "Working" + ellipsis();
        case Status::Severity::success:  return "Done";
        case Status::Severity::warning:  return "Attention";
        case Status::Severity::error:    return "Error";
        case Status::Severity::info:     break;
    }

    return "Ready";
}
}
