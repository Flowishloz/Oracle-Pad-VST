#include "AtmosphereEngine.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <algorithm>

// ============================================================================
// Preset table — index matches atmosphere_state [0–4].
//   0 = RAINFOREST  1 = TEMPLE  2 = VALLEY  3 = CITY  4 = SILO
// ============================================================================
const CloudPreset AtmosphereCloud::kPresets[AtmosphereCloud::kNumPresets] =
{
    // 0: RAINFOREST — Living Chaos: heavy diffusion, fast deep LFO
    {
        "RAINFOREST",
        /* diffuserG  */ 0.80f,
        /* tankLMs    */ 41.0f,
        /* tankRMs    */ 47.0f,
        /* modAPLMs   */ 11.0f,
        /* modAPRMs   */ 13.0f,
        /* lfoRateHz  */ 0.8f,
        /* lfoDepthMs */ 3.0f,
        /* lpfHz      */ 6000.0f,
        /* feedback   */ 0.80f,
    },
    // 1: TEMPLE — Holy Shimmer: gentle LFO, bright long trails
    {
        "TEMPLE",
        /* diffuserG  */ 0.60f,
        /* tankLMs    */ 53.0f,
        /* tankRMs    */ 61.0f,
        /* modAPLMs   */ 17.0f,
        /* modAPRMs   */ 19.0f,
        /* lfoRateHz  */ 0.2f,
        /* lfoDepthMs */ 0.5f,
        /* lpfHz      */ 12000.0f,
        /* feedback   */ 0.85f,
    },
    // 2: VALLEY — Infinite Wash: slow LFO, very long tanks, dark damping
    {
        "VALLEY",
        /* diffuserG  */ 0.70f,
        /* tankLMs    */ 89.0f,
        /* tankRMs    */ 97.0f,
        /* modAPLMs   */ 23.0f,
        /* modAPRMs   */ 31.0f,
        /* lfoRateHz  */ 0.1f,
        /* lfoDepthMs */ 2.0f,
        /* lpfHz      */ 2500.0f,
        /* feedback   */ 0.90f,
    },
    // 3: CITY — Urban Slap: very short tanks, fluttery LFO
    {
        "CITY",
        /* diffuserG  */ 0.75f,
        /* tankLMs    */ 17.0f,
        /* tankRMs    */ 23.0f,
        /* modAPLMs   */  5.0f,
        /* modAPRMs   */  7.0f,
        /* lfoRateHz  */ 1.2f,
        /* lfoDepthMs */ 0.8f,
        /* lpfHz      */ 5000.0f,
        /* feedback   */ 0.70f,
    },
    // 4: SILO — Concrete Tower: low diffusion (early transients pass), near-static LFO
    {
        "SILO",
        /* diffuserG  */ 0.30f,
        /* tankLMs    */ 37.0f,
        /* tankRMs    */ 43.0f,
        /* modAPLMs   */  7.0f,
        /* modAPRMs   */ 11.0f,
        /* lfoRateHz  */ 0.05f,
        /* lfoDepthMs */ 0.2f,
        /* lpfHz      */ 9000.0f,
        /* feedback   */ 0.92f,
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
    // std::floor before int cast prevents negative truncation artefacts.
    const int   n    = static_cast<int> (std::floor (delaySamples));
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
    const float delayed = buf_[(writePos_ - delay_) & mask_];
    const float v       = x - g_ * delayed;
    buf_[writePos_]     = v;
    writePos_           = (writePos_ + 1) & mask_;
    return delayed + g_ * v;
}

// ============================================================================
// ModulatedAllPassFilter
// ============================================================================

void ModulatedAllPassFilter::prepare (int maxDelaySamples)
{
    line_.prepare (maxDelaySamples + 4);
}

void ModulatedAllPassFilter::setModulation (float nominalDelaySamples,
                                             float depthSamples,
                                             float rateHz,
                                             double sampleRate) noexcept
{
    nominalDelay_ = nominalDelaySamples;
    depthSamples_ = depthSamples;
    lfoPhaseInc_  = juce::MathConstants<float>::twoPi
                    * rateHz / static_cast<float> (sampleRate);
}

float ModulatedAllPassFilter::process (float x) noexcept
{
    // Clamp to minimum of 1 sample so Hermite indices never go negative.
    const float delay = juce::jmax (1.0f,
                                    nominalDelay_ + depthSamples_ * std::sin (lfoPhase_));

    lfoPhase_ += lfoPhaseInc_;
    if (lfoPhase_ >= juce::MathConstants<float>::twoPi)
        lfoPhase_ -= juce::MathConstants<float>::twoPi;

    const float delayed = line_.read (delay);
    const float v       = x - g_ * delayed;
    line_.push (v);
    return delayed + g_ * v;
}

void ModulatedAllPassFilter::reset () noexcept
{
    line_.reset();
    lfoPhase_ = 0.0f;
}

// ============================================================================
// AtmosphereCloud — private helpers
// ============================================================================

void AtmosphereCloud::applyPreset (int idx) noexcept
{
    const CloudPreset& p = kPresets[idx];

    for (auto& d : diffusers_)
        d.setCoeff (p.diffuserG);

    // ModulatedAllPassFilters get matching coefficients to the diffuser
    // (preset g also governs the tank all-pass character)
    modAPL_.setCoeff (p.diffuserG);
    modAPR_.setCoeff (p.diffuserG);

    tankLDelay_ = msToSamp (p.tankLMs, sampleRate_);
    tankRDelay_ = msToSamp (p.tankRMs, sampleRate_);

    const float depthSamp = msToSamp (p.lfoDepthMs, sampleRate_);
    modAPL_.setModulation (msToSamp (p.modAPLMs, sampleRate_), depthSamp,
                           p.lfoRateHz, sampleRate_);
    modAPR_.setModulation (msToSamp (p.modAPRMs, sampleRate_), depthSamp,
                           p.lfoRateHz, sampleRate_);

    dampL_.setFreq (p.lpfHz, sampleRate_);
    dampR_.setFreq (p.lpfHz, sampleRate_);

    feedbackGain_ = p.feedback;
}

// ============================================================================
// AtmosphereCloud — public interface
// ============================================================================

void AtmosphereCloud::prepare (double sampleRate, int /*maxBlockSize*/)
{
    sampleRate_ = sampleRate;

    // Input diffusers — sized by their fixed kDiffMs values
    for (int i = 0; i < kNumDiff; ++i)
    {
        const int samp = static_cast<int> (msToSamp (kDiffMs[i], sampleRate)) + 1;
        diffusers_[i].prepare (samp);
    }

    // Tank delay lines: VALLEY is the longest (97ms). 110ms ceiling covers
    // all 5 presets at any sample rate up to 192kHz with headroom.
    const int maxTankSamp = static_cast<int> (sampleRate * 0.11) + 8;
    tankL_.prepare (maxTankSamp);
    tankR_.prepare (maxTankSamp);

    // Modulated all-pass lines: VALLEY max = 31ms nominal + 2ms LFO depth = 33ms.
    // 40ms ceiling provides safe margin for all presets and sample rates.
    const int maxModAPSamp = static_cast<int> (sampleRate * 0.04) + 8;
    modAPL_.prepare (maxModAPSamp);
    modAPR_.prepare (maxModAPSamp);

    reset();
}

void AtmosphereCloud::reset ()
{
    for (auto& d : diffusers_) d.reset();
    tankL_.reset();    tankR_.reset();
    modAPL_.reset();   modAPR_.reset();
    dampL_.reset();    dampR_.reset();
    fbL_ = 0.0f;
    fbR_ = 0.0f;
    lastPreset_ = -1;
}

void AtmosphereCloud::process (juce::AudioBuffer<float>& buffer,
                                float spatialX, float /*spatialY*/,
                                int   presetIdx, float wetMix)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    const int numCh      = buffer.getNumChannels();
    if (numSamples == 0 || numCh == 0 || wetMix < 1e-6f) return;

    presetIdx = juce::jlimit (0, kNumPresets - 1, presetIdx);
    if (presetIdx != lastPreset_)
    {
        applyPreset (presetIdx);
        lastPreset_ = presetIdx;
    }

    // Spatial panning of the wet field via radar X coordinate
    const float panAngle = (spatialX + 1.0f) * 0.25f * juce::MathConstants<float>::pi;
    const float panL     = std::cos (panAngle);
    const float panR     = std::sin (panAngle);

    const float* inL  = buffer.getReadPointer  (0);
    const float* inR  = numCh > 1 ? buffer.getReadPointer  (1) : inL;
    float*       outL = buffer.getWritePointer (0);
    float*       outR = numCh > 1 ? buffer.getWritePointer (1) : nullptr;

    constexpr float kDenormBias = 1.0e-15f;

    for (int s = 0; s < numSamples; ++s)
    {
        const float dryL = inL[s];
        const float dryR = inR[s];

        // ── A: Input diffusion ───────────────────────────────────────────────
        float diffused = (dryL + dryR) * 0.5f + kDenormBias;
        for (auto& d : diffusers_)
            diffused = d.process (diffused);

        // ── B+C: Figure-Eight cross-coupled tanks ────────────────────────────
        // tanh applied to cross-feedback paths (STEP D) before injection.
        const float inToL = diffused + std::tanh (fbR_ * feedbackGain_);
        const float inToR = diffused + std::tanh (fbL_ * feedbackGain_);

        // Left tank: ModAP → push into long delay → read → LP damp
        const float apOutL   = modAPL_.process (inToL);
        tankL_.push (apOutL);
        const float tankOutL = dampL_.process (tankL_.read (tankLDelay_));

        // Right tank: ModAP → push into long delay → read → LP damp
        const float apOutR   = modAPR_.process (inToR);
        tankR_.push (apOutR);
        const float tankOutR = dampR_.process (tankR_.read (tankRDelay_));

        // Update cross-feedback state for next sample
        fbL_ = tankOutL;
        fbR_ = tankOutR;

        // ── Stereo wet output with spatial panning ───────────────────────────
        const float wetL = tankOutL * panL + tankOutR * (1.0f - panL) * 0.3f;
        const float wetR = tankOutR * panR + tankOutL * (1.0f - panR) * 0.3f;

        outL[s] = dryL * (1.0f - wetMix) + wetL * wetMix * 2.0f;
        if (outR)
            outR[s] = dryR * (1.0f - wetMix) + wetR * wetMix * 2.0f;
    }
}
