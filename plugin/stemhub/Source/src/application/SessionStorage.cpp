#include "application/SessionStorage.hpp"
#include "application/ProjectFileService.hpp"

SessionStorage SessionStorage::forCurrentUser()
{
    const auto folder = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory).getChildFile("Stemhub");

    // Earlier versions kept the token in session.json, next to a global "last project" that each
    // DAW project's own link replaces. Signing in once writes the new store.
    folder.getChildFile("session.json").deleteFile();

    return { std::make_shared<FileCredentialStore>(folder.getChildFile("credentials.json")),
             folder.getChildFile("pending-restore.json"),
             stemhub::projectfiles::getDefaultManagedWorkingCopyFolder() };
}
