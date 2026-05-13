#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <vector>
#include <cmath>

// ============================================================================
// ModDelayLine — power-of-2 ring buffer with cubic Hermite fractional read.
// Used for FDN delay lines (LFO-modulated) and pre-delay / tap lines.
// ============================================================================
class ModDelayLine
{
public:
    void  prepare (int maxDelaySamples);
    void  reset   () noexcept;
    void  push    (float x) noexcept;
    float read    (float delaySamples) const noexcept;  // fractional delay

private:
    std::vector<float> buf_;
    int writePos_ = 0;
    int mask_     = 0;

    static float hermite4 (float ym1, float y0, float y1, float y2,
                            float t) noexcept;
};

// ============================================================================
// OnePoleLP — one-pole IIR lowpass for per-delay HF damping.
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
// ============================================================================
class AllPassFilter
{
public:
    void  prepare (int delaySamples);
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
// PeakEQ — biquad peaking EQ for low-mid coloration per preset.
// ============================================================================
class PeakEQ
{
public:
    void  prepare  (double sampleRate) noexcept;
    void  setParams (float freqHz, float gainDb, float Q) noexcept;
    float process  (float x) noexcept;
    void  reset    () noexcept { z1_ = z2_ = 0.0f; }

private:
    double sr_ = 44100.0;
    float  b0_ = 1.0f, b1_ = 0.0f, b2_ = 0.0f;
    float  a1_ = 0.0f, a2_ = 0.0f;
    float  z1_ = 0.0f, z2_ = 0.0f;
};

// ============================================================================
// AtmospherePreset — per-space DSP parameters.
// All values are at the "neutral" (close, distance = 0) radar position.
// Far values (distance = 1) are interpolated at block rate.
// ============================================================================
struct AtmospherePreset
{
    const char* name;

    float preDelayMs;       // Pre-delay before diffuser + FDN
    float rt60;             // Reverb time T60 (seconds)
    float roomScale;        // Multiplier on base FDN delay times

    float hfDampHz;         // HF damping cutoff at distance=0 (close)
    float hfDampHzFar;      // HF damping cutoff at distance=1 (air absorption)

    float lfoRateHz;        // LFO rate for in-tail delay modulation
    float lfoDepthMs;       // LFO depth (ms) at distance=0
    float lfoDepthMsFar;    // LFO depth (ms) at distance=1

    float lfBoostDb;        // Low-mid peak EQ (0 = flat)
    float lfBoostHz;        // Peak EQ centre frequency
    float lfBoostQ;         // Peak EQ bandwidth

    bool  useMultiTap;      // Enable mountain-echo multi-tap pre-feed (Valley)
    bool  useAsymER;        // Enable asymmetric ER injection (City)
};

// ============================================================================
// AtmosphereEngine — Phase 13 psychoacoustic reverb.
//
// Signal flow (mono input → stereo output):
//   input → pre-delay
//          → (multi-tap bounce feed for Valley)
//          → (asymmetric ER injection for City)
//          → 4-stage Schroeder all-pass diffuser
//          → 8-delay FDN (Hadamard mix, per-delay HF damping, LFO modulation)
//          → biquad peak EQ
//          → stereo split (even delays → L, odd delays → R with sign alternation)
//          → dry/wet blend
//
// Radar integration (invisible to user):
//   spatial_y [-1, +1]:  +1 = close, -1 = far back.
//     distance = (1 - y) * 0.5 ∈ [0, 1]
//     As distance increases: HF cutoff drops (air absorption),
//                            LFO depth rises (spatial scatter),
//                            pre-delay extends slightly.
//   spatial_x [-1, +1]:  pans the wet stereo field.
//
// atmosphere_state [0–4]:  preset index, maps to the 5 space types.
// atmosphere_mix   [0–1]:  master dry/wet control.
// ============================================================================
class AtmosphereEngine
{
public:
    static constexpr int kFDN       = 8;
    static constexpr int kNumPresets = 5;   // maps to atmosphere_state 0–4
    static constexpr int kNumAP     = 4;    // all-pass diffuser stages
    static constexpr int kNumTaps   = 3;    // Valley mountain-echo taps
    static constexpr int kNumERTaps = 5;    // City asymmetric ER taps

