#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>
#include <array>
#include <cstdint>

// ============================================================================
// GranularSampleBuffer — Phase 15
// ============================================================================
struct GranularSampleBuffer : public juce::ReferenceCountedObject
{
    using Ptr = juce::ReferenceCountedObjectPtr<GranularSampleBuffer>;

    juce::AudioBuffer<float> audio;
    double                   sourceSampleRate = 44100.0;
};

// ============================================================================
// Grain — POD
// ============================================================================
struct Grain
{
    bool   active       = false;
    double readPos      = 0.0;
    double readSpeed    = 1.0;
    int    totalSamples = 0;
    int    samplesDone  = 0;
    float  gain         = 1.0f;
};

// ============================================================================
// GranularEngine — Phase 15  "Stochastic Microcosm"
// ============================================================================
class GranularEngine
{
public:
    static constexpr int kMaxGrains = 64;

    void prepare (double sampleRate, int blockSize);
    void reset();

    void setNote (int note) noexcept;

    void loadBuffer (GranularSampleBuffer::Ptr newBuffer);

    void processBlock (juce::AudioBuffer<float>& output,
                       float density,
                       float sizeSec,
                       float spray,
                       float pitch,
                       float level,
                       float scape,
                       float startNorm,
                       float endNorm,
                       float fadeInFrac,
                       float fadeOutFrac,
                       bool  loop);

    int getActiveGrainPositions (float* posOut, int maxOut) const noexcept;

    bool   hasBuffer()           const noexcept;
    int    getBufferNumSamples() const noexcept;
    double getBufferSampleRate() const noexcept;

    GranularSampleBuffer::Ptr getDisplayBuffer() const noexcept;

private:
    double sampleRate_ = 44100.0;
    int currentMidiNote_ = 60;

    juce::SpinLock            bufferLock_;
    GranularSampleBuffer::Ptr currentBuffer_;

    std::array<Grain, kMaxGrains> grains_ {};

    double schedulerPhase_ = 0.0;

    std::array<std::atomic<float>, kMaxGrains> grainPosAtomic_ {};

    uint32_t rngState_ = 0x7A3D9F1Bu;
    float    nextRandom() noexcept;

    void spawnGrain (const GranularSampleBuffer& buf,
                     float sizeSec,
                     float spray,
                     float pitch,
                     float scape,
                     float startNorm,
                     float endNorm,
                     bool  loop) noexcept;

    static float grainEnvelope (int n, int N,
                                float fadeInFrac,
                                float fadeOutFrac) noexcept;

    static float readLinear (const juce::AudioBuffer<float>& buf,
                              int    channel,
                              double pos) noexcept;
};
