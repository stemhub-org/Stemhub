#include "application/SessionCache.hpp"

namespace
{
juce::File cacheFileOverride;
bool useInMemoryCacheOverride = false;
juce::var inMemoryCacheOverride;

juce::File resolveSessionCacheFile()
{
    if (cacheFileOverride.getFullPathName().isNotEmpty())
        return cacheFileOverride;

    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("Stemhub")
        .getChildFile("session.json");
}

juce::var loadCachedSessionJson()
{
    if (useInMemoryCacheOverride)
        return inMemoryCacheOverride;

    const auto sessionFile = resolveSessionCacheFile();
    if (!sessionFile.existsAsFile())
        return {};

    const auto parsed = juce::JSON::parse(sessionFile.loadFileAsString());
    return parsed;
}

void updateCachedSession(const std::function<void(juce::DynamicObject&)>& updater)
{
    if (useInMemoryCacheOverride)
    {
        const auto existing = loadCachedSessionJson();
        juce::DynamicObject::Ptr sessionObject;
        if (auto* object = existing.getDynamicObject(); object != nullptr)
            sessionObject = object;
        else
            sessionObject = new juce::DynamicObject();

        updater(*sessionObject);
        inMemoryCacheOverride = juce::var(sessionObject.get());
        return;
    }

    const auto sessionFile = resolveSessionCacheFile();
    const auto sessionDirectory = sessionFile.getParentDirectory();
    if ((!sessionDirectory.exists() && !sessionDirectory.createDirectory()) || !sessionDirectory.isDirectory())
        return;

    const auto existing = loadCachedSessionJson();
    juce::DynamicObject::Ptr sessionObject;
    if (auto* object = existing.getDynamicObject(); object != nullptr)
        sessionObject = object;
    else
        sessionObject = new juce::DynamicObject();

    updater(*sessionObject);

    const auto sessionJson = juce::JSON::toString(juce::var(sessionObject.get()), true);
    sessionFile.replaceWithText(sessionJson);
}

}

namespace stemhub::sessioncache
{
juce::String loadLastOpenedProjectFilePath()
{
    const auto jsonValue = loadCachedSessionJson();
    const auto* sessionObject = jsonValue.getDynamicObject();
    if (sessionObject == nullptr)
        return {};

    return sessionObject->getProperty("last_opened_project_file").toString();
}

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
    updateCachedSession([&token](juce::DynamicObject& sessionObject)
    {
        sessionObject.setProperty("access_token", token);
    });
}

void saveProjectId(const juce::String& projectId)
{
    updateCachedSession([&projectId](juce::DynamicObject& sessionObject)
    {
        sessionObject.setProperty("last_project_id", projectId);
    });
}

void saveLastOpenedProjectFilePath(const juce::String& projectFilePath)
{
    updateCachedSession([&projectFilePath](juce::DynamicObject& sessionObject)
    {
        sessionObject.setProperty("last_opened_project_file", projectFilePath);
    });
}

void clearProjectId()
{
    updateCachedSession([](juce::DynamicObject& sessionObject)
    {
        sessionObject.removeProperty("last_project_id");
    });
}

void clearLastOpenedProjectFilePath()
{
    updateCachedSession([](juce::DynamicObject& sessionObject)
    {
        sessionObject.removeProperty("last_opened_project_file");
    });
}

void clearProjectContext()
{
    updateCachedSession([](juce::DynamicObject& sessionObject)
    {
        sessionObject.removeProperty("last_project_id");
        sessionObject.removeProperty("last_opened_project_file");
    });
}

void setCacheFileOverrideForTesting(const juce::File& file)
{
    cacheFileOverride = file;
    useInMemoryCacheOverride = true;
    inMemoryCacheOverride = juce::var();
}

void clearCacheFileOverrideForTesting()
{
    cacheFileOverride = juce::File();
    useInMemoryCacheOverride = false;
    inMemoryCacheOverride = juce::var();
}

void clear()
{
    if (useInMemoryCacheOverride)
    {
        inMemoryCacheOverride = juce::var();
        return;
    }

    const auto sessionFile = resolveSessionCacheFile();
    if (sessionFile.existsAsFile())
        sessionFile.deleteFile();
}
}
