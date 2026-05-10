<![CDATA[
#pragma once
#include "PluginProcessor.h"

class OraclePadAudioProcessorEditor : public juce::AudioProcessorEditor {
public:
    OraclePadAudioProcessorEditor (OraclePadAudioProcessor&);
    ~OraclePadAudioProcessorEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;
private:
    OraclePadAudioProcessor& audioProcessor;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OraclePadAudioProcessorEditor)
};
]]>