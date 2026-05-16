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

    void start (int /*note*/, float freq, double sr, int wtSize,
                const juce::ADSR::Parameters& p)
    {
        currentAngle = 0.0f;
        angleDelta   = freq * (float)wtSize / (float)sr;
        adsr.setSampleRate (sr);
        adsr.setParameters (p);
        adsr.noteOn();
    }

    void release() { adsr.noteOff(); }

    bool isActive() const noexcept { return adsr.isActive(); }

    std::pair<float, float> renderStereo (const std::vector<float>& wt1,
                                          const std::vector<float>& wt2,
                                          float morph, float tilt, float spread)
    {
        if (!adsr.isActive()) return { 0.0f, 0.0f };

        const int   sz   = (int)wt1.size();
        const int   idx0 = (int)currentAngle % sz;
        const int   idx1 = (idx0 + 1) % sz;
        const float frac = currentAngle - (float)(int)currentAngle;

        float s1     = wt1[idx0] + frac * (wt1[idx1] - wt1[idx0]);
        float s2     = wt2[idx0] + frac * (wt2[idx1] - wt2[idx0]);
        float sample = (s1 + morph * (s2 - s1)) * adsr.getNextSample();

        currentAngle += angleDelta;
        if (currentAngle >= (float)sz) currentAngle -= (float)sz;

        const float l = sample * std::cos (tilt * juce::MathConstants<float>::halfPi)
                                * (1.0f - spread);
        const float r = sample * std::sin (tilt * juce::MathConstants<float>::halfPi)
                                * (1.0f + spread);
        return { l, r };
    }

private:
    float       currentAngle = 0.0f, angleDelta = 0.0f;
    juce::ADSR  adsr;
};

// ============================================================================
// SubOscillator — monophonic sine/square morphable sub bass (Phase 12)
// ============================================================================
class SubOscillator
{
public:
    void prepare (double sr, int /*samplesPerBlock*/)
    {
        sampleRate   = sr;
        phase        = 0.0f;
        targetLevel  = 0.0f;
        currentLevel = 0.0f;
    }

    void setOctaveOffset (int octaves)
    {
        octaveMultiplier = std::pow (2.0f, (float)octaves);
    }

    void noteOn  (int note)
    {
        freq        = (float)juce::MidiMessage::getMidiNoteInHertz (note) * octaveMultiplier;
        targetLevel = 1.0f;
    }

    void noteOff (int /*note*/) { targetLevel = 0.0f; }

    float processSample (float level, float shape)
    {
        currentLevel += 0.005f * (targetLevel - currentLevel);
        if (currentLevel < 0.0001f) return 0.0f;

        phase += freq / (float)sampleRate;
        if (phase >= 1.0f) phase -= 1.0f;

        const float sine   = std::sin (phase * juce::MathConstants<float>::twoPi);
        const float square = (phase < 0.5f) ? 1.0f : -1.0f;
        return (sine * (1.0f - shape) + square * shape) * level * currentLevel;
    }

private:
    double sampleRate      = 44100.0;
    float  phase           = 0.0f;
    float  freq            = 0.0f;
    float  currentLevel    = 0.0f;
    float  targetLevel     = 0.0f;
    float  octaveMultiplier = 0.5f;
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

private:
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void buildWavetables();

    static constexpr int numVoices     = 8;
    static constexpr int wavetableSize = 2048;

    std::vector<OracleVoice>  voices;
    std::vector<float>        osc1Wavetables[2];

    SubOscillator    subOsc_;
    VintageProcessor vintageProc_;
    AtmosphereEngine atmosphereEngine_;
    juce::ADSR       masterGate_;

    juce::AudioBuffer<float> oscBuf_, subBuf_, granBuf_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OraclePadAudioProcessor)
};
