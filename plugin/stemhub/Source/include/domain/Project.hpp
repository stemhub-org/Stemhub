#pragma once

#include <JuceHeader.h>

struct Project
{
    juce::String id;
    juce::String name;
    juce::String description;
    juce::String category;
    bool isPublic { false };

    [[nodiscard]] bool isValid() const noexcept
    {
        return !id.isEmpty() && !name.isEmpty();
    }
};
