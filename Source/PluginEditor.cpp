<![CDATA[
#include "PluginProcessor.h"
#include "PluginEditor.h"

OraclePadAudioProcessorEditor::OraclePadAudioProcessorEditor (OraclePadAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p) { setSize (400, 300); }

OraclePadAudioProcessorEditor::~OraclePadAudioProcessorEditor() {}

void OraclePadAudioProcessorEditor::paint (juce::Graphics& g) {
    g.fillAll (juce::Colours::black);
    g.setColour (juce::Colours::cyan);
    g.setFont (24.0f);
    g.drawFittedText ("ORACLE PAD", getLocalBounds(), juce::Justification::centred, 1);
}

void OraclePadAudioProcessorEditor::resized() {}
]]>