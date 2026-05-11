#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"

class OraclePadAudioProcessorEditor : public juce::AudioProcessorEditor 
{
public:
    OraclePadAudioProcessorEditor (OraclePadAudioProcessor&);
    ~OraclePadAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    OraclePadAudioProcessor& audioProcessor;

    // UI Components
    juce::Slider gainSlider;
    juce::Label  statusLabel;
    
    // Section Labels
    juce::Label bannerLabel;
    juce::Label osc1Label;
    juce::Label osc2Label;
    juce::Label arpLabel;
    juce::Label radarLabel;
    juce::Label globalSettingsLabel;

    juce::Colour cyberBlue = juce::Colour(0xFF00F0FF);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OraclePadAudioProcessorEditor)
};