#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <vector>
#include <cmath>

// ============================================================================
// ModDelayLine — power-of-2 ring buffer with cubic Hermite fractional read.
// Strict rule: index wrapping uses safe modulo; fractional remainder uses
// std::floor before int cast to prevent negative C++ truncation explosions.
// ============================================================================
class ModDelayLine
{
public:
    void  prepare (int maxDelaySamples);
    void  reset   () noexcept;
    void  push    (float x) noexcept;
    float read    (float delaySamples) const noexcept;

private:
    std::vector<float> buf_;
    int writePos_ = 0;
    int mask_     = 0;

    static float hermite4 (float ym1, float y0, float y1, float y2,
                            float t) noexcept;
};

// ============================================================================
// OnePoleLP — one-pole IIR lowpass for per-tank HF damping.
// ============================================================================
class OnePoleLP
{
public:
    void  setFreq (float cutoffHz, double sampleRate) noexcept;
    float process (float x) noexcept { z_ = g_ * z_ + (1.0f - g_) * x; return z_; }
    void  reset   () noexcept { z_ = 0.0f; }

private:
    float g_ = 0.0f;
    float z_ = 0.0f;
};

// ============================================================================
// AllPassFilter — Schroeder all-pass diffuser, fixed integer delay.
// Equation: v = x - g*s[d]; y = s[d] + g*v  (all-pass transfer function)
// ============================================================================
class AllPassFilter
{
public:
    void  prepare  (int delaySamples);
    void  setCoeff (float g) noexcept { g_ = g; }
    float process  (float x) noexcept;
    void  reset    () noexcept;

private:
    std::vector<float> buf_;
    int   writePos_ = 0;
    int   delay_    = 0;
    int   mask_     = 0;
    float g_        = 0.5f;
};

// ============================================================================
// ModulatedAllPassFilter — Schroeder all-pass with LFO-modulated fractional
// delay. Uses 3rd-Order Hermite interpolation via ModDelayLine.
// Equation: delayed = line_.read(nominal + depth*sin(lfoPhase));
//           v = x - g*delayed;  y = delayed + g*v
// ============================================================================
class ModulatedAllPassFilter
{
public:
    void  prepare       (int maxDelaySamples);
    void  setCoeff      (float g) noexcept { g_ = g; }
    void  setModulation (float nominalDelaySamples, float depthSamples,
                         float rateHz, double sampleRate) noexcept;
    float process       (float x) noexcept;
    void  reset         () noexcept;

private:
    ModDelayLine line_;
    float g_            = 0.5f;
    float nominalDelay_ = 0.0f;
    float depthSamples_ = 0.0f;
    float lfoPhase_     = 0.0f;
    float lfoPhaseInc_  = 0.0f;
};

// ============================================================================
// CloudPreset — per-atmosphere tuning for the Figure-Eight all-pass cloud.
// ============================================================================
struct CloudPreset
{
    const char* name;
    float diffuserG;   // coefficient for all 4 input diffusers
    float tankLMs;     // left long-delay tank (ms)
    float tankRMs;     // right long-delay tank (ms)
    float modAPLMs;    // left modulated all-pass nominal delay (ms)
    float modAPRMs;    // right modulated all-pass nominal delay (ms)
    float lfoRateHz;   // LFO rate for both modulated all-passes
    float lfoDepthMs;  // LFO depth (ms) for both modulated all-passes
    float lpfHz;       // one-pole lowpass cutoff for HF tank damping
    float feedback;    // cross-coupled feedback gain
};

// ============================================================================
// AtmosphereCloud — Dattorro / Lexicon Figure-Eight Modulated All-Pass Network
//
// Signal flow (mono input → stereo output):
//
//   monoIn
//     → [4 x AllPassFilter in series]  (input diffusion, g from preset)
//     → Left Tank:  ModulatedAllPassFilter → ModDelayLine → OnePoleLP
//     → Right Tank: ModulatedAllPassFilter → ModDelayLine → OnePoleLP
//
//   Cross-feedback (Figure-Eight coupling):
//     leftTankOut × feedbackGain → tanh → rightTankInput  (and vice versa)
//
//   Wet output tapped from tank LP outputs; blended with dry via wetMix.
//   spatialX [-1,+1] pans the wet stereo field.
//
// atmosphere_state [0–4] → preset index.
// atmosphere_mix   [0–1] → master dry/wet control.
// ============================================================================
class AtmosphereCloud
{
public:
    static constexpr int kNumPresets = 5;
    static constexpr int kNumDiff    = 4;

    void prepare (double sampleRate, int maxBlockSize);
    void reset   ();

    void process (juce::AudioBuffer<float>& buffer,
                  float spatialX, float spatialY,
                  int   presetIdx, float wetMix);

    static const CloudPreset kPresets[kNumPresets];

private:
    double sampleRate_ = 44100.0;

    // Fixed input diffuser delay times (ms) — Dattorro reference tuning
    static constexpr std::array<float, kNumDiff> kDiffMs = { 4.7f, 3.1f, 7.3f, 5.9f };

    // Input diffusion stage — 4 static all-passes in series
    std::array<AllPassFilter, kNumDiff> diffusers_;

    // Left tank chain: ModAP → long delay → LP damp
    ModulatedAllPassFilter modAPL_;
    ModDelayLine           tankL_;
    OnePoleLP              dampL_;

    // Right tank chain: ModAP → long delay → LP damp
    ModulatedAllPassFilter modAPR_;
    ModDelayLine           tankR_;
    OnePoleLP              dampR_;

    float tankLDelay_   = 0.0f;   // samples
    float tankRDelay_   = 0.0f;   // samples
    float feedbackGain_ = 0.85f;

    // Running cross-feedback state (one sample lookahead — lock-free safe)
    float fbL_ = 0.0f;   // left tank output → feeds right tank
    float fbR_ = 0.0f;   // right tank output → feeds left tank

    int lastPreset_ = -1;

    void applyPreset (int idx) noexcept;

    static float msToSamp (float ms, double sr) noexcept
    { return static_cast<float> (ms * sr / 1000.0); }
};
