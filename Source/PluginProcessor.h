#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

// Linear-interpolating wavetable oscillator. Zero allocation in the audio thread.
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
        auto index0 = (int) currentIndex;
        auto index1 = index0 == tableSize - 1 ? 0 : index0 + 1;
        auto frac   = currentIndex - (float) index0;

        auto sample = (*wavetable)[index0] + frac * ((*wavetable)[index1] - (*wavetable)[index0]);

        currentIndex += tableDelta;
        if (currentIndex >= (float) tableSize)
            currentIndex -= (float) tableSize;

        return sample;
    }

    void setFrequency (float frequency, double sampleRate) noexcept
    {
        if (wavetable == nullptr) return;
        tableDelta = (float) wavetable->size() * frequency / (float) sampleRate;
    }

    void reset() noexcept { currentIndex = 0.0f; tableDelta = 0.0f; }
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

private:
    void buildWavetables();

    static constexpr int wavetableSize = 2048;
    static constexpr int numHarmonics  = 64;

    // osc1Wavetables[0] = Analog Pulse, [1] = Saw, [2] = Sub
    std::array<std::vector<float>, 3> osc1Wavetables;

    WavetableOscillator osc1;
    juce::ADSR          osc1Envelope;

    double currentSampleRate = 44100.0;
    int    currentNote       = -1;
    int    currentShapeIndex = -1;

    // DIAGNOSTIC — remove after signal trace confirmed
    double currentPhase = 0.0;
    double phaseDelta   = 0.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OraclePadAudioProcessor)
};
