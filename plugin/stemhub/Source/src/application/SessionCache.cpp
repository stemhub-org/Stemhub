#include "application/SessionCache.hpp"

namespace
{
juce::File resolveSessionCacheFile()
{
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("Stemhub")
        .getChildFile("session.json");
}

juce::var loadCachedSessionJson()
{
    const auto sessionFile = resolveSessionCacheFile();
    if (!sessionFile.existsAsFile())
        return {};

    return juce::JSON::parse(sessionFile.loadFileAsString());
}

void saveCachedSession(const juce::String& token, const juce::String& projectId)
{
    const auto sessionFile = resolveSessionCacheFile();
    const auto sessionDirectory = sessionFile.getParentDirectory();
    if ((!sessionDirectory.exists() && !sessionDirectory.createDirectory()) || !sessionDirectory.isDirectory())
        return;

    juce::DynamicObject::Ptr sessionObject = new juce::DynamicObject();
    sessionObject->setProperty("access_token", token);
    sessionObject->setProperty("last_project_id", projectId);

    const auto sessionJson = juce::JSON::toString(juce::var(sessionObject.get()), true);
    sessionFile.replaceWithText(sessionJson);
}
}

namespace stemhub::sessioncache
{
juce::String loadAccessToken()
{
    const auto jsonValue = loadCachedSessionJson();
    const auto* sessionObject = jsonValue.getDynamicObject();
    if (sessionObject == nullptr)
        return {};

    return sessionObject->getProperty("access_token").toString();
}

juce::String loadProjectId()
{
    const auto jsonValue = loadCachedSessionJson();
    const auto* sessionObject = jsonValue.getDynamicObject();
    if (sessionObject == nullptr)
        return {};

    return sessionObject->getProperty("last_project_id").toString();
}

void saveAccessToken(const juce::String& token)
{
    saveCachedSession(token, loadProjectId());
}

void saveProjectId(const juce::String& projectId)
{
    saveCachedSession(loadAccessToken(), projectId);
}

void clear()
{
    const auto sessionFile = resolveSessionCacheFile();
    if (sessionFile.existsAsFile())
        sessionFile.deleteFile();
}
}

