#include "AtmosphereEngine.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <algorithm>
#include <cassert>

// ============================================================================
// Preset table — maps directly to atmosphere_state values 0–4.
// ============================================================================
const AtmospherePreset AtmosphereEngine::kPresets[AtmosphereEngine::kNumPresets] =
{
    // 0: RAINFOREST — organic, scattered, lush
    {
        "RAINFOREST",
        /* preDelayMs    */ 20.0f,
        /* rt60          */ 3.5f,
        /* roomScale     */ 1.0f,
        /* hfDampHz      */ 4000.0f,
        /* hfDampHzFar   */ 1000.0f,   // heavy air absorption at distance
        /* lfoRateHz     */ 0.35f,
        /* lfoDepthMs    */ 0.8f,
        /* lfoDepthMsFar */ 3.0f,       // deep spatial scatter far back
        /* lfBoostDb     */ 0.0f,
        /* lfBoostHz     */ 250.0f,
        /* lfBoostQ      */ 0.7f,
        /* useMultiTap   */ false,
        /* useAsymER     */ false,
    },
    // 1: TEMPLE — stone, cavernous, resonant
    {
        "TEMPLE",
        /* preDelayMs    */ 40.0f,
        /* rt60          */ 6.0f,
        /* roomScale     */ 2.5f,
        /* hfDampHz      */ 5500.0f,
        /* hfDampHzFar   */ 2000.0f,
        /* lfoRateHz     */ 0.08f,     // barely moves — stone doesn't shift
        /* lfoDepthMs    */ 0.15f,
        /* lfoDepthMsFar */ 0.55f,
        /* lfBoostDb     */ 3.5f,      // resonant low-mid buildup
        /* lfBoostHz     */ 310.0f,
        /* lfBoostQ      */ 1.2f,
        /* useMultiTap   */ false,
        /* useAsymER     */ false,
    },
    // 2: VALLEY — expansive, echoing, open air
    {
        "VALLEY",
        /* preDelayMs    */ 80.0f,
        /* rt60          */ 5.0f,
        /* roomScale     */ 3.0f,
        /* hfDampHz      */ 9000.0f,   // open air, less close damping
        /* hfDampHzFar   */ 1400.0f,   // extreme absorption at distance
        /* lfoRateHz     */ 0.18f,
        /* lfoDepthMs    */ 0.4f,
        /* lfoDepthMsFar */ 2.5f,
        /* lfBoostDb     */ -1.0f,
        /* lfBoostHz     */ 180.0f,
        /* lfBoostQ      */ 0.7f,
        /* useMultiTap   */ true,      // mountain-echo multi-tap pre-feed
        /* useAsymER     */ false,
    },
    // 3: CITY — concrete, glass, asymmetric
    {
        "CITY",
        /* preDelayMs    */ 15.0f,
        /* rt60          */ 2.0f,
        /* roomScale     */ 0.8f,
        /* hfDampHz      */ 7000.0f,
        /* hfDampHzFar   */ 3500.0f,
        /* lfoRateHz     */ 0.55f,
        /* lfoDepthMs    */ 0.35f,
        /* lfoDepthMsFar */ 1.2f,
        /* lfBoostDb     */ -2.0f,
        /* lfBoostHz     */ 200.0f,
        /* lfBoostQ      */ 0.8f,
        /* useMultiTap   */ false,
        /* useAsymER     */ true,      // asymmetric ER injection
    },
    // 4: SILO — metal, tight, highly reflective
    {
        "SILO",
        /* preDelayMs    */ 5.0f,
        /* rt60          */ 4.0f,
        /* roomScale     */ 0.6f,
        /* hfDampHz      */ 13000.0f,  // near-zero HF damping — shiny metal
        /* hfDampHzFar   */ 5500.0f,
        /* lfoRateHz     */ 2.5f,      // fast — smears metallic resonant nodes
        /* lfoDepthMs    */ 0.75f,
        /* lfoDepthMsFar */ 2.0f,
        /* lfBoostDb     */ 0.0f,
        /* lfBoostHz     */ 200.0f,
        /* lfBoostQ      */ 0.7f,
        /* useMultiTap   */ false,
        /* useAsymER     */ false,
    },
};

