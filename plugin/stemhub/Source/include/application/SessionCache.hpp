#pragma once

#include <JuceHeader.h>

namespace stemhub::sessioncache
{
juce::String loadAccessToken();
juce::String loadProjectId();
juce::String loadLastOpenedProjectFilePath();
void saveAccessToken(const juce::String& token);
void saveProjectId(const juce::String& projectId);
void saveLastOpenedProjectFilePath(const juce::String& projectFilePath);
void clear();
}
