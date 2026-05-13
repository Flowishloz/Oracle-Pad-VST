#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "AtmosphereEngine.h"
#include <array>
#include <atomic>
#include <cmath>

// ============================================================================
// MonoSubOsc — Phase 12, unchanged.
// Lowest-note priority tracker with 5ms crossfade and gain-to-saturation macro.
// ============================================================================
class MonoSubOsc
{
public:
    void prepare (double sampleRate, int blockSize);
    void noteOn  (int midiNote);
    void noteOff (int midiNote);
    void allNotesOff();

    float processSample (float subLevel, float subShape);

private:
    static constexpr float  kPlateauDb    = -6.0f;
    static constexpr double kCrossfadeMs  = 5.0;

    double sampleRate_        = 44100.0;
    int    crossfadeSamples_  = 220;

    std::array<bool, 128> noteActive_ {};
    int lowestNote_ = -1;

    double phaseA_ = 0.0, freqA_ = 0.0;
    double phaseB_ = 0.0, freqB_ = 0.0;

    bool inCrossfade_        = false;
    int  crossfadeRemaining_ = 0;

    void  updateLowestNote();
    void  startCrossfadeTo (int newNote);
    static float triangleWave (double phase) noexcept;
    static float midiToHz     (int note)     noexcept;
};

// ============================================================================
// OraclePadAudioProcessor
// ============================================================================
class OraclePadAudioProcessor : public juce::AudioProcessor
{
public:
    OraclePadAudioProcessor();
    ~OraclePadAudioProcessor() override;

    void prepareToPlay  (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock   (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor()        const override { return true; }
    const juce::String getName() const override { return "OraclePad"; }
    bool acceptsMidi()      const override { return true; }
    bool producesMidi()     const override { return false; }
    double getTailLengthSeconds() const override { return 4.0; }

    int  getNumPrograms()    override { return 1; }
    int  getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;

    // Peak output level — read by RadarComponent for orb glow
    std::atomic<float> outputLevel { 0.0f };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    MonoSubOsc      subOsc_;
    AtmosphereEngine atmos_;

    void renderSubRange (juce::AudioBuffer<float>& buffer,
                         int startSample, int endSample,
                         float subLevel, float subShape);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OraclePadAudioProcessor)
};