// ============================================================================
// ModDelayLine
// ============================================================================

void ModDelayLine::prepare (int maxDelaySamples)
{
    int pow2 = 1;
    while (pow2 < maxDelaySamples + 4) pow2 <<= 1;
    buf_.assign (pow2, 0.0f);
    mask_     = pow2 - 1;
    writePos_ = 0;
}

void ModDelayLine::reset () noexcept
{
    std::fill (buf_.begin(), buf_.end(), 0.0f);
    writePos_ = 0;
}

void ModDelayLine::push (float x) noexcept
{
    buf_[writePos_] = x;
    writePos_       = (writePos_ + 1) & mask_;
}

// Cubic Hermite interpolation — reads from the ring buffer at a fractional
// delay.  writePos_ points to the NEXT write slot, so sample 1 delay ago is
// at writePos_-1, 2 ago is writePos_-2, etc.
float ModDelayLine::read (float delaySamples) const noexcept
{
    const int   n    = static_cast<int> (delaySamples);
    const float frac = delaySamples - static_cast<float> (n);

    // Four samples around the interpolation point (oldest → newest order
    // for the Hermite kernel: ym1, y0, y1, y2).
    const float ym1 = buf_[(writePos_ - n - 2) & mask_];
    const float y0  = buf_[(writePos_ - n - 1) & mask_];  // floor sample
    const float y1  = buf_[(writePos_ - n    ) & mask_];  // ceil sample
    const float y2  = buf_[(writePos_ - n + 1) & mask_];

    return hermite4 (ym1, y0, y1, y2, frac);
}

// Catmull-Rom / cubic Hermite spline between y0 and y1 at parameter t∈[0,1].
float ModDelayLine::hermite4 (float ym1, float y0, float y1, float y2,
                               float t) noexcept
{
    const float c0 =  y0;
    const float c1 =  0.5f * (y1 - ym1);
    const float c2 =  ym1 - 2.5f * y0 + 2.0f * y1 - 0.5f * y2;
    const float c3 = -0.5f * ym1 + 1.5f * y0 - 1.5f * y1 + 0.5f * y2;
    return ((c3 * t + c2) * t + c1) * t + c0;
}

// ============================================================================
// OnePoleLP
// ============================================================================

void OnePoleLP::setFreq (float cutoffHz, double sampleRate) noexcept
{
    g_ = std::exp (-juce::MathConstants<float>::twoPi
                   * cutoffHz / static_cast<float> (sampleRate));
}

// ============================================================================
// AllPassFilter
// ============================================================================

void AllPassFilter::prepare (int delaySamples)
{
    delay_ = delaySamples;
    int pow2 = 1;
    while (pow2 <= delaySamples) pow2 <<= 1;
    buf_.assign (pow2, 0.0f);
    mask_     = pow2 - 1;
    writePos_ = 0;
}

void AllPassFilter::reset () noexcept
{
    std::fill (buf_.begin(), buf_.end(), 0.0f);
    writePos_ = 0;
}

float AllPassFilter::process (float x) noexcept
{
    const int readPos  = (writePos_ - delay_) & mask_;
    const float delayed = buf_[readPos];

    // Schroeder all-pass: v = x - g*delayed, y = delayed + g*v
    const float v = x - g_ * delayed;
    buf_[writePos_] = v;
    writePos_ = (writePos_ + 1) & mask_;
    return delayed + g_ * v;
}

// ============================================================================
// PeakEQ  (Audio EQ Cookbook peaking filter)
// ============================================================================

void PeakEQ::prepare (double sampleRate) noexcept { sr_ = sampleRate; }

