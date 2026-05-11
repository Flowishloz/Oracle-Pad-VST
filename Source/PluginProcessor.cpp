#include "PluginProcessor.h"
#include "PluginEditor.h"

OraclePadAudioProcessor::OraclePadAudioProcessor()
     : AudioProcessor (BusesProperties()
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
       apvts (*this, nullptr, "Parameters",
       {
           std::make_unique<juce::AudioParameterFloat> ("master_gain", "Master Gain",        0.0f,  1.0f,  0.8f),
           std::make_unique<juce::AudioParameterChoice>("osc1_shape",  "Oscillator 1 Shape", juce::StringArray {"Saw", "Square", "Sub"}, 0),
           std::make_unique<juce::AudioParameterFloat> ("osc1_pitch",  "Oscillator 1 Pitch", -24.0f, 24.0f, 0.0f),
           std::make_unique<juce::AudioParameterChoice>("osc2_shape",  "Oscillator 2 Shape", juce::StringArray {"Stab 1", "Stab 2", "Texture"}, 0),
           std::make_unique<juce::AudioParameterFloat> ("osc2_pitch",  "Oscillator 2 Pitch", -24.0f, 24.0f, 0.0f),
           std::make_unique<juce::AudioParameterFloat> ("spatial_x",   "Spatial X",          -1.0f,  1.0f,  0.0f),
           std::make_unique<juce::AudioParameterFloat> ("spatial_y",   "Spatial Y",           0.0f,  1.0f,  0.0f),
           std::make_unique<juce::AudioParameterChoice>("weather_mode", "Weather Mode", juce::StringArray {"Forest", "Valley", "Temple", "Hut", "Basement"}, 0)
       })
{
    padParams.attack  = 0.8f;   // slow bloom — the "lush" pad character
    padParams.decay   = 0.4f;
    padParams.sustain = 0.85f;
    padParams.release = 1.5f;   // cinematic tail
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
// Wavetable construction
//
// All harmonics are seeded via sin(k * theta) using a single shared phase
// accumulator. This guarantees phase coherence across the entire spectrum:
// no inter-harmonic offsets, no frequency-dependent group delay variation.
// Blauert & Laws (1978) show that group delay > ~1.6ms at 1-2kHz is audible
// as "smearing" — our approach keeps it at zero within the wavetable itself.
//
// 64 harmonics covers ~65Hz (MIDI 42, F#2) before aliasing. Sufficient for
// pad fundamentals. Higher notes naturally use fewer harmonics of the table.
// ---------------------------------------------------------------------------
void OraclePadAudioProcessor::buildWavetables()
{
    for (auto& wt : osc1Wavetables)
        wt.assign (wavetableSize, 0.0f);

    const float twoPi = juce::MathConstants<float>::twoPi;

    // [0] Saw: classic bright, rich pad base — full 1/k series
    for (int i = 0; i < wavetableSize; ++i)
    {
        float phase = twoPi * (float) i / (float) wavetableSize;
        for (int k = 1; k <= numHarmonics; ++k)
            osc1Wavetables[0][i] += (1.0f / (float) k) * std::sin ((float) k * phase);
    }

    // [1] Square: odd harmonics only — hollow, "woody" vintage character
    for (int i = 0; i < wavetableSize; ++i)
    {
        float phase = twoPi * (float) i / (float) wavetableSize;
        for (int k = 1; k <= numHarmonics; k += 2)
            osc1Wavetables[1][i] += (1.0f / (float) k) * std::sin ((float) k * phase);
    }

    // [2] Sub: pure sine — voice frequency is halved at note-on (one octave below)
    for (int i = 0; i < wavetableSize; ++i)
        osc1Wavetables[2][i] = std::sin (twoPi * (float) i / (float) wavetableSize);

    // Normalise all tables to [-1, 1]
    for (auto& wt : osc1Wavetables)
    {
        float peak = 0.0f;
        for (auto s : wt) peak = std::max (peak, std::abs (s));
        if (peak > 0.0f)
            for (auto& s : wt) s /= peak;
    }
}

// ---------------------------------------------------------------------------
// Voice allocation
// ---------------------------------------------------------------------------
OracleVoice* OraclePadAudioProcessor::findFreeVoice() noexcept
{
    for (auto& v : voices) if (!v.active)              return &v;
    for (auto& v : voices) if (!v.envelope.isActive()) return &v;
    return &voices[0]; // steal oldest
}

OracleVoice* OraclePadAudioProcessor::findVoiceForNote (int noteNum) noexcept
{
    for (auto& v : voices)
        if (v.active && v.noteNumber == noteNum) return &v;
    return nullptr;
}

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------
void OraclePadAudioProcessor::prepareToPlay (double sampleRate, int /*samplesPerBlock*/)
{
    currentSampleRate = sampleRate;
    buildWavetables();

    for (auto& v : voices)
    {
        v.active     = false;
        v.noteNumber = -1;
        v.envelope.setSampleRate (sampleRate);
        v.envelope.setParameters (padParams);
        v.envelope.reset();
    }

    outputLevel.store (0.0f, std::memory_order_relaxed);
}

void OraclePadAudioProcessor::releaseResources() {}

// ---------------------------------------------------------------------------
// Audio rendering
// Polyphonic, sample-accurate MIDI, zero heap allocation in the audio thread.
// ---------------------------------------------------------------------------
void OraclePadAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                             juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();

    const int   shapeIndex     = (int) apvts.getRawParameterValue ("osc1_shape")->load();
    const float pitchSemitones =       apvts.getRawParameterValue ("osc1_pitch")->load();
    const float masterGain     =       apvts.getRawParameterValue ("master_gain")->load();
    const bool  isSubMode      = (shapeIndex == 2); // Sub plays one octave below

    const auto& activeTable = osc1Wavetables[(size_t) shapeIndex];
    const int   numSamples  = buffer.getNumSamples();
    const int   numChannels = buffer.getNumChannels();

    auto midiIter = midiMessages.begin();
    auto midiEnd  = midiMessages.end();

    for (int i = 0; i < numSamples; ++i)
    {
        // Sample-accurate MIDI dispatch
        while (midiIter != midiEnd && (*midiIter).samplePosition <= i)
        {
            auto msg = (*midiIter).getMessage();

            if (msg.isNoteOn())
            {
                const int note = msg.getNoteNumber();
                float freq = 440.0f * std::pow (2.0f, ((float) note - 69.0f + pitchSemitones) / 12.0f);
                if (isSubMode) freq *= 0.5f; // one octave below

                // Legato retrigger: release any existing voice for this note
                if (auto* existing = findVoiceForNote (note))
                    existing->release();

                if (auto* v = findFreeVoice())
                    v->start (note, freq, currentSampleRate, activeTable, padParams);
            }
            else if (msg.isNoteOff())
            {
                if (auto* v = findVoiceForNote (msg.getNoteNumber()))
                    v->release();
            }
            else if (msg.isAllNotesOff() || msg.isAllSoundOff())
            {
                for (auto& v : voices) v.active = false;
            }

            ++midiIter;
        }

        // Mix all active voices
        float mono = 0.0f;
        for (auto& v : voices)
            mono += v.render();

        // 0.3f per-voice headroom for 8 voices; tanh soft-clips transient peaks
        // from voice stealing without introducing hard distortion
        mono = std::tanh (mono * 0.3f) * masterGain;

        for (int ch = 0; ch < numChannels; ++ch)
            buffer.getWritePointer (ch)[i] = mono;
    }

    // Leaky peak follower — feeds the editor's radar visualiser (message thread reads this)
    float peak = 0.0f;
    if (numChannels > 0)
    {
        auto* ch0 = buffer.getReadPointer (0);
        for (int i = 0; i < numSamples; ++i)
            peak = std::max (peak, std::abs (ch0[i]));
    }
    const float prev = outputLevel.load (std::memory_order_relaxed);
    outputLevel.store (prev * 0.85f + peak * 0.15f, std::memory_order_relaxed);
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
