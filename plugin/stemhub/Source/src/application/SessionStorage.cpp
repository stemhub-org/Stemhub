#include "application/SessionStorage.hpp"
#include "application/AppFolders.hpp"

namespace
{
// Earlier versions kept a global "last project" and the token in session.json, and, on macOS and
// Windows, their files in another folder. The token there would stay in clear text: signing in
// once writes the new one. Projects restored there are left alone.
void removeLegacyFiles(const juce::File& appData)
{
    const auto legacy = stemhub::folders::legacyAppData();
    legacy.getChildFile("session.json").deleteFile();

    if (legacy == appData)
        return;

    legacy.getChildFile("credentials.json").deleteFile();
    legacy.getChildFile("pending-restore.json").deleteFile();

    const auto legacyConfig = legacy.getChildFile("config.json");
    const auto config = appData.getChildFile("config.json");
    if (legacyConfig.existsAsFile() && !config.exists() && appData.createDirectory().wasOk())
        legacyConfig.moveFileTo(config);
}
}

SessionStorage SessionStorage::forCurrentUser()
{
    const auto appData = stemhub::folders::appData();
    removeLegacyFiles(appData);

    return { std::make_shared<FileCredentialStore>(appData.getChildFile("credentials.json")),
             appData.getChildFile("pending-restore.json"),
             stemhub::folders::restoredProjects(),
             WorkingCopyIndex(appData.getChildFile("working-copies.json")) };
}