void PeakEQ::setParams (float freqHz, float gainDb, float Q) noexcept
{
    if (std::abs (gainDb) < 0.01f) { b0_ = 1.0f; b1_ = b2_ = a1_ = a2_ = 0.0f; return; }

    const float A     = std::pow (10.0f, gainDb / 40.0f);
    const float w0    = juce::MathConstants<float>::twoPi
                        * freqHz / static_cast<float> (sr_);
    const float cosW0 = std::cos (w0);
    const float sinW0 = std::sin (w0);
    const float alpha = sinW0 / (2.0f * Q);

    const float a0 = 1.0f + alpha / A;
    b0_ = (1.0f + alpha * A) / a0;
    b1_ = (-2.0f * cosW0)    / a0;
    b2_ = (1.0f - alpha * A) / a0;
    a1_ = (-2.0f * cosW0)    / a0;
    a2_ = (1.0f - alpha / A) / a0;
}

float PeakEQ::process (float x) noexcept
{
    // Direct Form II Transposed
    const float y = b0_ * x + z1_;
    z1_           = b1_ * x - a1_ * y + z2_;
    z2_           = b2_ * x - a2_ * y;
    return y;
}

// ============================================================================
// AtmosphereEngine — private helpers
// ============================================================================

// Fast Walsh-Hadamard transform, in-place, 8-point, normalised (÷√8).
void AtmosphereEngine::hadamard8 (float* v) noexcept
{
    // Butterfly stage 1
    for (int i = 0; i < 8; i += 2)
    {
        float a = v[i]; float b = v[i + 1];
        v[i] = a + b; v[i + 1] = a - b;
    }
    // Butterfly stage 2
    for (int i = 0; i < 8; i += 4)
        for (int j = 0; j < 2; ++j)
        {
            float a = v[i + j]; float b = v[i + j + 2];
            v[i + j] = a + b;   v[i + j + 2] = a - b;
        }
    // Butterfly stage 3
    for (int j = 0; j < 4; ++j)
    {
        float a = v[j]; float b = v[j + 4];
        v[j] = a + b;   v[j + 4] = a - b;
    }
    // Normalise
    constexpr float inv8 = 0.35355339f; // 1/√8
    for (int i = 0; i < 8; ++i) v[i] *= inv8;
}

// Called at block rate when preset or distance changes.
void AtmosphereEngine::applyPreset (int presetIdx, float distance) noexcept
{
    const AtmospherePreset& p = kPresets[presetIdx];
    const float twoPi = juce::MathConstants<float>::twoPi;

    // ── Per-delay feedback gain (ensures uniform RT60 across all delay lengths)
    for (int i = 0; i < kFDN; ++i)
    {
        const float delayMs   = kBaseMs[i] * p.roomScale;
        const float delaySec  = delayMs / 1000.0f;
        fdnDelaySamp_[i]      = msToSamp (delayMs, sampleRate_);
        // g_i = 10^( -3 * D_i / RT60 )  →  each delay decays to -60 dB in RT60
        fdnFbGain_[i] = std::pow (10.0f, -3.0f * delaySec / p.rt60);
    }

    // ── LFO phase increments (rate × per-delay multiplier)
    const float lfoBase = p.lfoRateHz * twoPi / static_cast<float> (sampleRate_);
    for (int i = 0; i < kFDN; ++i)
        lfoPhaseInc_[i] = lfoBase * kLfoMult[i];

    // ── Smooth LFO depth: interpolate between close and far values
    const float targetDepthMs = p.lfoDepthMs + distance * (p.lfoDepthMsFar - p.lfoDepthMs);
    smLfoDepth_ = smooth (smLfoDepth_, msToSamp (targetDepthMs, sampleRate_));

    // ── Smooth HF damping frequency (air absorption over distance)
    const float targetHfHz = p.hfDampHz + distance * (p.hfDampHzFar - p.hfDampHz);
    smHfHz_ = smooth (smHfHz_, targetHfHz);

    // ── Update damping filter coefficients
    for (int i = 0; i < kFDN; ++i)
        dampFilters_[i].setFreq (smHfHz_, sampleRate_);

    // ── Smooth pre-delay (adds subtle propagation time at distance)
    const float preMs = p.preDelayMs * (1.0f + distance * 0.25f);
    smPreDelaySamp_ = smooth (smPreDelaySamp_, msToSamp (preMs, sampleRate_));

    // ── EQ (both channels, same coefficients)
    for (auto& eq : peakEQ_)
        eq.setParams (p.lfBoostHz, p.lfBoostDb, p.lfBoostQ);
}

