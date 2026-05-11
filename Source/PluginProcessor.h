#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "PresetManager.h"

// ---------------------------------------------------------------------------
// Band-limited wavetable oscillator — zero allocation in the audio thread.
// Tables are passed in at render time so a single oscillator can morph
// between two tables by peeking both at the same phase (shared accumulator).
// ---------------------------------------------------------------------------
struct WavetableOscillator
{
    float currentIndex = 0.0f;
    float tableDelta   = 0.0f;

    // Read from `tbl` at current phase with linear interpolation, then advance.
    float tick (const std::vector<float>& tbl) noexcept
    {
        const int   size = (int) tbl.size();
        const int   i0   = (int) currentIndex;
        const int   i1   = i0 + 1 < size ? i0 + 1 : 0;
        const float frac = currentIndex - (float) i0;
        const float s    = tbl[i0] + frac * (tbl[i1] - tbl[i0]);
        currentIndex += tableDelta;
        if (currentIndex >= (float) size) currentIndex -= (float) size;
        return s;
    }

    // Read from an external table at the current phase without advancing.
    // Used to sample two tables at the same phase for morph blending.
    float peek (const std::vector<float>& tbl) const noexcept
    {
        const int   size = (int) tbl.size();
        const int   i0   = (int) currentIndex;
        const int   i1   = i0 + 1 < size ? i0 + 1 : 0;
        const float frac = currentIndex - (float) i0;
        return tbl[i0] + frac * (tbl[i1] - tbl[i0]);
    }

    void advance (int tableSize) noexcept
    {
        currentIndex += tableDelta;
        if (currentIndex >= (float) tableSize) currentIndex -= (float) tableSize;
    }

    void setFrequency (float freq, double sampleRate, int tableSize) noexcept
    {
        tableDelta = (float) tableSize * freq / (float) sampleRate;
    }

    void reset() noexcept { currentIndex = 0.0f; tableDelta = 0.0f; }
};

// ---------------------------------------------------------------------------
// One synthesiser voice.
//
// mainOsc  — phase accumulator for Saw↔Square morph (single shared phase).
// subOsc   — independent phase for -1 octave sine blend.
// tiltZ    — 1-pole filter state for the Harmonic Tilt EQ.
// voiceIndex — 0-7; even voices spread left, odd voices spread right.
//
// renderStereo() returns a {L, R} pair. All parameters are passed in from
// processBlock so the voice itself holds no parameter state — spatial engine
// can intercept and modify per-voice signals cleanly later.
// ---------------------------------------------------------------------------
struct OracleVoice
{
    WavetableOscillator mainOsc;
    WavetableOscillator subOsc;
    juce::ADSR          envelope;
    int                 noteNumber = -1;
    bool                active     = false;
    int                 voiceIndex = 0;
    float               tiltZ      = 0.0f;

    void start (int note, float frequency, double sampleRate,
                int tableSize,
                const juce::ADSR::Parameters& params,
                float spreadPhaseOffset) noexcept
    {
        mainOsc.reset();
        subOsc.reset();
        mainOsc.currentIndex = spreadPhaseOffset;
        mainOsc.setFrequency (frequency,         sampleRate, tableSize);
        subOsc.setFrequency  (frequency * 0.5f,  sampleRate, tableSize);
        tiltZ      = 0.0f;
        noteNumber = note;
        active     = true;
        envelope.setSampleRate (sampleRate);
        envelope.setParameters (params);
        envelope.reset();    // zero output to prevent re-trigger pop/ghost from mid-release level
        envelope.noteOn();
    }

    void release() noexcept { envelope.noteOff(); }

