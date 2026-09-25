#include "network/ApiConfig.hpp"
#include "application/Log.hpp"

#ifndef STEMHUB_DEFAULT_API_BASE_URL
 #define STEMHUB_DEFAULT_API_BASE_URL "http://localhost:8000"
#endif

namespace
{
juce::String withoutTrailingSlashes(juce::String url)
{
    url = url.trim();
    while (url.endsWithChar('/'))
        url = url.dropLastCharacters(1);

    return url;
}

bool isLocalHost(const juce::String& hostAndPort)
{
    const auto host = hostAndPort.startsWithChar('[')
        ? hostAndPort.upToFirstOccurrenceOf("]", true, false)
        : hostAndPort.upToFirstOccurrenceOf(":", false, false);

    return host.equalsIgnoreCase("localhost") || host == "127.0.0.1" || host == "[::1]";
}
}

namespace stemhub::api
{
juce::String normaliseBaseUrl(const juce::String& candidate)
{
    const auto url = withoutTrailingSlashes(candidate);

    if (url.startsWithIgnoreCase("https://") && url.length() > 8)
        return url;

    if (url.startsWithIgnoreCase("http://")
        && isLocalHost(url.substring(7).upToFirstOccurrenceOf("/", false, false)))
        return url;

    return {};
}

juce::String chooseBaseUrl(const juce::String& fromEnvironment,
                           const juce::String& configFileJson,
                           const juce::String& builtInDefault)
{
    const auto fromConfigFile = juce::JSON::parse(configFileJson).getProperty("api_base_url", {}).toString();

    for (const auto& candidate : { fromEnvironment, fromConfigFile })
    {
        if (candidate.trim().isEmpty())
            continue;

        if (const auto url = normaliseBaseUrl(candidate); url.isNotEmpty())
            return url;

        stemhub::log::warning("Ignoring API base URL \"" + candidate + "\": use https, or http to localhost only.");
    }

    return withoutTrailingSlashes(builtInDefault);
}

juce::File getConfigFile()
{
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("Stemhub")
        .getChildFile("config.json");
}

juce::String resolveBaseUrl()
{
    const auto configFile = getConfigFile();
    return chooseBaseUrl(juce::SystemStats::getEnvironmentVariable("STEMHUB_API_BASE_URL", {}),
                         configFile.existsAsFile() ? configFile.loadFileAsString() : juce::String(),
                         STEMHUB_DEFAULT_API_BASE_URL);
}
}