// ============================================================================
// AtmosphereEngine — public interface
// ============================================================================

void AtmosphereEngine::prepare (double sampleRate, int /*maxBlockSize*/)
{
    sampleRate_ = sampleRate;

    // FDN delay lines — max 300 ms × roomScale 3 + 3 ms LFO depth + margin
    const int maxFDNSamp = static_cast<int> (sampleRate * 0.32) + 8;
    for (auto& d : fdnLines_)  d.prepare (maxFDNSamp);

    // Pre-delay — max 80 ms + 25% distance stretch
    const int maxPreSamp = static_cast<int> (sampleRate * 0.11) + 8;
    preDelayLine_.prepare (maxPreSamp);

    // Multi-tap line — max 90 ms
    const int maxTapSamp = static_cast<int> (sampleRate * 0.10) + 8;
    tapLine_.prepare (maxTapSamp);

    // All-pass diffusers
    for (int i = 0; i < kNumAP; ++i)
    {
        const int apSamp = static_cast<int> (msToSamp (kAPMs[i], sampleRate)) + 1;
        allPass_[i].prepare (apSamp);
        allPass_[i].setCoeff (kAPCoeff[i]);
    }

    // EQ
    for (auto& eq : peakEQ_) eq.prepare (sampleRate);

    reset();
}

void AtmosphereEngine::reset ()
{
    for (auto& d : fdnLines_)   d.reset();
    for (auto& f : dampFilters_) f.reset();
    for (auto& a : allPass_)    a.reset();
    for (auto& eq : peakEQ_)    eq.reset();
    preDelayLine_.reset();
    tapLine_.reset();

    std::fill (std::begin (fdnFb_),      std::end (fdnFb_),       0.0f);
    std::fill (std::begin (lfoPhase_),   std::end (lfoPhase_),    0.0f);
    std::fill (std::begin (lfoPhaseInc_),std::end (lfoPhaseInc_), 0.0f);

    smLfoDepth_    = 0.01f;
    smHfHz_        = 8000.0f;
    smPreDelaySamp_= 0.0f;
    lastPreset_    = -1;
}

