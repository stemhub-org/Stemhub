#include "PluginProcessor.h"
#include "PluginEditor.h"

StemhubAudioProcessorEditor::StemhubAudioProcessorEditor (StemhubAudioProcessor& audioProcessor)
    : AudioProcessorEditor (&audioProcessor)
{
    setSize (400, 300);
}

void StemhubAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));
    g.setColour (juce::Colours::white);
    g.setFont (juce::FontOptions (15.0f));
    g.drawFittedText ("Hello World!", getLocalBounds(), juce::Justification::centred, 1);
}

void StemhubAudioProcessorEditor::resized()
{
}
