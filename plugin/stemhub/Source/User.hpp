#pragma once

#include <functional>
#include <string>
#include <utility>
#include <vector>
#include "States.hpp"
#pragma once

#include <JuceHeader.h>

struct User {
    juce::String id;
    juce::String email;
    juce::String username;

    [[nodiscard]] bool isValid() const noexcept {
        return !id.isEmpty() && !email.isEmpty() && !username.isEmpty();
    }
};
