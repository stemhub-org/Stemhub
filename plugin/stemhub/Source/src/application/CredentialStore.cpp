#include "application/CredentialStore.hpp"

#if ! JUCE_WINDOWS
 #include <sys/stat.h>
#endif

namespace
{
void restrictToCurrentUser(const juce::File& fileOrFolder, const bool isFolder)
{
   #if JUCE_WINDOWS
    juce::ignoreUnused(fileOrFolder, isFolder);
   #else
    ::chmod(fileOrFolder.getFullPathName().toRawUTF8(), isFolder ? S_IRWXU : (S_IRUSR | S_IWUSR));
   #endif
}
}

FileCredentialStore::FileCredentialStore(juce::File fileToUse)
    : file(std::move(fileToUse))
{
}

juce::String FileCredentialStore::loadToken() const
{
    if (!file.existsAsFile())
        return {};

    return juce::JSON::parse(file.loadFileAsString()).getProperty("access_token", {}).toString().trim();
}

void FileCredentialStore::saveToken(const juce::String& token)
{
    const auto folder = file.getParentDirectory();
    if (folder.createDirectory().failed())
        return;

    // Set before writing, so the token is never in a folder others can open.
    restrictToCurrentUser(folder, true);

    auto* object = new juce::DynamicObject();
    object->setProperty("access_token", token);
    if (file.replaceWithText(juce::JSON::toString(juce::var(object))))
        restrictToCurrentUser(file, false);
}

void FileCredentialStore::clear()
{
    file.deleteFile();
}
