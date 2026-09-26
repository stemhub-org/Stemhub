#pragma once

#include <JuceHeader.h>

// A message for the user and how to present it. Set where the event happens, so nothing has to
// guess later whether a text means success or failure.
struct Status
{
    enum class Severity
    {
        info,
        progress,
        success,
        warning,
        error
    };

    Severity severity { Severity::info };
    juce::String text;

    static Status info(juce::String message) { return { Severity::info, std::move(message) }; }
    static Status progress(juce::String message) { return { Severity::progress, std::move(message) }; }
    static Status success(juce::String message) { return { Severity::success, std::move(message) }; }
    static Status warning(juce::String message) { return { Severity::warning, std::move(message) }; }
    static Status error(juce::String message) { return { Severity::error, std::move(message) }; }

    [[nodiscard]] bool isEmpty() const noexcept { return text.isEmpty(); }
    [[nodiscard]] bool isError() const noexcept { return severity == Severity::error; }
};
