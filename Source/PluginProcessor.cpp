#include "PluginProcessor.h"
#include "PluginEditor.h"

OraclePadAudioProcessor::OraclePadAudioProcessor()
     : AudioProcessor (BusesProperties()
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
       apvts (*this, nullptr, "Parameters", 
       {
           std::make_unique<juce::AudioParameterChoice>("osc1_shape", "Oscillator 1 Shape", juce::StringArray {"Analog Pulse", "Saw", "Sub"}, 0),
           std::make_unique<juce::AudioParameterFloat>("osc1_pitch", "Oscillator 1 Pitch", -24.0f, 24.0f, 0.0f),
           std::make_unique<juce::AudioParameterChoice>("osc2_shape", "Oscillator 2 Shape", juce::StringArray {"Stab 1", "Stab 2", "Texture"}, 0),
           std::make_unique<juce::AudioParameterFloat>("osc2_pitch", "Oscillator 2 Pitch", -24.0f, 24.0f, 0.0f),
           std::make_unique<juce::AudioParameterFloat>("spatial_x", "Spatial X", -1.0f, 1.0f, 0.0f),
           std::make_unique<juce::AudioParameterFloat>("spatial_y", "Spatial Y", 0.0f, 1.0f, 0.0f),
           std::make_unique<juce::AudioParameterChoice>("weather_mode", "Weather Mode", juce::StringArray {"Forest", "Valley", "Temple", "Hut", "Basement"}, 0)
       })
{
}

OraclePadAudioProcessor::~OraclePadAudioProcessor() {}

const juce::String OraclePadAudioProcessor::getName() const { return JucePlugin_Name; }
bool OraclePadAudioProcessor::acceptsMidi() const { return true; }
bool OraclePadAudioProcessor::producesMidi() const { return false; }
double OraclePadAudioProcessor::getTailLengthSeconds() const { return 0.0; }
int OraclePadAudioProcessor::getNumPrograms() { return 1; }
int OraclePadAudioProcessor::getCurrentProgram() { return 0; }
void OraclePadAudioProcessor::setCurrentProgram (int index) {}
const juce::String OraclePadAudioProcessor::getProgramName (int index) { return {}; }
void OraclePadAudioProcessor::changeProgramName (int index, const juce::String& newName) {}

void OraclePadAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock) {}
void OraclePadAudioProcessor::releaseResources() {}

void OraclePadAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();
}

bool OraclePadAudioProcessor::hasEditor() const { return true; }
juce::AudioProcessorEditor* OraclePadAudioProcessor::createEditor() { return new OraclePadAudioProcessorEditor (*this); }

void OraclePadAudioProcessor::getStateInformation (juce::MemoryBlock& destData) {}
void OraclePadAudioProcessor::setStateInformation (const void* data, int sizeInBytes) {}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new OraclePadAudioProcessor(); }