    void prepare (double sampleRate, int maxBlockSize);
    void reset   ();

    // Process stereo buffer in-place.
    //   spatialX   [-1, +1]  — L/R wet image offset
    //   spatialY   [-1, +1]  — front/back distance
    //   presetIdx  [0,  4]   — atmosphere_state value
    //   wetMix     [0,  1]   — atmosphere_mix value
    void process (juce::AudioBuffer<float>& buffer,
                  float spatialX, float spatialY,
                  int   presetIdx, float wetMix);

    // Five preset descriptors (atmosphere_state 0–4)
    static const AtmospherePreset kPresets[kNumPresets];

private:
    double sampleRate_ = 44100.0;

    // Base FDN delay times (ms) — geometrically spaced, mutually-prime in samples
    // at 44.1 kHz.  roomScale multiplies these per preset.
    static constexpr std::array<float, kFDN> kBaseMs =
        { 23.71f, 28.09f, 33.57f, 40.33f, 47.19f, 52.97f, 64.11f, 77.39f };

    // Per-delay LFO rate offset multipliers — decorrelates modulation across delays
    static constexpr std::array<float, kFDN> kLfoMult =
        { 1.000f, 1.073f, 0.931f, 1.137f, 0.873f, 1.193f, 0.811f, 1.251f };

    // All-pass diffuser timings and coefficients (fixed, room-independent)
    static constexpr std::array<float, kNumAP> kAPMs    = { 13.1f, 7.3f, 19.7f, 5.1f };
    static constexpr std::array<float, kNumAP> kAPCoeff = { 0.70f, 0.65f, 0.60f, 0.55f };

    // Valley: mountain-echo multi-tap feed times (ms) and weights
    static constexpr std::array<float, kNumTaps>  kValleyTapMs = { 28.0f, 56.0f, 82.0f };
    static constexpr std::array<float, kNumTaps>  kValleyTapW  = { 0.50f, 0.33f, 0.20f };

    // City: asymmetric ER injection times (ms) and weights
    static constexpr std::array<float, kNumERTaps> kCityERMs = { 5.2f, 9.7f, 14.3f, 21.8f, 31.5f };
    static constexpr std::array<float, kNumERTaps> kCityERW  = { 0.60f, 0.50f, 0.40f, 0.30f, 0.20f };

    // ── FDN state ──────────────────────────────────────────────────────────
    float fdnFb_[kFDN] = {};               // inter-block FDN feedback state
    float fdnDelaySamp_[kFDN] = {};        // nominal delay length per line (samples)
    float fdnFbGain_[kFDN] = {};           // per-delay feedback gain (RT60-derived)

    std::array<ModDelayLine, kFDN> fdnLines_;
    std::array<OnePoleLP,    kFDN> dampFilters_;

    // ── LFO ────────────────────────────────────────────────────────────────
    float lfoPhase_[kFDN]    = {};
    float lfoPhaseInc_[kFDN] = {};        // radians/sample (rate * kLfoMult[i])
    float smLfoDepth_        = 0.01f;     // smoothed LFO depth (samples)

    // ── Diffuser ───────────────────────────────────────────────────────────
    std::array<AllPassFilter, kNumAP> allPass_;

    // ── Pre-delay ──────────────────────────────────────────────────────────
    ModDelayLine preDelayLine_;
    float        smPreDelaySamp_ = 0.0f;  // smoothed pre-delay (samples)

    // ── Multi-tap (Valley + City) ──────────────────────────────────────────
    ModDelayLine tapLine_;

    // ── Per-channel EQ ─────────────────────────────────────────────────────
    std::array<PeakEQ, 2> peakEQ_;

    // ── Smoothed block-rate params ─────────────────────────────────────────
    float smHfHz_   = 8000.0f;    // HF damping frequency (smoothed)
    int   lastPreset_ = -1;

    // ── Helpers ────────────────────────────────────────────────────────────
    static void  hadamard8    (float* v) noexcept;
    void         applyPreset  (int presetIdx, float distance) noexcept;
    static float msToSamp     (float ms, double sr) noexcept
                              { return static_cast<float>(ms * sr / 1000.0); }

    // One-pole block-rate smoother
    static float smooth (float current, float target) noexcept
                        { return current + 0.05f * (target - current); }
};
