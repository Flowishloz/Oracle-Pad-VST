#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

// Band-limited wavetable oscillator with linear interpolation.
// Pointer-based — zero allocation in the audio thread.
struct WavetableOscillator
{
    const std::vector<float>* wavetable = nullptr;
    float currentIndex = 0.0f;
    float tableDelta   = 0.0f;

    void setWavetable (const std::vector<float>& wt) noexcept { wavetable = &wt; }

    float getNextSample() noexcept
    {
        if (wavetable == nullptr) return 0.0f;
        auto tableSize = (int) wavetable->size();
        auto index0    = (int) currentIndex;
        auto index1    = index0 == tableSize - 1 ? 0 : index0 + 1;
        auto frac      = currentIndex - (float) index0;
        auto sample    = (*wavetable)[index0] + frac * ((*wavetable)[index1] - (*wavetable)[index0]);
        currentIndex  += tableDelta;
        if (currentIndex >= (float) tableSize) currentIndex -= (float) tableSize;
        return sample;
    }

    void setFrequency (float frequency, double sampleRate) noexcept
    {
        if (wavetable == nullptr) return;
        tableDelta = (float) wavetable->size() * frequency / (float) sampleRate;
    }

    void reset() noexcept { currentIndex = 0.0f; tableDelta = 0.0f; }
};

// One synthesizer voice: oscillator + ADSR envelope.
struct OracleVoice
{
    WavetableOscillator oscillator;
    juce::ADSR          envelope;
    int                 noteNumber = -1;
    bool                active     = false;

    void start (int note, float frequency, double sampleRate,
                const std::vector<float>& table,
                const juce::ADSR::Parameters& params) noexcept
    {
        oscillator.setWavetable (table);
        oscillator.setFrequency (frequency, sampleRate);
        oscillator.reset();
        envelope.setSampleRate (sampleRate);
        envelope.setParameters (params);
        envelope.noteOn();
        noteNumber = note;
        active     = true;
    }

    void release() noexcept { envelope.noteOff(); }

    float render() noexcept
    {
        if (!active) return 0.0f;
        float env = envelope.getNextSample();
        if (!envelope.isActive()) { active = false; noteNumber = -1; return 0.0f; }
        return oscillator.getNextSample() * env;
    }
};

class OraclePadAudioProcessor  : public juce::AudioProcessor
{
public:
    OraclePadAudioProcessor();
    ~OraclePadAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;
    bool acceptsMidi() const override;
    bool producesMidi() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

    // Written by the audio thread, read by the editor timer — atomic for safety
    std::atomic<float> outputLevel { 0.0f };

private:
    void buildWavetables();
    OracleVoice* findFreeVoice()               noexcept;
    OracleVoice* findVoiceForNote (int noteNum) noexcept;

    // 2048-sample tables, 64 harmonics, all seeded at sin(k*θ) — flat group delay
    static constexpr int wavetableSize = 2048;
    static constexpr int numHarmonics  = 64;
    static constexpr int numVoices     = 8;

    // [0] Saw  [1] Square  [2] Sub (sine, played one octave below)
    std::array<std::vector<float>, 3> osc1Wavetables;
    std::array<OracleVoice, numVoices> voices;
    juce::ADSR::Parameters             padParams;

    double currentSampleRate = 44100.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OraclePadAudioProcessor)
};
