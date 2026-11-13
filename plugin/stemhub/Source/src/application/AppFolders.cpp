#include "application/AppFolders.hpp"

namespace stemhub::folders
{
juce::File appData()
{
   #if JUCE_MAC
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("Application Support")
        .getChildFile("Stemhub");
   #elif JUCE_WINDOWS
    return juce::File::getSpecialLocation(juce::File::windowsLocalAppData).getChildFile("Stemhub");
   #else
    return legacyAppData();
   #endif
}

juce::File legacyAppData()
{
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory).getChildFile("Stemhub");
}

juce::File restoredProjects()
{
    return juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("StemHub");
}
}
