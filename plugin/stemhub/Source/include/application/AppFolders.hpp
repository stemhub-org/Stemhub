#pragma once

#include <JuceHeader.h>

// Where the plugin keeps files on this machine.
namespace stemhub::folders
{
// The plugin's own files for this user: the token, the restore hand-off, the working-copy record
// and config.json. ~/Library/Application Support/Stemhub on macOS, %LOCALAPPDATA%\Stemhub on
// Windows (not the roaming profile: the files name paths on this machine), ~/.config/Stemhub
// elsewhere.
juce::File appData();

// The folder earlier versions used instead of appData(), or appData() itself where it hasn't
// moved: ~/Library/Stemhub on macOS, %APPDATA%\Stemhub on Windows.
juce::File legacyAppData();

// Where opening a project from the grid restores its latest version: Documents/StemHub, where
// the user can find the project and keep it.
juce::File restoredProjects();
}
