#include "PluginProcessor.h"
#include "PluginEditor.h"

// ============================================================================
// MonoSubOsc — Phase 12 implementation, unchanged.
// ============================================================================

void MonoSubOsc::prepare (double sampleRate, int /*blockSize*/)
{
    sampleRate_       = sampleRate;
    crossfadeSamples_ = static_cast<int> (sampleRate * (kCrossfadeMs / 1000.0));
    noteActive_.fill (false);
    lowestNote_          = -1;
    phaseA_ = phaseB_    = 0.0;
    freqA_  = freqB_     = 0.0;
    inCrossfade_         = false;
    crossfadeRemaining_  = 0;
}

void MonoSubOsc::noteOn (int midiNote)
{
    if (midiNote < 0 || midiNote > 127) return;
    noteActive_[midiNote] = true;
    updateLowestNote();
}

void MonoSubOsc::noteOff (int midiNote)
{
    if (midiNote < 0 || midiNote > 127) return;
    noteActive_[midiNote] = false;
    updateLowestNote();
}

void MonoSubOsc::allNotesOff()
{
    noteActive_.fill (false);
    lowestNote_         = -1;
    inCrossfade_        = false;
    crossfadeRemaining_ = 0;
}

void MonoSubOsc::updateLowestNote()
{
    int newLowest = -1;
    for (int i = 0; i < 128; ++i)
        if (noteActive_[i]) { newLowest = i; break; }

    if (newLowest == lowestNote_) return;

    if (newLowest < 0)
    {
        phaseB_             = phaseA_;
        freqB_              = freqA_;
        freqA_              = 0.0;
        inCrossfade_        = (lowestNote_ >= 0);
        crossfadeRemaining_ = crossfadeSamples_;
        lowestNote_         = -1;
    }
    else
    {
        startCrossfadeTo (newLowest);
        lowestNote_ = newLowest;
    }
}

void MonoSubOsc::startCrossfadeTo (int newNote)
{
    phaseB_ = phaseA_;
    freqB_  = freqA_;
    freqA_  = static_cast<double> (midiToHz (newNote));

    if (lowestNote_ >= 0)
    {
        inCrossfade_        = true;
        crossfadeRemaining_ = crossfadeSamples_;
    }
    else
    {
        phaseA_      = 0.0;
        inCrossfade_ = false;
    }
}

float MonoSubOsc::processSample (float subLevel, float subShape)
{
    phaseA_ += freqA_ / sampleRate_;
    if (phaseA_ >= 1.0) phaseA_ -= 1.0;

    const float twoPi = juce::MathConstants<float>::twoPi;
    auto waveform = [&] (double ph) -> float
    {
        float s = std::sin (twoPi * static_cast<float> (ph));
        float t = triangleWave (ph);
        return (1.0f - subShape) * s + subShape * t;
    };

    float output = waveform (phaseA_);

    if (inCrossfade_)
    {
        phaseB_ += freqB_ / sampleRate_;
        if (phaseB_ >= 1.0) phaseB_ -= 1.0;
        float t = 1.0f - static_cast<float> (crossfadeRemaining_) /
                         static_cast<float> (crossfadeSamples_);
        output = t * output + (1.0f - t) * waveform (phaseB_);
        if (--crossfadeRemaining_ <= 0)
            inCrossfade_ = false;
    }

    if (lowestNote_ < 0 && !inCrossfade_)
        return 0.0f;

    const float plateau = juce::Decibels::decibelsToGain (kPlateauDb);
    if (subLevel <= 0.5f)
    {
        output *= (subLevel * 2.0f) * plateau;
    }
    else
    {
        float drive     = (subLevel - 0.5f) * 2.0f;
        float inputGain = 1.0f + drive * 3.0f;
        output = std::tanh (output * inputGain) * plateau;
    }

    return output;
}

float MonoSubOsc::triangleWave (double phase) noexcept
{
    if (phase < 0.25) return static_cast<float> (4.0 * phase);
    if (phase < 0.75) return static_cast<float> (2.0 - 4.0 * phase);
    return                    static_cast<float> (4.0 * phase - 4.0);
}

float MonoSubOsc::midiToHz (int note) noexcept
{
    return 440.0f * std::pow (2.0f, (note - 69) / 12.0f);
}