void AtmosphereEngine::process (juce::AudioBuffer<float>& buffer,
                                 float spatialX, float spatialY,
                                 int   presetIdx, float wetMix)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    const int numCh      = buffer.getNumChannels();
    if (numSamples == 0 || numCh == 0 || wetMix < 1e-6f) return;

    presetIdx = juce::jlimit (0, kNumPresets - 1, presetIdx);

    // ── Block-rate: compute distance from Radar Y, update smoothed params ──
    // spatialY = +1 → close (distance = 0), spatialY = -1 → far (distance = 1)
    const float distance = juce::jlimit (0.0f, 1.0f, (1.0f - spatialY) * 0.5f);
    applyPreset (presetIdx, distance);
    lastPreset_ = presetIdx;

    const AtmospherePreset& p = kPresets[presetIdx];

    // ── Stereo pan of the wet field from spatial_x ─────────────────────────
    // Constant-power pan law on the wet signal
    const float panAngle = (spatialX + 1.0f) * 0.25f         // [0, 0.5]
                           * juce::MathConstants<float>::pi;   // [0, π/2]
    const float panL = std::cos (panAngle);
    const float panR = std::sin (panAngle);

    // ── Process sample-by-sample ───────────────────────────────────────────
    const float* inL  = numCh > 0 ? buffer.getReadPointer  (0) : nullptr;
    const float* inR  = numCh > 1 ? buffer.getReadPointer  (1) : nullptr;
    float*       outL = numCh > 0 ? buffer.getWritePointer (0) : nullptr;
    float*       outR = numCh > 1 ? buffer.getWritePointer (1) : nullptr;

    // Tiny DC bias prevents denormals in silent passages inside the FDN.
    constexpr float kDenormBias = 1.0e-15f;

    for (int s = 0; s < numSamples; ++s)
    {
        // Mono dry mix
        const float dryL = (inL ? inL[s] : 0.0f);
        const float dryR = (inR ? inR[s] : dryL);
        float monoIn     = (dryL + dryR) * 0.5f;

        // ── Pre-delay ─────────────────────────────────────────────────────
        preDelayLine_.push (monoIn);
        float sig = preDelayLine_.read (smPreDelaySamp_);

        // ── Multi-tap (Valley: mountain echo; City: asymmetric ER) ────────
        tapLine_.push (monoIn);

        if (p.useMultiTap)
        {
            // Three mountain-bounce echoes feed into the FDN input
            for (int t = 0; t < kNumTaps; ++t)
                sig += kValleyTapW[t] *
                       tapLine_.read (msToSamp (kValleyTapMs[t], sampleRate_));
            sig *= 0.5f;  // compensate for extra energy
        }
        else if (p.useAsymER)
        {
            // Asymmetric early reflections — sparser than a Schroeder diffuser,
            // alternating L/R polarity to create hard-surface flutter onset.
            float erSum = 0.0f;
            for (int t = 0; t < kNumERTaps; ++t)
            {
                const float tap = tapLine_.read (msToSamp (kCityERMs[t], sampleRate_));
                erSum += (t % 2 == 0 ? 1.0f : -1.0f) * kCityERW[t] * tap;
            }
            sig = sig * 0.6f + erSum * 0.4f;
        }

        // ── All-pass diffuser chain (4 stages) ────────────────────────────
        for (auto& ap : allPass_)
            sig = ap.process (sig + kDenormBias);

        // ── FDN — read, damp, Hadamard mix, inject, write ─────────────────
        // 1. Read each delay line at the LFO-modulated position
        float fdnOut[kFDN];
        for (int i = 0; i < kFDN; ++i)
        {
            const float modOffset = smLfoDepth_ * std::sin (lfoPhase_[i]);
            fdnOut[i] = fdnLines_[i].read (fdnDelaySamp_[i] + modOffset);
        }

        // 2. Apply HF damping filter to each delay output
        for (int i = 0; i < kFDN; ++i)
            fdnOut[i] = dampFilters_[i].process (fdnOut[i]);

        // 3. Hadamard mix (orthogonal, energy-preserving)
        float mixed[kFDN];
        for (int i = 0; i < kFDN; ++i) mixed[i] = fdnOut[i];
        hadamard8 (mixed);

        // 4. Scale by per-delay RT60-derived feedback gain + inject diffused input
        constexpr float kInjectGain = 0.35355339f; // 1/√8 — uniform FDN input spread
        for (int i = 0; i < kFDN; ++i)
            mixed[i] = mixed[i] * fdnFbGain_[i] + sig * kInjectGain;

        // 5. Write back to delay lines + advance LFOs
        for (int i = 0; i < kFDN; ++i)
        {
            fdnLines_[i].push (mixed[i]);
            lfoPhase_[i] += lfoPhaseInc_[i];
            if (lfoPhase_[i] >= juce::MathConstants<float>::twoPi)
                lfoPhase_[i] -= juce::MathConstants<float>::twoPi;
        }

        // ── Stereo output extraction ───────────────────────────────────────
        // Alternating-sign summation decorrelates L/R without a dedicated
        // decorrelation filter.  Scale by 0.25 (1/4 delays per side).
        float wetL = (fdnOut[0] - fdnOut[2] + fdnOut[4] - fdnOut[6]) * 0.25f;
        float wetR = (fdnOut[1] - fdnOut[3] + fdnOut[5] - fdnOut[7]) * 0.25f;

        // ── Peak EQ (low-mid colour per preset) ───────────────────────────
        wetL = peakEQ_[0].process (wetL);
        wetR = peakEQ_[1].process (wetR);

        // ── Apply spatial_x panning to the wet field ──────────────────────
        const float panWetL = wetL * panL + wetR * (1.0f - panL) * 0.3f;
        const float panWetR = wetR * panR + wetL * (1.0f - panR) * 0.3f;

        // ── Dry/wet blend and write output ────────────────────────────────
        if (outL) outL[s] = dryL * (1.0f - wetMix) + panWetL * wetMix * 2.0f;
        if (outR) outR[s] = dryR * (1.0f - wetMix) + panWetR * wetMix * 2.0f;
    }
}