    // morphAmt  : 0 = pure Saw, 1 = pure Square
    // subBlend  : 0..1 — sub octave gain
    // tiltParam : -1..1 — negative = dark (LP), positive = bright (HP boost)
    // spreadAmt : 0..1 — L/R amplitude offset between even/odd voices
    std::pair<float, float> renderStereo (
        const std::vector<float>& sawTbl,
        const std::vector<float>& sqTbl,
        const std::vector<float>& subTbl,
        float morphAmt,
        float subBlend,
        float tiltParam,
        float spreadAmt) noexcept
    {
        if (!active) return {0.0f, 0.0f};

        const float env = envelope.getNextSample();
        if (!envelope.isActive())
        {
            active     = false;
            noteNumber = -1;
            envelope.reset();   // hard-zero output — prevents sub-threshold hum
            return {0.0f, 0.0f};
        }

        // Morph: blend Saw and Square at the same phase, then advance once.
        const float sawS  = mainOsc.peek (sawTbl);
        const float sqS   = mainOsc.peek (sqTbl);
        const float mainS = sawS + morphAmt * (sqS - sawS);
        mainOsc.advance ((int) sawTbl.size());

        // Sub oscillator (independent phase, -1 oct).
        float rawSub     = subOsc.tick (subTbl);
        // Gentle harmonic saturation — adds upper-partial weight, audible on small speakers
        const float subS = std::tanh (rawSub * 1.5f) * subBlend;

        float dry = (mainS + subS) * env;

        // Harmonic Tilt EQ — 1-pole shelving filter, zero heap allocation.
        if (tiltParam < -0.01f)
        {
            // Dark: low-pass. Coefficient proportional to |tilt|.
            const float a = -tiltParam * 0.9f;  // 0 → 0.9 as tilt → -1
            tiltZ = a * tiltZ + (1.0f - a) * dry;
            dry   = tiltZ;
        }
        else if (tiltParam > 0.01f)
        {
            // Bright: add high-frequency content (x - mild_LP(x)).
            tiltZ = 0.6f * tiltZ + 0.4f * dry;
            dry   = dry + tiltParam * (dry - tiltZ);
        }
        else
        {
            tiltZ = dry; // keep filter tracking when flat
        }

        // Stereo spread: even voices bias left, odd voices bias right.
        const float offset = spreadAmt * 0.35f;
        const float lGain  = (voiceIndex % 2 == 0) ? 1.0f + offset : 1.0f - offset;
        const float rGain  = (voiceIndex % 2 == 0) ? 1.0f - offset : 1.0f + offset;

        return {dry * lGain, dry * rGain};
    }
};

// ---------------------------------------------------------------------------
// Blauert binaural spatializer — ITD circular delay + ILD shelving + distance LPF + DRR.
// Lock-free, zero heap allocation. All state lives in struct members.
//
// X-axis: -1 = hard left,  0 = centre,  +1 = hard right
// Y-axis: +1 = full front, 0 = centre,  -1 = full rear
// ---------------------------------------------------------------------------
struct Spatializer
{
    // 256 samples covers 0.8 ms ITD headroom at sample rates up to 192 kHz.
    static constexpr int kDelayBuf = 256;

    float delayBufL[kDelayBuf] {};
    float delayBufR[kDelayBuf] {};
    int   writePos = 0;

    float ildZ_L  = 0.0f;   // ILD 1-pole LP state, left channel
    float ildZ_R  = 0.0f;   // ILD 1-pole LP state, right channel
    float distZ_L = 0.0f;   // Distance LPF state, left channel
    float distZ_R = 0.0f;   // Distance LPF state, right channel

    // Rear pinna notch — biquad Direct Form II Transposed state (L and R).
    float rearZ1_L = 0.0f, rearZ2_L = 0.0f;
    float rearZ1_R = 0.0f, rearZ2_R = 0.0f;

    void prepare (double /*sampleRate*/) noexcept
    {
        std::fill (delayBufL, delayBufL + kDelayBuf, 0.0f);
        std::fill (delayBufR, delayBufR + kDelayBuf, 0.0f);
        writePos = 0;
        ildZ_L = ildZ_R = distZ_L = distZ_R = 0.0f;
        rearZ1_L = rearZ2_L = rearZ1_R = rearZ2_R = 0.0f;
    }

    // Linear-interpolating read from `buf` at `delaySamples` behind writePos.
    // Call AFTER writing buf[writePos] and BEFORE advancing writePos.
    float readDelayed (const float* buf, float delaySamples) const noexcept
    {
        const int   i0   = (int) delaySamples;
        const float frac = delaySamples - (float) i0;
        const int   r0   = (writePos - i0 + kDelayBuf) % kDelayBuf;
        const int   r1   = (r0 - 1   + kDelayBuf) % kDelayBuf;
        return buf[r0] + frac * (buf[r1] - buf[r0]);
    }
};

