<![CDATA[
#include "PluginProcessor.h"
#include "PluginEditor.h"

OraclePadAudioProcessor::OraclePadAudioProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)) {}

OraclePadAudioProcessor::~OraclePadAudioProcessor() {}

void OraclePadAudioProcessor::prepareToPlay (double, int) {}

void OraclePadAudioProcessor::releaseResources() {}

void OraclePadAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) {
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();
}

juce::AudioProcessorEditor* OraclePadAudioProcessor::createEditor() {
    return new OraclePadAudioProcessorEditor (*this);
}

void OraclePadAudioProcessor::getStateInformation (juce::MemoryBlock&) {}

void OraclePadAudioProcessor::setStateInformation (const void*, int) {}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new OraclePadAudioProcessor();
}
]]>