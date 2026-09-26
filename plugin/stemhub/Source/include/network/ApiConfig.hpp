#pragma once

#include <JuceHeader.h>

namespace stemhub::api
{
// The StemHub API the plugin talks to. First usable value wins:
//   1. STEMHUB_API_BASE_URL in the environment (development builds run from a shell),
//   2. "api_base_url" in the config file below (DAWs don't inherit a shell's environment),
//   3. the URL the plugin was built with (CMake option STEMHUB_API_BASE_URL).
juce::String resolveBaseUrl();

// <app data>/Stemhub/config.json
juce::File getConfigFile();

// The rules of resolveBaseUrl() without reading the environment or the disk.
juce::String chooseBaseUrl(const juce::String& fromEnvironment,
                           const juce::String& configFileJson,
                           const juce::String& builtInDefault);

// https, or http to this machine only (the bearer token must never travel in clear), without
// trailing slashes. Empty when the value isn't usable.
juce::String normaliseBaseUrl(const juce::String& candidate);
}
