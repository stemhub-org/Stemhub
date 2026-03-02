#include "PluginProcessor.hpp"
#include "PluginEditor.hpp"

namespace
{
    void drawCenteredMessage (juce::Graphics& g, const juce::String& message) {
        g.setFont (juce::FontOptions (20.0f, juce::Font::bold));
        g.drawFittedText (message, g.getClipBounds(), juce::Justification::centred, 2);
    }

    void drawSignedOutDisplay (juce::Graphics& g) {
        drawCenteredMessage (g, "Please sign in to your Stemhub account to access your projects.");
    }

    void drawSignedInDisplay (juce::Graphics& g, const StemhubAudioProcessor& audioProcessor) {
        drawCenteredMessage (g, "Welcome back " + audioProcessor.getUsername() + "!");
    }

    void drawAuthErrorDisplay (juce::Graphics& g) {
        drawCenteredMessage (g, "An error occurred during authentication. Please try again.");
    }
}

StemhubAudioProcessorEditor::StemhubAudioProcessorEditor (StemhubAudioProcessor& processorToEdit)
    : AudioProcessorEditor (&processorToEdit), audioProcessor (processorToEdit)
{
    setSize (600, 400);
    addAndMakeVisible (signedOutButton);
    addAndMakeVisible (signingInButton);
    addAndMakeVisible (signedInButton);
    addAndMakeVisible (authErrorButton);

    signedOutButton.onClick = [this] {
        audioProcessor.clearSession();
        repaint();
    };
    signingInButton.onClick = [this] {
        audioProcessor.setAuthState (AuthState::signingIn);
        repaint();
    };
    signedInButton.onClick = [this] {
        User user;
        user.id = "12345";
        user.email = "test@stemhub.dev";
        user.username = "Test User";

        audioProcessor.setCurrentUser (user);
        audioProcessor.setAuthState (AuthState::signedIn);
        repaint();
    };
    authErrorButton.onClick = [this] {
        audioProcessor.setAuthState (AuthState::authError);
        repaint();
    };
}

void StemhubAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));
    g.setColour (juce::Colours::white);

    switch (audioProcessor.getAuthState()) {
        case AuthState::signedOut:
            drawSignedOutDisplay (g);
            return;

        case AuthState::signingIn:
            drawCenteredMessage (g, "Connecting to Stemhub...");
            return;

        case AuthState::signedIn:
            drawSignedInDisplay (g, audioProcessor);
            return;

        case AuthState::authError:
            drawAuthErrorDisplay (g);
            return;
    }
}

void StemhubAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (20);

    signedOutButton.setBounds (area.removeFromTop (30));
    area.removeFromTop (8);

    signingInButton.setBounds (area.removeFromTop (30));
    area.removeFromTop (8);

    signedInButton.setBounds (area.removeFromTop (30));
    area.removeFromTop (8);

    authErrorButton.setBounds (area.removeFromTop (30));
}
