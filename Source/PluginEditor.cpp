#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
// THE CONSTRUCTOR: Creates the UI elements
OraclePadAudioProcessorEditor::OraclePadAudioProcessorEditor (OraclePadAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p) 
{
    // Configure the Slider
    gainSlider.setSliderStyle(juce::Slider::LinearVertical);
    gainSlider.setRange(0.0, 1.0, 0.01);
    gainSlider.setValue(0.5);
    addAndMakeVisible(gainSlider);

    // Configure the Label
    statusLabel.setText("OraclePad: Standby", juce::dontSendNotification);
    statusLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(statusLabel);

    // Set the window size
    setSize (400, 300);
}

// THE DESTRUCTOR
OraclePadAudioProcessorEditor::~OraclePadAudioProcessorEditor() 
{
}

//==============================================================================
// THE PAINTER: Colors the background
void OraclePadAudioProcessorEditor::paint (juce::Graphics& g) 
{
    g.fillAll (juce::Colours::black); 
}

// THE RESIZER: Places the elements on the screen
void OraclePadAudioProcessorEditor::resized() 
{
    gainSlider.setBounds(20, 20, 50, 260);
    statusLabel.setBounds(100, 130, 200, 40);
}