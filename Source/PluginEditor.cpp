#include "PluginProcessor.h"
#include "PluginEditor.h"

OraclePadAudioProcessorEditor::OraclePadAudioProcessorEditor (OraclePadAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    gainSlider.setSliderStyle(juce::Slider::LinearVertical);
    gainSlider.setRange(0.0, 1.0, 0.01);
    gainSlider.setValue(0.5);
    addAndMakeVisible(gainSlider);

    statusLabel.setText("OraclePad: Standby", juce::dontSendNotification);
    statusLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(statusLabel);

    setSize (400, 300);
}

OraclePadAudioProcessorEditor::~OraclePadAudioProcessorEditor() {}

void OraclePadAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black);
}

void OraclePadAudioProcessorEditor::resized()
{
    gainSlider.setBounds(20, 20, 50, 260);
    statusLabel.setBounds(100, 130, 200, 40);
}