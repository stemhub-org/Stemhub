#pragma once

#include <JuceHeader.h>

struct Branch
{
    juce::String id;
    juce::String projectId;
    juce::String name;
    bool isDeleted { false };

    [[nodiscard]] bool isValid() const noexcept
    {
        return !id.isEmpty() && !projectId.isEmpty() && !name.isEmpty();
    }
};
