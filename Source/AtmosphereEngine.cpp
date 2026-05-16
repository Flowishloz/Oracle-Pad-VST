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
        /* hfDampHzFar   */ 1000.0f,
        /* lfoRateHz     */ 0.35f,
        /* lfoDepthMs    */ 0.8f,
        /* lfoDepthMsFar */ 3.0f,
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
        /* lfoRateHz     */ 0.08f,
        /* lfoDepthMs    */ 0.15f,
        /* lfoDepthMsFar */ 0.55f,
        /* lfBoostDb     */ 3.5f,
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
        /* hfDampHz      */ 9000.0f,
        /* hfDampHzFar   */ 1400.0f,
        /* lfoRateHz     */ 0.18f,
        /* lfoDepthMs    */ 0.4f,
        /* lfoDepthMsFar */ 2.5f,
        /* lfBoostDb     */ -1.0f,
        /* lfBoostHz     */ 180.0f,
        /* lfBoostQ      */ 0.7f,
        /* useMultiTap   */ true,
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
        /* useAsymER     */ true,
    },
    // 4: SILO — metal, tight, highly reflective
    {
        "SILO",
        /* preDelayMs    */ 5.0f,
        /* rt60          */ 4.0f,
        /* roomScale     */ 0.6f,
        /* hfDampHz      */ 13000.0f,
        /* hfDampHzFar   */ 5500.0f,
        /* lfoRateHz     */ 2.5f,
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

float ModDelayLine::read (float delaySamples) const noexcept
{
    const int   n    = static_cast<int> (delaySamples);
    const float frac = delaySamples - static_cast<float> (n);

    const float ym1 = buf_[(writePos_ - n - 2) & mask_];
    const float y0  = buf_[(writePos_ - n - 1) & mask_];
    const float y1  = buf_[(writePos_ - n    ) & mask_];
    const float y2  = buf_[(writePos_ - n + 1) & mask_];

    return hermite4 (ym1, y0, y1, y2, frac);
}

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
    const float y = b0_ * x + z1_;
    z1_           = b1_ * x - a1_ * y + z2_;
    z2_           = b2_ * x - a2_ * y;
    return y;
}

// ============================================================================
// AtmosphereEngine — private helpers
// ============================================================================

void AtmosphereEngine::hadamard8 (float* v) noexcept
{
    for (int i = 0; i < 8; i += 2)
    {
        float a = v[i]; float b = v[i + 1];
        v[i] = a + b; v[i + 1] = a - b;
    }
    for (int i = 0; i < 8; i += 4)
        for (int j = 0; j < 2; ++j)
        {
            float a = v[i + j]; float b = v[i + j + 2];
            v[i + j] = a + b;   v[i + j + 2] = a - b;
        }
    for (int j = 0; j < 4; ++j)
    {
        float a = v[j]; float b = v[j + 4];
        v[j] = a + b;   v[j + 4] = a - b;
    }
    constexpr float inv8 = 0.35355339f;
    for (int i = 0; i < 8; ++i) v[i] *= inv8;
}

void AtmosphereEngine::applyPreset (int presetIdx, float distance) noexcept
{
    const AtmospherePreset& p = kPresets[presetIdx];
    const float twoPi = juce::MathConstants<float>::twoPi;

    for (int i = 0; i < kFDN; ++i)
    {
        const float delayMs   = kBaseMs[i] * p.roomScale;
        const float delaySec  = delayMs / 1000.0f;
        fdnDelaySamp_[i]      = msToSamp (delayMs, sampleRate_);
        fdnFbGain_[i] = std::pow (10.0f, -3.0f * delaySec / p.rt60);
    }

    const float lfoBase = p.lfoRateHz * twoPi / static_cast<float> (sampleRate_);
    for (int i = 0; i < kFDN; ++i)
        lfoPhaseInc_[i] = lfoBase * kLfoMult[i];

    const float targetDepthMs = p.lfoDepthMs + distance * (p.lfoDepthMsFar - p.lfoDepthMs);
    smLfoDepth_ = smooth (smLfoDepth_, msToSamp (targetDepthMs, sampleRate_));

    const float targetHfHz = p.hfDampHz + distance * (p.hfDampHzFar - p.hfDampHz);
    smHfHz_ = smooth (smHfHz_, targetHfHz);

    for (int i = 0; i < kFDN; ++i)
        dampFilters_[i].setFreq (smHfHz_, sampleRate_);

    const float preMs = p.preDelayMs * (1.0f + distance * 0.25f);
    smPreDelaySamp_ = smooth (smPreDelaySamp_, msToSamp (preMs, sampleRate_));

    for (auto& eq : peakEQ_)
        eq.setParams (p.lfBoostHz, p.lfBoostDb, p.lfBoostQ);
}

// ============================================================================
// AtmosphereEngine — public interface
// ============================================================================