// ---------------------------------------------------------------------------
// Analog BBD Chorus — dual fractional-delay lines, 0.8 Hz sine LFO.
// Roland trick: right channel LFO is phase-inverted so L/R delay times
// diverge, creating natural stereo width without the comb-filter artifacting
// of in-phase modulation.  6 kHz 1-pole LP simulates BBD chip roll-off.
// All coefficients pre-computed in prepare(); zero allocation on audio thread.
// ---------------------------------------------------------------------------
struct BBDChorus
{
    static constexpr int kBuf = 2048;

    float delayL[kBuf] {};
    float delayR[kBuf] {};
    int   writePos    = 0;
    float lfoPhase    = 0.0f;
    float lfoInc      = 0.0f;   // 0.8 / sampleRate
    float centerSmp   = 0.0f;   // 5 ms * sampleRate
    float modDepthSmp = 0.0f;   // 2 ms * sampleRate
    float lpCoef      = 0.0f;   // exp(-2π * 6000 / sampleRate)
    float lpZ_L       = 0.0f;
    float lpZ_R       = 0.0f;

    void prepare (double sampleRate) noexcept
    {
        std::fill (delayL, delayL + kBuf, 0.0f);
        std::fill (delayR, delayR + kBuf, 0.0f);
        writePos    = 0;
        lfoPhase    = 0.0f;
        lfoInc      = 0.8f / (float) sampleRate;
        centerSmp   = 5.0f * (float) sampleRate / 1000.0f;
        modDepthSmp = 2.0f * (float) sampleRate / 1000.0f;
        lpCoef      = std::exp (-juce::MathConstants<float>::twoPi
                                * 6000.0f / (float) sampleRate);
        lpZ_L = lpZ_R = 0.0f;
    }

    std::pair<float, float> process (float inL, float inR) noexcept
    {
        const float lfo = std::sin (lfoPhase * juce::MathConstants<float>::twoPi);
        lfoPhase += lfoInc;
        if (lfoPhase >= 1.0f) lfoPhase -= 1.0f;

        // Roland phase inversion: left uses +lfo, right uses −lfo
        const float dL = juce::jlimit (1.0f, (float) (kBuf - 2), centerSmp + lfo * modDepthSmp);
        const float dR = juce::jlimit (1.0f, (float) (kBuf - 2), centerSmp - lfo * modDepthSmp);

        delayL[writePos] = inL;
        delayR[writePos] = inR;

        auto readFrac = [this](const float* buf, float d) noexcept -> float {
            const int   i0   = (int) d;
            const float frac = d - (float) i0;
            const int   r0   = (writePos - i0 + kBuf) % kBuf;
            const int   r1   = (r0 - 1   + kBuf) % kBuf;
            return buf[r0] + frac * (buf[r1] - buf[r0]);
        };

        float wetL = readFrac (delayL, dL);
        float wetR = readFrac (delayR, dR);
        writePos = (writePos + 1) % kBuf;

        // BBD chip roll-off: 1-pole LP at 6 kHz
        lpZ_L = lpCoef * lpZ_L + (1.0f - lpCoef) * wetL;
        lpZ_R = lpCoef * lpZ_R + (1.0f - lpCoef) * wetR;

        return { 0.5f * inL + 0.5f * lpZ_L,
                 0.5f * inR + 0.5f * lpZ_R };
    }
};

// ---------------------------------------------------------------------------
// Abbey Road wet-send HPF — 2nd-order Butterworth high-pass (fc = 300 Hz).
// Applied block-level to the wet frame before AtmosphereEngine reverb to
// prevent low-frequency mud build-up.  Direct Form II Transposed.
// ---------------------------------------------------------------------------
struct BiquadHP
{
    float b0=1.f, b1=0.f, b2=0.f, a1=0.f, a2=0.f;
    float z1L=0.f, z2L=0.f, z1R=0.f, z2R=0.f;

    void prepare (double sampleRate, float fc) noexcept
    {
        const float w0    = juce::MathConstants<float>::twoPi * fc / (float) sampleRate;
        const float cosW0 = std::cos (w0);
        const float sinW0 = std::sin (w0);
        const float alpha = sinW0 / std::sqrt (2.0f);  // Q = 0.707, Butterworth
        const float a0    = 1.0f + alpha;
        b0 = ((1.0f + cosW0) * 0.5f) / a0;
        b1 = (-(1.0f + cosW0))       / a0;
        b2 = b0;
        a1 = (-2.0f * cosW0)          / a0;
        a2 = (1.0f - alpha)            / a0;
        z1L = z2L = z1R = z2R = 0.0f;
    }

