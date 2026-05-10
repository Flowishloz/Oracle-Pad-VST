#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
// THE CONSTRUCTOR & DESTRUCTOR
OraclePadAudioProcessor::OraclePadAudioProcessor()
     : AudioProcessor (BusesProperties()
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
}

OraclePadAudioProcessor::~OraclePadAudioProcessor()
{
}

//==============================================================================
// AUDIO ENGINE FUNCTIONS
void OraclePadAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
}

void OraclePadAudioProcessor::releaseResources()
{
}

void OraclePadAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());
}

//==============================================================================
// CONNECTING TO THE UI
juce::AudioProcessorEditor* OraclePadAudioProcessor::createEditor()
{
    return new OraclePadAudioProcessorEditor (*this);
}

//==============================================================================
// PRESETS (SAVE/LOAD)
void OraclePadAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
}

void OraclePadAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
}

//==============================================================================
// THE IGNITION SWITCH (Crucial for the Linker)
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new OraclePadAudioProcessor();
}