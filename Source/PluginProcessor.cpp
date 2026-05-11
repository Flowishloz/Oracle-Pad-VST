#include "PluginProcessor.h"
#include "PluginEditor.h"

OraclePadAudioProcessor::OraclePadAudioProcessor()
     : AudioProcessor (BusesProperties()
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
       apvts (*this, nullptr, "Parameters",
       {
           std::make_unique<juce::AudioParameterChoice>("osc1_shape",  "Oscillator 1 Shape", juce::StringArray {"Analog Pulse", "Saw", "Sub"}, 0),
           std::make_unique<juce::AudioParameterFloat> ("osc1_pitch",  "Oscillator 1 Pitch", -24.0f, 24.0f, 0.0f),
           std::make_unique<juce::AudioParameterChoice>("osc2_shape",  "Oscillator 2 Shape", juce::StringArray {"Stab 1", "Stab 2", "Texture"}, 0),
           std::make_unique<juce::AudioParameterFloat> ("osc2_pitch",  "Oscillator 2 Pitch", -24.0f, 24.0f, 0.0f),
           std::make_unique<juce::AudioParameterFloat> ("spatial_x",   "Spatial X",          -1.0f,  1.0f,  0.0f),
           std::make_unique<juce::AudioParameterFloat> ("spatial_y",   "Spatial Y",           0.0f,  1.0f,  0.0f),
           std::make_unique<juce::AudioParameterChoice>("weather_mode", "Weather Mode", juce::StringArray {"Forest", "Valley", "Temple", "Hut", "Basement"}, 0)
       })
{
}

OraclePadAudioProcessor::~OraclePadAudioProcessor() {}

const juce::String OraclePadAudioProcessor::getName() const      { return JucePlugin_Name; }
bool OraclePadAudioProcessor::acceptsMidi()  const               { return true; }
bool OraclePadAudioProcessor::producesMidi() const               { return false; }
double OraclePadAudioProcessor::getTailLengthSeconds() const     { return 2.0; }
int  OraclePadAudioProcessor::getNumPrograms()                   { return 1; }
int  OraclePadAudioProcessor::getCurrentProgram()                { return 0; }
void OraclePadAudioProcessor::setCurrentProgram (int)            {}
const juce::String OraclePadAudioProcessor::getProgramName (int) { return {}; }
void OraclePadAudioProcessor::changeProgramName (int, const juce::String&) {}

// ---------------------------------------------------------------------------
// Wavetable construction — called once in prepareToPlay, never on audio thread
// ---------------------------------------------------------------------------
void OraclePadAudioProcessor::buildWavetables()
{
    for (auto& wt : osc1Wavetables)
        wt.assign (wavetableSize, 0.0f);

    const float twoPi = juce::MathConstants<float>::twoPi;

    // [2] Sub: pure sine
    for (int i = 0; i < wavetableSize; ++i)
        osc1Wavetables[2][i] = std::sin (twoPi * (float) i / (float) wavetableSize);

    // [1] Saw: additive harmonics 1/k * sin(k * phase)
    for (int i = 0; i < wavetableSize; ++i)
    {
        float phase = twoPi * (float) i / (float) wavetableSize;
        for (int k = 1; k <= numHarmonics; ++k)
            osc1Wavetables[1][i] += (1.0f / (float) k) * std::sin ((float) k * phase);
    }

    // [0] Analog Pulse: odd harmonics only (Alpha Juno character)
    for (int i = 0; i < wavetableSize; ++i)
    {
        float phase = twoPi * (float) i / (float) wavetableSize;
        for (int k = 1; k <= numHarmonics; k += 2)
            osc1Wavetables[0][i] += (1.0f / (float) k) * std::sin ((float) k * phase);
    }

    // Normalise each table to [-1, 1]
    for (auto& wt : osc1Wavetables)
    {
        float peak = 0.0f;
        for (auto s : wt) peak = std::max (peak, std::abs (s));
        if (peak > 0.0f)
            for (auto& s : wt) s /= peak;
    }
}

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------
void OraclePadAudioProcessor::prepareToPlay (double sampleRate, int /*samplesPerBlock*/)
{
    currentSampleRate = sampleRate;
    buildWavetables();

    // DIAGNOSTIC: pre-compute phase increment for 440 Hz test tone
    currentPhase = 0.0;
    phaseDelta   = juce::MathConstants<double>::twoPi * 440.0 / sampleRate;

    // Pad envelope
    juce::ADSR::Parameters padParams;
    padParams.attack  = 0.8f;
    padParams.decay   = 0.4f;
    padParams.sustain = 0.85f;
    padParams.release = 1.5f;
    osc1Envelope.setParameters (padParams);
    osc1Envelope.setSampleRate (sampleRate);

    osc1.reset();
    currentNote       = -1;
    currentShapeIndex = -1;
}

void OraclePadAudioProcessor::releaseResources() {}

// ---------------------------------------------------------------------------
// Audio rendering
// ---------------------------------------------------------------------------
void OraclePadAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                             juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();

    // *** DIAGNOSTIC: raw 440 Hz sine at 0.1f — bypasses all MIDI/ADSR ***
    {
        const int numCh = buffer.getNumChannels();
        const int numS  = buffer.getNumSamples();
        for (int i = 0; i < numS; ++i)
        {
            float sample = 0.1f * (float) std::sin (currentPhase);
            currentPhase += phaseDelta;
            if (currentPhase >= juce::MathConstants<double>::twoPi)
                currentPhase -= juce::MathConstants<double>::twoPi;
            for (int ch = 0; ch < numCh; ++ch)
                buffer.getWritePointer (ch)[i] = sample;
        }
        return; // *** all real DSP below is bypassed ***
    }

    // --- Real DSP (active once diagnostic block above is removed) ---
    auto shapeIndex     = (int) apvts.getRawParameterValue ("osc1_shape")->load();
    auto pitchSemitones =       apvts.getRawParameterValue ("osc1_pitch")->load();

    if (shapeIndex != currentShapeIndex)
    {
        osc1.setWavetable (osc1Wavetables[(size_t) shapeIndex]);
        currentShapeIndex = shapeIndex;
    }

    if (currentNote >= 0)
    {
        float freq = 440.0f * std::pow (2.0f, ((float) currentNote - 69.0f + pitchSemitones) / 12.0f);
        osc1.setFrequency (freq, currentSampleRate);
    }

    const int numSamples  = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();
    float* leftChannel    = buffer.getWritePointer (0);
    float* rightChannel   = numChannels > 1 ? buffer.getWritePointer (1) : leftChannel;

    auto midiIter = midiMessages.begin();
    auto midiEnd  = midiMessages.end();

    for (int i = 0; i < numSamples; ++i)
    {
        while (midiIter != midiEnd && midiIter->samplePosition <= i)
        {
            auto msg = midiIter->getMessage();
            if (msg.isNoteOn())
            {
                currentNote = msg.getNoteNumber();
                float freq  = 440.0f * std::pow (2.0f, ((float) currentNote - 69.0f + pitchSemitones) / 12.0f);
                osc1.setFrequency (freq, currentSampleRate);
                osc1Envelope.noteOn();
            }
            else if (msg.isNoteOff() && msg.getNoteNumber() == currentNote)
            {
                osc1Envelope.noteOff();
                currentNote = -1;
            }
            else if (msg.isAllNotesOff() || msg.isAllSoundOff())
            {
                osc1Envelope.reset();
                currentNote = -1;
            }
            ++midiIter;
        }

        float envGain = osc1Envelope.getNextSample();
        float mono    = osc1.getNextSample() * envGain * 0.5f;
        leftChannel[i]  = mono;
        rightChannel[i] = mono;
    }
}

// ---------------------------------------------------------------------------
// Editor / state
// ---------------------------------------------------------------------------
bool OraclePadAudioProcessor::hasEditor() const { return true; }
juce::AudioProcessorEditor* OraclePadAudioProcessor::createEditor()
{
    return new OraclePadAudioProcessorEditor (*this);
}

void OraclePadAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void OraclePadAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState != nullptr && xmlState->hasTagName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new OraclePadAudioProcessor(); }