// ============================================================================
// OraclePadAudioProcessor — APVTS parameter layout
//
// Parameter IDs are stable across all phases:
//   sub_level, sub_shape          — Phase 12 MonoSubOsc macros
//   spatial_x, spatial_y          — Radar X/Y (read by RadarComponent)
//   atmosphere_state              — Preset selector [0–4]: Rainforest, Temple,
//                                   Valley, City, Silo (read by RadarComponent)
//   atmosphere_mix                — Master dry/wet [0–1]
// ============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout
OraclePadAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // Phase 12 — Sub-oscillator macros
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "sub_level", 1 }, "Sub Level",
        juce::NormalisableRange<float> (0.0f, 1.0f), 0.5f));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "sub_shape", 1 }, "Sub Shape",
        juce::NormalisableRange<float> (0.0f, 1.0f), 0.0f));

    // Phase 13 — Radar spatial coordinates
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "spatial_x", 1 }, "Spatial X",
        juce::NormalisableRange<float> (-1.0f, 1.0f), 0.0f));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "spatial_y", 1 }, "Spatial Y",
        juce::NormalisableRange<float> (-1.0f, 1.0f), 0.5f));

    // Phase 13 — Atmosphere preset selector (0=Rainforest, 1=Temple,
    //             2=Valley, 3=City, 4=Silo)
    layout.add (std::make_unique<juce::AudioParameterInt> (
        juce::ParameterID { "atmosphere_state", 1 }, "Atmosphere",
        0, 4, 0));

    // Phase 13 — Master dry/wet
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "atmosphere_mix", 1 }, "Atmosphere Mix",
        juce::NormalisableRange<float> (0.0f, 1.0f), 0.4f));

    return layout;
}

// ============================================================================
// Constructor / Destructor
// ============================================================================

OraclePadAudioProcessor::OraclePadAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Parameters", createParameterLayout())
{
}

OraclePadAudioProcessor::~OraclePadAudioProcessor() {}

// ============================================================================
// Audio lifecycle
// ============================================================================

void OraclePadAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    subOsc_.prepare (sampleRate, samplesPerBlock);
    atmos_.prepare  (sampleRate, samplesPerBlock);
}

void OraclePadAudioProcessor::releaseResources()
{
    subOsc_.allNotesOff();
    atmos_.reset();
}

// ============================================================================
// processBlock
//
// Signal chain:
//   1. Sample-accurate MIDI → MonoSubOsc renders into stereo buffer (dry signal)
//   2. Read Radar + Atmosphere parameters
//   3. AtmosphereEngine processes buffer in-place (dry → wet blend)
//   4. Peak meter update for RadarComponent orb glow
// ============================================================================
void OraclePadAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                            juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples          = buffer.getNumSamples();
    const int totalOutputChannels = getTotalNumOutputChannels();

    for (int ch = getTotalNumInputChannels(); ch < totalOutputChannels; ++ch)
        buffer.clear (ch, 0, numSamples);

    // ── Phase 12: Sub-oscillator — sample-accurate MIDI dispatch ──────────
    const float subLevel = apvts.getRawParameterValue ("sub_level")->load();
    const float subShape = apvts.getRawParameterValue ("sub_shape")->load();

    int renderStart = 0;
    for (const auto& meta : midiMessages)
    {
        const int eventPos = meta.samplePosition;
        if (eventPos > renderStart)
            renderSubRange (buffer, renderStart, eventPos, subLevel, subShape);

        const auto msg = meta.getMessage();
        if      (msg.isNoteOn())                              subOsc_.noteOn  (msg.getNoteNumber());
        else if (msg.isNoteOff())                             subOsc_.noteOff (msg.getNoteNumber());
        else if (msg.isAllNotesOff() || msg.isAllSoundOff()) subOsc_.allNotesOff();

        renderStart = eventPos;
    }
    if (renderStart < numSamples)
        renderSubRange (buffer, renderStart, numSamples, subLevel, subShape);

    // ── Phase 13: Atmosphere Engine ───────────────────────────────────────
    const float spatialX     = apvts.getRawParameterValue ("spatial_x")->load();
    const float spatialY     = apvts.getRawParameterValue ("spatial_y")->load();
    const int   atmState     = static_cast<int> (
                                   apvts.getRawParameterValue ("atmosphere_state")->load());
    const float atmMix       = apvts.getRawParameterValue ("atmosphere_mix")->load();

    atmos_.process (buffer, spatialX, spatialY, atmState, atmMix);

    // ── Peak meter (RadarComponent orb glow) ──────────────────────────────
    float peak = 0.0f;
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        const float chPeak = buffer.getMagnitude (ch, 0, numSamples);
        peak = std::max (peak, chPeak);
    }
    // Simple envelope follower (attack fast, release ~200ms at 44.1k/512)
    const float prev = outputLevel.load (std::memory_order_relaxed);
    outputLevel.store (prev < peak ? peak : prev * 0.985f, std::memory_order_relaxed);
}

void OraclePadAudioProcessor::renderSubRange (juce::AudioBuffer<float>& buffer,
                                               int startSample, int endSample,
                                               float subLevel, float subShape)
{
    const int numOut = buffer.getNumChannels();
    for (int s = startSample; s < endSample; ++s)
    {
        const float sub = subOsc_.processSample (subLevel, subShape);
        for (int ch = 0; ch < numOut; ++ch)
            buffer.addSample (ch, s, sub);
    }
}

// ============================================================================
// Editor / State
// ============================================================================

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
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));
    if (xml && xml->hasTagName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

// ============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new OraclePadAudioProcessor();
}