    void reset() noexcept { z1L = z2L = z1R = z2R = 0.0f; }

    void processBlock (float* bufL, float* bufR, int numSamples) noexcept
    {
        for (int i = 0; i < numSamples; ++i)
        {
            const float yL = b0 * bufL[i] + z1L;
            z1L = b1 * bufL[i] - a1 * yL + z2L;
            z2L = b2 * bufL[i] - a2 * yL;
            bufL[i] = yL;
            const float yR = b0 * bufR[i] + z1R;
            z1R = b1 * bufR[i] - a1 * yR + z2R;
            z2R = b2 * bufR[i] - a2 * yR;
            bufR[i] = yR;
        }
    }
};

// ---------------------------------------------------------------------------
// Atmosphere Engine — Blauert enclosure reverb (5 physical space presets).
// The Spatializer DRR wet frame is fed here; output is summed with the dry
// frame at the master bus.  Zero heap allocation after prepare().
// ---------------------------------------------------------------------------
struct AtmosphereEngine
{
    juce::dsp::Reverb        reverb;
    juce::AudioBuffer<float> wetBuf;   // pre-allocated in prepare()
    int                      lastState = -1;

    static juce::dsp::Reverb::Parameters paramsForState (int s) noexcept
    {
        juce::dsp::Reverb::Parameters p;
        p.dryLevel   = 0.0f;
        p.wetLevel   = 1.0f;
        p.freezeMode = 0.0f;
        switch (s)
        {
            case 0:  p.roomSize = 0.4f;  p.damping = 0.9f; p.width = 0.6f; break; // Rainforest
            case 1:  p.roomSize = 0.9f;  p.damping = 0.2f; p.width = 1.0f; break; // Temple
            case 2:  p.roomSize = 0.95f; p.damping = 0.5f; p.width = 1.0f; break; // Valley
            case 3:  p.roomSize = 0.3f;  p.damping = 0.1f; p.width = 0.8f; break; // City
            default: p.roomSize = 1.0f;  p.damping = 0.0f; p.width = 0.2f; break; // The Silo
        }
        return p;
    }

    void prepare (double sampleRate, int maxSamplesPerBlock)
    {
        juce::dsp::ProcessSpec spec;
        spec.sampleRate       = sampleRate;
        spec.maximumBlockSize = (juce::uint32) maxSamplesPerBlock;
        spec.numChannels      = 2;
        reverb.prepare (spec);
        reverb.setParameters (paramsForState (0));
        lastState = 0;
        wetBuf.setSize (2, maxSamplesPerBlock, false, true, false);
    }

    void reset() noexcept { reverb.reset(); }

    // Fill wetBuf with wet frames during the per-sample loop, then call this once
    // per block.  Processes wetBuf in-place; result is added to dry at master bus.
    void processWetBlock (int numSamples, int state)
    {
        if (state != lastState)
        {
            reverb.setParameters (paramsForState (state));
            lastState = state;
        }
        juce::dsp::AudioBlock<float> block (wetBuf.getArrayOfWritePointers(),
                                            2, (size_t) numSamples);
        reverb.process (juce::dsp::ProcessContextReplacing<float> (block));
    }
};

// ---------------------------------------------------------------------------

class OraclePadAudioProcessor : public juce::AudioProcessor
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
    bool acceptsMidi()  const override;
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

    // Written by audio thread, read by editor timer — atomic for thread safety.
    std::atomic<float> outputLevel { 0.0f };

    // Declared after apvts so it initialises after apvts in member-init order.
    PresetManager presetManager;

private:
    void buildWavetables();
    OracleVoice* findFreeVoice()               noexcept;
    OracleVoice* findVoiceForNote (int noteNum) noexcept;

    // 2048-sample tables, 64 harmonics, all seeded at sin(k*θ) — flat group delay.
    static constexpr int wavetableSize = 2048;
    static constexpr int numHarmonics  = 64;
    static constexpr int numVoices     = 8;

    // [0] Saw  [1] Square  [2] Sub (pure sine, played one octave below)
    std::array<std::vector<float>, 3> osc1Wavetables;
    std::array<OracleVoice, numVoices> voices;
    juce::ADSR::Parameters             padParams;

    Spatializer      spatializer;
    AtmosphereEngine atmosphereEngine;
    BBDChorus        chorus;
    BiquadHP         wetHpf;

    double currentSampleRate = 44100.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OraclePadAudioProcessor)
};
