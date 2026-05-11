#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"

class OraclePadAudioProcessorEditor : public juce::AudioProcessorEditor {
public:
    OraclePadAudioProcessorEditor (OraclePadAudioProcessor&);
    ~OraclePadAudioProcessorEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;
private:
    juce::Slider gainSlider;  // Add this
    juce::Label  statusLabel; // Add this
    
    OraclePadAudioProcessor& audioProcessor;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OraclePadAudioProcessorEditor)
};