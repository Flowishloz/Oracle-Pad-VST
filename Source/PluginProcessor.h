#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <vector>
#include <cmath>
#include <algorithm>
#include "GranularEngine.h"
#include "AtmosphereEngine.h"
#include "PresetManager.h"

// ============================================================================
// OracleVoice — one polyphonic voice for Osc 1 (wavetable, 8 instances)
// ============================================================================
class OracleVoice : public juce::SynthesiserVoice
{
public:
    bool canPlaySound (juce::SynthesiserSound* sound) override { return sound != nullptr; }

    void startNote (int, float, juce::SynthesiserSound*, int) override {}
    void stopNote  (float, bool)                              override {}
    void pitchWheelMoved  (int)                              override {}
    void controllerMoved  (int, int)                         override {}
    void renderNextBlock  (juce::AudioBuffer<float>&, int, int) override {}

    void start (int note, float freq, double sr, int /*wtSize*/,
                const juce::ADSR::Parameters& p)
    {
        midiNote  = note;
        baseFreq_ = freq;
        sr_       = (float)sr;

        // XorShift32 per-voice micro-detune (±0.1 % of base freq, scales with mix).
        static uint32_t rng = 0xABCD1234u;
        rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
        detuneOffset_ = ((float)(rng & 0xFFFF) / 65535.0f - 0.5f) * 0.002f;

        sinePhase_  = 0.0f;
        sawPhase_   = 0.0f;
        pulsePhase_ = 0.0f;
        driftAccum_ = 0.0f;
        lpState_    = 0.0f;

        adsr.setSampleRate (sr);
        adsr.setParameters (p);
        adsr.noteOn();
    }

    void release()          { adsr.noteOff(); }
    bool isActive() const noexcept { return adsr.isActive(); }
    int  getNote()  const noexcept { return midiNote; }

    // mix   [0,1] — 0=pure sine; 0→0.5 blends saw; 0.5→1 blends pulse.
    // morph [0,1] — Juno pulse width: 0=35%, 0.5=50% (square), 1=65%.
    std::pair<float, float> renderStereo (float mix, float morph, float spread,
                                          float cutHz, float sr)
    {
        if (!adsr.isActive()) return { 0.0f, 0.0f };

        const float env  = adsr.getNextSample();
        const float freq = baseFreq_ * (1.0f + detuneOffset_ * mix);
        const float dt   = freq / sr;

        // Foundation sine
        sinePhase_ += dt;
        if (sinePhase_ >= 1.0f) sinePhase_ -= 1.0f;
        const float sine = std::sin (sinePhase_ * juce::MathConstants<float>::twoPi);

        // Drifting sawtooth (0.07 Hz LFO on phase)
        driftAccum_ += 0.07f / sr;
        if (driftAccum_ >= 1.0f) driftAccum_ -= 1.0f;
        const float drift = std::sin (driftAccum_ * juce::MathConstants<float>::twoPi) * 0.0025f;
        sawPhase_ += dt * (1.0f + drift);
        if (sawPhase_ >= 1.0f) sawPhase_ -= 1.0f;
        const float saw = 2.0f * sawPhase_ - 1.0f;

        // Alpha Juno-style pulse (morph maps pw 0.35→0.65)
        pulsePhase_ += dt;
        if (pulsePhase_ >= 1.0f) pulsePhase_ -= 1.0f;
        const float pw    = 0.35f + morph * 0.30f;
        const float pulse = (pulsePhase_ < pw) ? 1.0f : -1.0f;

        // Progressive mix morphing
        const float sawBlend   = juce::jlimit (0.0f, 1.0f, mix * 2.0f);
        const float pulseBlend = juce::jlimit (0.0f, 1.0f, (mix - 0.5f) * 2.0f);
        float sample = (sine  * (1.0f - sawBlend * 0.45f - pulseBlend * 0.30f)
                      + saw   * sawBlend   * 0.45f
                      + pulse * pulseBlend * 0.30f) * env;

        // One-pole LP filter (Cut knob)
        const float fc  = juce::jlimit (20.0f, 20000.0f, cutHz) / sr;
        const float c   = std::exp (-juce::MathConstants<float>::twoPi * fc);
        lpState_ = lpState_ * c + sample * (1.0f - c);
        sample   = lpState_;

        // Per-voice stereo spread
        return { sample * (1.0f - spread * 0.5f),
                 sample * (1.0f + spread * 0.5f) };
    }

private:
    float      baseFreq_     = 440.0f;
    float      sr_           = 44100.0f;
    float      detuneOffset_ = 0.0f;
    float      sinePhase_    = 0.0f;
    float      sawPhase_     = 0.0f;
    float      pulsePhase_   = 0.0f;
    float      driftAccum_   = 0.0f;
    float      lpState_      = 0.0f;
    int        midiNote      = -1;
    juce::ADSR adsr;
};

// ============================================================================
// VintageProcessor — Phase 14 stub (pass-through at vintage=0)
// ============================================================================
class VintageProcessor
{
public:
    void prepare (double sr, int /*blockSize*/) { sampleRate_ = sr; }
    void reset   () {}

    void process (juce::AudioBuffer<float>& /*buffer*/, float /*amount*/)
    {
        // Stub — Phase 14 unrecovered. Pass-through intentional at amount=0.
    }

private:
    double sampleRate_ = 44100.0;
};

// ============================================================================
// OraclePadAudioProcessor — OEL-90 main audio processor
//
// Member declaration order is load-bearing:
//   apvts MUST appear before presetManager_ (C++ initializes in declaration order).
// ============================================================================
class OraclePadAudioProcessor : public juce::AudioProcessor
{
public:
    OraclePadAudioProcessor();
    ~OraclePadAudioProcessor() override;

    void prepareToPlay  (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "OEL-90"; }
    bool   acceptsMidi()   const override { return true; }
    bool   producesMidi()  const override { return false; }
    bool   isMidiEffect()  const override { return false; }
    double getTailLengthSeconds() const override { return 2.0; }

    int  getNumPrograms()                        override { return 1; }
    int  getCurrentProgram()                     override { return 0; }
    void setCurrentProgram (int)                 override {}
    const juce::String getProgramName (int)      override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    void loadGranularSample (const juce::File& file);

    // apvts MUST be declared first — PresetManager holds a reference to it.
    juce::AudioProcessorValueTreeState apvts;
    std::atomic<float>  currentOutputLevel { 0.0f };
    GranularEngine      granEngine_;
    PresetManager       presetManager_;

    // Lock-free stereo FIFO — audio thread writes, Lissajous timer reads.
    static constexpr int kScopeFifoSize = 4096;
    struct StereoSample { float l, r; };
    juce::AbstractFifo scopeFifo  { kScopeFifoSize };
    StereoSample       scopeBuffer[kScopeFifoSize] {};

private:
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void buildWavetables();

    static constexpr int numVoices     = 8;
    static constexpr int wavetableSize = 2048;

    std::vector<OracleVoice>  voices;
    std::vector<float>        osc1Wavetables[2];
    int                       rrIdx_ = 0;

    VintageProcessor  vintageProc_;
    AtmosphereCloud   atmosphereEngine_;
    juce::ADSR        masterGate_;
    juce::IIRFilter   masterHPF_[2];   // 35 Hz high-pass, one per stereo channel

    juce::AudioBuffer<float> oscBuf_, granBuf_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OraclePadAudioProcessor)
};