void AtmosphereEngine::prepare (double sampleRate, int /*maxBlockSize*/)
{
    sampleRate_ = sampleRate;

    const int maxFDNSamp = static_cast<int> (sampleRate * 0.32) + 8;
    for (auto& d : fdnLines_)  d.prepare (maxFDNSamp);

    const int maxPreSamp = static_cast<int> (sampleRate * 0.11) + 8;
    preDelayLine_.prepare (maxPreSamp);

    const int maxTapSamp = static_cast<int> (sampleRate * 0.10) + 8;
    tapLine_.prepare (maxTapSamp);

    for (int i = 0; i < kNumAP; ++i)
    {
        const int apSamp = static_cast<int> (msToSamp (kAPMs[i], sampleRate)) + 1;
        allPass_[i].prepare (apSamp);
        allPass_[i].setCoeff (kAPCoeff[i]);
    }

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

    const float distance = juce::jlimit (0.0f, 1.0f, (1.0f - spatialY) * 0.5f);
    applyPreset (presetIdx, distance);
    lastPreset_ = presetIdx;

    const AtmospherePreset& p = kPresets[presetIdx];

    const float panAngle = (spatialX + 1.0f) * 0.25f
                           * juce::MathConstants<float>::pi;
    const float panL = std::cos (panAngle);
    const float panR = std::sin (panAngle);

    const float* inL  = numCh > 0 ? buffer.getReadPointer  (0) : nullptr;
    const float* inR  = numCh > 1 ? buffer.getReadPointer  (1) : nullptr;
    float*       outL = numCh > 0 ? buffer.getWritePointer (0) : nullptr;
    float*       outR = numCh > 1 ? buffer.getWritePointer (1) : nullptr;

    constexpr float kDenormBias = 1.0e-15f;

    for (int s = 0; s < numSamples; ++s)
    {
        const float dryL = (inL ? inL[s] : 0.0f);
        const float dryR = (inR ? inR[s] : dryL);
        float monoIn     = (dryL + dryR) * 0.5f;

        preDelayLine_.push (monoIn);
        float sig = preDelayLine_.read (smPreDelaySamp_);

        tapLine_.push (monoIn);

        if (p.useMultiTap)
        {
            for (int t = 0; t < kNumTaps; ++t)
                sig += kValleyTapW[t] *
                       tapLine_.read (msToSamp (kValleyTapMs[t], sampleRate_));
            sig *= 0.5f;
        }
        else if (p.useAsymER)
        {
            float erSum = 0.0f;
            for (int t = 0; t < kNumERTaps; ++t)
            {
                const float tap = tapLine_.read (msToSamp (kCityERMs[t], sampleRate_));
                erSum += (t % 2 == 0 ? 1.0f : -1.0f) * kCityERW[t] * tap;
            }
            sig = sig * 0.6f + erSum * 0.4f;
        }

        for (auto& ap : allPass_)
            sig = ap.process (sig + kDenormBias);

        float fdnOut[kFDN];
        for (int i = 0; i < kFDN; ++i)
        {
            const float modOffset = smLfoDepth_ * std::sin (lfoPhase_[i]);
            fdnOut[i] = fdnLines_[i].read (fdnDelaySamp_[i] + modOffset);
        }

        for (int i = 0; i < kFDN; ++i)
            fdnOut[i] = dampFilters_[i].process (fdnOut[i]);

        float mixed[kFDN];
        for (int i = 0; i < kFDN; ++i) mixed[i] = fdnOut[i];
        hadamard8 (mixed);

        constexpr float kInjectGain = 0.35355339f;
        for (int i = 0; i < kFDN; ++i)
            mixed[i] = mixed[i] * fdnFbGain_[i] + sig * kInjectGain;

        for (int i = 0; i < kFDN; ++i)
        {
            fdnLines_[i].push (mixed[i]);
            lfoPhase_[i] += lfoPhaseInc_[i];
            if (lfoPhase_[i] >= juce::MathConstants<float>::twoPi)
                lfoPhase_[i] -= juce::MathConstants<float>::twoPi;
        }

        float wetL = (fdnOut[0] - fdnOut[2] + fdnOut[4] - fdnOut[6]) * 0.25f;
        float wetR = (fdnOut[1] - fdnOut[3] + fdnOut[5] - fdnOut[7]) * 0.25f;

        wetL = peakEQ_[0].process (wetL);
        wetR = peakEQ_[1].process (wetR);

        const float panWetL = wetL * panL + wetR * (1.0f - panL) * 0.3f;
        const float panWetR = wetR * panR + wetL * (1.0f - panR) * 0.3f;

        if (outL) outL[s] = dryL * (1.0f - wetMix) + panWetL * wetMix * 2.0f;
        if (outR) outR[s] = dryR * (1.0f - wetMix) + panWetR * wetMix * 2.0f;
    }
}
