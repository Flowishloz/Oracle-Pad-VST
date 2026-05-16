#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <algorithm>

// ============================================================================
// APVTS parameter layout — 24 parameters (ground truth for all UI attachments).
// ============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout OraclePadAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<juce::AudioParameterFloat> ("osc1_vol",    "Osc 1 Vol",  0.0f, 1.0f,   0.8f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("osc1_morph",  "Morph",      0.0f, 1.0f,   0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("osc1_tilt",   "Tilt",       0.0f, 1.0f,   0.5f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("osc1_spread", "Spread",     0.0f, 1.0f,   0.2f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> ("sub_level",   "Sub Level",  0.0f, 1.0f,   0.5f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("sub_shape",   "Sub Shape",  0.0f, 1.0f,   0.0f));
    params.push_back (std::make_unique<juce::AudioParameterInt>   ("sub_octave",  "Sub Octave", -2,   0,      -1));

    params.push_back (std::make_unique<juce::AudioParameterFloat> ("gran_density",  "Density",   1.0f,  100.0f, 20.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("gran_size",     "Size",      0.01f, 0.5f,   0.1f));
    params.push_back (std::make_unique<juce::AudioParameterBool>  ("gran_loop",     "Loop",      true));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("gran_start",    "Start",     0.0f,  1.0f,   0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("gran_end",      "End",       0.0f,  1.0f,   1.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("gran_fade_in",  "Fade In",   0.0f,  0.5f,   0.05f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("gran_fade_out", "Fade Out",  0.0f,  0.5f,   0.05f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> ("adsr_attack",  "Attack",  0.01f, 5.0f,  0.1f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("adsr_decay",   "Decay",   0.1f,  5.0f,  1.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("adsr_sustain", "Sustain", 0.0f,  1.0f,  0.8f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("adsr_release", "Release", 0.1f,  10.0f, 1.5f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> ("spatial_x", "Spatial X", 0.0f, 1.0f, 0.5f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("spatial_y", "Spatial Y", 0.0f, 1.0f, 0.5f));

    params.push_back (std::make_unique<juce::AudioParameterInt>   ("atmosphere_state", "Atmosphere", 0,    4,   0));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("atmosphere_mix",   "Atmos Mix",  0.0f, 1.0f, 0.3f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> ("vintage",     "Vintage",     0.0f, 1.0f, 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("master_gain", "Master Gain", 0.0f, 2.0f, 0.8f));

    return { params.begin(), params.end() };
}

// ============================================================================
// Constructor / Destructor
// ============================================================================
OraclePadAudioProcessor::OraclePadAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
    : AudioProcessor (BusesProperties()
                      .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
#else
    :
#endif
      apvts         (*this, nullptr, "Parameters", createParameterLayout()),
      presetManager_ (apvts)
{
    voices.resize (numVoices);
    buildWavetables();
}

OraclePadAudioProcessor::~OraclePadAudioProcessor() {}

// ============================================================================
// Wavetable construction — sine (slot 0) and square (slot 1)
// ============================================================================
void OraclePadAudioProcessor::buildWavetables()
{
    osc1Wavetables[0].resize (wavetableSize);
    osc1Wavetables[1].resize (wavetableSize);
    for (int i = 0; i < wavetableSize; ++i)
    {
        const float phase = (float)i / wavetableSize * juce::MathConstants<float>::twoPi;
        osc1Wavetables[0][i] = std::sin (phase);
        osc1Wavetables[1][i] = (phase < juce::MathConstants<float>::pi) ? 1.0f : -1.0f;
    }
}

// ============================================================================
// Lifecycle
// ============================================================================
void OraclePadAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    const int sz = std::max (8192, samplesPerBlock);

    oscBuf_.setSize (2, sz);
    subBuf_.setSize (2, sz);
    granBuf_.setSize (2, sz);

    subOsc_.prepare           (sampleRate, sz);
    granEngine_.prepare       (sampleRate, sz);
    atmosphereEngine_.prepare (sampleRate, sz);
    vintageProc_.prepare      (sampleRate, sz);

    masterGate_.setSampleRate (sampleRate);
}

void OraclePadAudioProcessor::releaseResources()
{
    atmosphereEngine_.reset();
    vintageProc_.reset();
}

bool OraclePadAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

// ============================================================================
// processBlock
//
// Signal chain (enforced by directive):
//
//   Main Pad Path = OSC 1 (wavetable) + OSC 2 (granular, ADSR-gated)
//   Main Pad Path  →  master_gain  →  VintageProcessor  →  AtmosphereEngine
//
//   Sub Path = Sub Oscillator (ADSR-gated)
//   Sub Path  →  master_gain
//
//   Final Output = Main Pad Path + Sub Path
//
// The Sub Path bypasses Vintage and Atmosphere entirely to preserve
// low-end phase integrity and prevent spatial smearing of the bass.
// ============================================================================
void OraclePadAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                              juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();

    // Guard: prepareToPlay must have sized the internal buffers first.
    // Returning here protects ModDelayLine/AllPassFilter from unallocated reads.
    if (oscBuf_.getNumSamples() < numSamples)
        return;

    buffer.clear();
    oscBuf_.clear();
    subBuf_.clear();
    granBuf_.clear();

    // ── ADSR params (updated every block so knob changes apply immediately) ─
    juce::ADSR::Parameters p;
    p.attack  = apvts.getRawParameterValue ("adsr_attack") ->load();
    p.decay   = apvts.getRawParameterValue ("adsr_decay")  ->load();
    p.sustain = apvts.getRawParameterValue ("adsr_sustain")->load();
    p.release = apvts.getRawParameterValue ("adsr_release")->load();
    masterGate_.setParameters (p);

    // ── MIDI event dispatch ─────────────────────────────────────────────────
    for (const auto meta : midiMessages)
    {
        const auto m = meta.getMessage();
        if (m.isNoteOn())
        {
            const int   note = m.getNoteNumber();
            const float freq = (float)m.getMidiNoteInHertz (note);
            for (auto& v : voices)
            {
                if (!v.isActive())
                {
                    v.start (note, freq, getSampleRate(), wavetableSize, p);
                    break;
                }
            }
            subOsc_.noteOn  (note);
            granEngine_.setNote (note);
            masterGate_.noteOn();
        }
        else if (m.isNoteOff())
        {
            for (auto& v : voices) v.release();
            subOsc_.noteOff (m.getNoteNumber());
            masterGate_.noteOff();
        }
    }

    // ── OSC 1: wavetable voices → oscBuf_ ──────────────────────────────────
    const float morph   = apvts.getRawParameterValue ("osc1_morph") ->load();
    const float tilt    = apvts.getRawParameterValue ("osc1_tilt")  ->load();
    const float spread  = apvts.getRawParameterValue ("osc1_spread")->load();
    const float osc1Vol = apvts.getRawParameterValue ("osc1_vol")   ->load();

    for (int s = 0; s < numSamples; ++s)
    {
        float l = 0.0f, r = 0.0f;
        for (auto& v : voices)
        {
            auto [vl, vr] = v.renderStereo (osc1Wavetables[0], osc1Wavetables[1],
                                            morph, tilt, spread);
            l += vl;  r += vr;
        }
        oscBuf_.addSample (0, s, l * osc1Vol);
        oscBuf_.addSample (1, s, r * osc1Vol);
    }

    // ── OSC 2: granular render into isolated granBuf_ ──────────────────────
    const float gran_density  = apvts.getRawParameterValue ("gran_density") ->load();
    const float gran_size     = apvts.getRawParameterValue ("gran_size")    ->load();
    const float gran_start    = apvts.getRawParameterValue ("gran_start")   ->load();
    const float gran_end      = apvts.getRawParameterValue ("gran_end")     ->load();
    const float gran_fade_in  = apvts.getRawParameterValue ("gran_fade_in") ->load();
    const float gran_fade_out = apvts.getRawParameterValue ("gran_fade_out")->load();
    const bool  gran_loop     = apvts.getRawParameterValue ("gran_loop")    ->load() > 0.5f;

    granEngine_.processBlock (granBuf_, gran_density, gran_size,
                              0.15f, 1.0f, 0.7f, 0.0f,
                              gran_start, gran_end,
                              gran_fade_in, gran_fade_out,
                              gran_loop);

    // ── ADSR Gating: route granular to Main Pad, sub to Sub Path ───────────
    // Gated granular merges into oscBuf_ (Main Pad Path).
    // Gated sub stays isolated in subBuf_ to bypass Vintage + Atmosphere.
    const float subL = apvts.getRawParameterValue ("sub_level")->load();
    const float subS = apvts.getRawParameterValue ("sub_shape")->load();
    subOsc_.setOctaveOffset ((int)apvts.getRawParameterValue ("sub_octave")->load());

    for (int s = 0; s < numSamples; ++s)
    {
        const float env = masterGate_.getNextSample();

        oscBuf_.addSample (0, s, granBuf_.getSample (0, s) * env);
        oscBuf_.addSample (1, s, granBuf_.getSample (1, s) * env);

        const float subOut = subOsc_.processSample (subL, subS) * env;
        subBuf_.addSample (0, s, subOut);
        subBuf_.addSample (1, s, subOut);
    }

    // ── Main Pad Path: oscBuf_ → output buffer → master gain ───────────────
    const float master = apvts.getRawParameterValue ("master_gain")->load();
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        buffer.addFrom (ch, 0, oscBuf_, ch, 0, numSamples);
    buffer.applyGain (master);

    // ── Main Pad Path → VintageProcessor ───────────────────────────────────
    const float vintAmt = apvts.getRawParameterValue ("vintage")->load();
    vintageProc_.process (buffer, vintAmt);

    // ── Main Pad Path → AtmosphereEngine ───────────────────────────────────
    // spatial_x / spatial_y stored as [0,1] in APVTS; remap to [-1,+1] here.
    const float rawSpatX  = apvts.getRawParameterValue ("spatial_x")        ->load();
    const float rawSpatY  = apvts.getRawParameterValue ("spatial_y")        ->load();
    const int   atmoState = (int)apvts.getRawParameterValue ("atmosphere_state")->load();
    const float atmoMix   = apvts.getRawParameterValue ("atmosphere_mix")   ->load();

    atmosphereEngine_.process (buffer,
                               rawSpatX * 2.0f - 1.0f,
                               rawSpatY * 2.0f - 1.0f,
                               atmoState, atmoMix);

    // ── Sub Path: master gain → mix directly into final output ─────────────
    // Sub bypasses Vintage and Atmosphere to preserve low-end phase integrity.
    subBuf_.applyGain (master);
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        buffer.addFrom (ch, 0, subBuf_, ch, 0, numSamples);

    // ── Update output level for Radar orb glow ─────────────────────────────
    if (numSamples > 0)
    {
        float rmsSum = 0.0f;
        const float* ch0 = buffer.getReadPointer (0);
        for (int s = 0; s < numSamples; ++s)
            rmsSum += ch0[s] * ch0[s];
        currentOutputLevel.store (std::sqrt (rmsSum / (float)numSamples),
                                  std::memory_order_relaxed);
    }
}

// ============================================================================
// Granular sample loading (message thread only)
// ============================================================================
void OraclePadAudioProcessor::loadGranularSample (const juce::File& file)
{
    juce::AudioFormatManager formatManager;
    formatManager.registerBasicFormats();

    std::unique_ptr<juce::AudioFormatReader> reader (
        formatManager.createReaderFor (file));
    if (reader == nullptr) return;

    auto* newBuf = new GranularSampleBuffer();
    newBuf->sourceSampleRate = reader->sampleRate;
    newBuf->audio.setSize ((int)reader->numChannels, (int)reader->lengthInSamples);
    reader->read (&newBuf->audio, 0, (int)reader->lengthInSamples, 0, true, true);

    GranularSampleBuffer::Ptr ptr (newBuf);
    granEngine_.loadBuffer (ptr);

    juce::MessageManager::callAsync ([this, ptr]()
    {
        if (auto* ed = dynamic_cast<OraclePadAudioProcessorEditor*> (getActiveEditor()))
            ed->waveformComponent.setWaveformData (ptr);
    });
}

// ============================================================================
// Editor creation
// ============================================================================
juce::AudioProcessorEditor* OraclePadAudioProcessor::createEditor()
{
    return new OraclePadAudioProcessorEditor (*this);
}

// ============================================================================
// State persistence
// ============================================================================
void OraclePadAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void OraclePadAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xmlState = getXmlFromBinary (data, sizeInBytes))
        if (xmlState->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}

// ============================================================================
// Plugin factory
// ============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new OraclePadAudioProcessor();
}
