#pragma once

#include <JuceHeader.h>

namespace stemhub::sessioncache
{
juce::String loadAccessToken();
juce::String loadProjectId();
void saveAccessToken(const juce::String& token);
void saveProjectId(const juce::String& projectId);
void clear();
}

