#include "PluginProcessor.h"
#include "PluginEditor.h"

OraclePadAudioProcessor::OraclePadAudioProcessor()
     : AudioProcessor (BusesProperties()
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
       apvts (*this, nullptr, "Parameters",
       {
           std::make_unique<juce::AudioParameterFloat>  ("master_gain",   "Master Gain",         0.0f,  1.0f,  0.8f),
           std::make_unique<juce::AudioParameterFloat>  ("osc1_morph",    "OSC1 Morph",          0.0f,  1.0f,  0.0f),
           std::make_unique<juce::AudioParameterFloat>  ("osc1_sub",      "OSC1 Sub Blend",
               juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f, 0.4f), 0.0f),
           std::make_unique<juce::AudioParameterFloat>  ("osc1_tilt",     "OSC1 Timbre Tilt",   -1.0f,  1.0f,  0.0f),
           std::make_unique<juce::AudioParameterFloat>  ("osc1_spread",   "OSC1 Voice Spread",   0.0f,  1.0f,  0.0f),
           std::make_unique<juce::AudioParameterFloat>  ("adsr_attack",   "Attack",
               juce::NormalisableRange<float> (0.001f, 4.0f, 0.001f, 0.4f), 0.8f),
           std::make_unique<juce::AudioParameterFloat>  ("adsr_decay",    "Decay",
               juce::NormalisableRange<float> (0.001f, 2.0f, 0.001f, 0.4f), 0.4f),
           std::make_unique<juce::AudioParameterFloat>  ("adsr_sustain",  "Sustain",             0.0f,  1.0f,  0.85f),
           std::make_unique<juce::AudioParameterFloat>  ("adsr_release",  "Release",
               juce::NormalisableRange<float> (0.001f, 5.0f, 0.001f, 0.4f), 1.5f),
           std::make_unique<juce::AudioParameterFloat>  ("spatial_x",        "Spatial X",          -1.0f,  1.0f,  0.0f),
           std::make_unique<juce::AudioParameterFloat>  ("spatial_y",        "Spatial Y",          -1.0f,  1.0f,  0.0f),
           std::make_unique<juce::AudioParameterInt>    ("atmosphere_state", "Atmosphere State",   0, 4, 0),
           std::make_unique<juce::AudioParameterChoice> ("weather_mode",     "Weather Mode",
               juce::StringArray {"Forest", "Valley", "Temple", "Hut", "Basement"}, 0)
       }),
       presetManager (apvts)
{
    padParams.attack  = 0.8f;   // slow bloom — the "lush" pad character
    padParams.decay   = 0.4f;
    padParams.sustain = 0.85f;
    padParams.release = 1.5f;   // cinematic tail
}

OraclePadAudioProcessor::~OraclePadAudioProcessor() {}

const juce::String OraclePadAudioProcessor::getName() const      { return JucePlugin_Name; }
bool OraclePadAudioProcessor::acceptsMidi()  const               { return true; }
bool OraclePadAudioProcessor::producesMidi() const               { return false; }
double OraclePadAudioProcessor::getTailLengthSeconds() const     { return 2.0; }
int  OraclePadAudioProcessor::getNumPrograms()                   { return 1; }
int  OraclePadAudioProcessor::getCurrentProgram()                { return 0; }
void OraclePadAudioProcessor::setCurrentProgram (int)            {}
const juce::String OraclePadAudioProcessor::getProgramName (int) { return {}; }
void OraclePadAudioProcessor::changeProgramName (int, const juce::String&) {}

// ---------------------------------------------------------------------------
// Wavetable construction
//
// All harmonics are seeded via sin(k * theta) using a single shared phase
// accumulator. This guarantees phase coherence across the entire spectrum:
// no inter-harmonic offsets, no frequency-dependent group delay variation.
// Blauert & Laws (1978) show that group delay > ~1.6ms at 1-2kHz is audible
// as "smearing" — our approach keeps it at zero within the wavetable itself.
//
// 64 harmonics covers ~65Hz (MIDI 42, F#2) before aliasing. Sufficient for
// pad fundamentals. Higher notes naturally use fewer harmonics of the table.
// ---------------------------------------------------------------------------
void OraclePadAudioProcessor::buildWavetables()
{
    for (auto& wt : osc1Wavetables)
        wt.assign (wavetableSize, 0.0f);

    const float twoPi = juce::MathConstants<float>::twoPi;

    // [0] Saw: classic bright, rich pad base — full 1/k series
    for (int i = 0; i < wavetableSize; ++i)
    {
        float phase = twoPi * (float) i / (float) wavetableSize;
        for (int k = 1; k <= numHarmonics; ++k)
            osc1Wavetables[0][i] += (1.0f / (float) k) * std::sin ((float) k * phase);
    }

    // [1] Square: odd harmonics only — hollow, "woody" vintage character
    for (int i = 0; i < wavetableSize; ++i)
    {
        float phase = twoPi * (float) i / (float) wavetableSize;
        for (int k = 1; k <= numHarmonics; k += 2)
            osc1Wavetables[1][i] += (1.0f / (float) k) * std::sin ((float) k * phase);
    }

    // [2] Sub: pure sine — subOsc runs at half the note frequency
    for (int i = 0; i < wavetableSize; ++i)
        osc1Wavetables[2][i] = std::sin (twoPi * (float) i / (float) wavetableSize);

    // Normalise all tables to [-1, 1]
    for (auto& wt : osc1Wavetables)
    {
        float peak = 0.0f;
        for (auto s : wt) peak = std::max (peak, std::abs (s));
        if (peak > 0.0f)
            for (auto& s : wt) s /= peak;
    }
}

// ---------------------------------------------------------------------------
// Voice allocation
// ---------------------------------------------------------------------------
OracleVoice* OraclePadAudioProcessor::findFreeVoice() noexcept
{
    for (auto& v : voices) if (!v.active)              return &v;
    for (auto& v : voices) if (!v.envelope.isActive()) return &v;
    // Steal oldest — explicitly zero it so the new note starts clean
    voices[0].active     = false;
    voices[0].noteNumber = -1;
    voices[0].envelope.reset();
    return &voices[0];
}

OracleVoice* OraclePadAudioProcessor::findVoiceForNote (int noteNum) noexcept
{
    for (auto& v : voices)
        if (v.active && v.noteNumber == noteNum) return &v;
    return nullptr;
}

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------
void OraclePadAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    buildWavetables();

    for (int i = 0; i < numVoices; ++i)
    {
        voices[i].voiceIndex = i;
        voices[i].active     = false;
        voices[i].noteNumber = -1;
        voices[i].tiltZ      = 0.0f;
        voices[i].envelope.setSampleRate (sampleRate);
        voices[i].envelope.setParameters (padParams);
        voices[i].envelope.reset();
    }

    spatializer.prepare (sampleRate);
    atmosphereEngine.prepare (sampleRate, samplesPerBlock);
    chorus.prepare  (sampleRate);
    wetHpf.prepare  (sampleRate, 300.0f);

    outputLevel.store (0.0f, std::memory_order_relaxed);
}

void OraclePadAudioProcessor::releaseResources() {}

// ---------------------------------------------------------------------------
// Audio rendering
// Polyphonic, sample-accurate MIDI, zero heap allocation in the audio thread.
// Stereo output: even voices spread left, odd voices spread right.
// ---------------------------------------------------------------------------
void OraclePadAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                             juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();

    // Read all parameters once per block — safe from the audio thread.
    const float masterGain = apvts.getRawParameterValue ("master_gain")->load();
    const float morphAmt   = apvts.getRawParameterValue ("osc1_morph")->load();
    const float subBlend   = apvts.getRawParameterValue ("osc1_sub")->load();
    const float tiltParam  = apvts.getRawParameterValue ("osc1_tilt")->load();
    const float spreadAmt  = apvts.getRawParameterValue ("osc1_spread")->load();

    // Live ADSR — read knobs every block so voices respond immediately
    padParams.attack  = apvts.getRawParameterValue ("adsr_attack")->load();
    padParams.decay   = apvts.getRawParameterValue ("adsr_decay")->load();
    padParams.sustain = apvts.getRawParameterValue ("adsr_sustain")->load();
    padParams.release = apvts.getRawParameterValue ("adsr_release")->load();
    for (auto& v : voices)
        if (v.active) v.envelope.setParameters (padParams);

    // Belt-and-suspenders: any voice that crossed idle between blocks gets zeroed now.
    // This catches edge cases where envelope.isActive() flipped at a block boundary.
    for (auto& v : voices)
        if (v.active && !v.envelope.isActive())
        {
            v.active = false; v.noteNumber = -1; v.envelope.reset();
        }

    // Spatial parameters — read once per block.
    const float spatialX = apvts.getRawParameterValue ("spatial_x")->load();
    const float spatialY = apvts.getRawParameterValue ("spatial_y")->load();

    // Pythagorean distance — 0 at origin, 1 at unit circle edge, clamped.
    const float distance = juce::jlimit (0.0f, 1.0f,
        std::sqrt (spatialX * spatialX + spatialY * spatialY));

    // ITD: Woodworth formula — 0.8 ms max; azimuth (X) only.
    const float itdDelaySmp = juce::jlimit (
        0.0f, (float) (Spatializer::kDelayBuf - 2),
        std::abs (spatialX) * (float) (0.0008 * currentSampleRate));

    const bool panRight = (spatialX >  0.001f);
    const bool panLeft  = (spatialX < -0.001f);

    // ILD: fc sweeps from Nyquist (|x|=0, transparent) to 1200 Hz (|x|=1, full head shadow).
    const float ildAmt  = std::abs (spatialX);
    const float ildFc   = (float) (currentSampleRate * 0.5) * (1.0f - ildAmt) + 1200.0f * ildAmt;
    const float ildCoef = std::exp (-juce::MathConstants<float>::twoPi * ildFc
                                    / (float) currentSampleRate);

    // Distance LPF: air-absorption driven by Pythagorean distance.
    const float distFc   = 20000.0f * std::pow (0.1f, distance);
    const float distCoef = std::exp (-juce::MathConstants<float>::twoPi * distFc
                                     / (float) currentSampleRate);

    // DRR: driven by Pythagorean distance (0 = close/dry, 1 = far/wet).
    const float dryGain = 1.0f - distance * 0.7f;
    const float wetGain = distance;

    // Rear pinna notch — Audio EQ Cookbook peaking EQ, fc=3kHz, Q=2.
    // Depth scales linearly with how far behind the listener the source is.
    // When rearAmt=0 (front or centre) A=1, filter is transparent (all-pass).
    const float rearAmt    = juce::jlimit (0.0f, 1.0f, -spatialY);
    const float notchGaindB = -10.0f * rearAmt;
    const float notchA     = std::pow (10.0f, notchGaindB / 40.0f);
    const float notchW0    = juce::MathConstants<float>::twoPi * 3000.0f / (float) currentSampleRate;
    const float notchCosW0 = std::cos (notchW0);
    const float notchAlpha = std::sin (notchW0) / (2.0f * 2.0f);   // Q = 2
    const float notchA0inv = 1.0f / (1.0f + notchAlpha / notchA);
    const float notchB0    = (1.0f + notchAlpha * notchA) * notchA0inv;
    const float notchB1    = (-2.0f * notchCosW0)          * notchA0inv;
    const float notchB2    = (1.0f - notchAlpha * notchA)  * notchA0inv;
    const float notchA1    = (-2.0f * notchCosW0)          * notchA0inv;
    const float notchA2    = (1.0f - notchAlpha / notchA)  * notchA0inv;

    const auto& sawTbl = osc1Wavetables[0];
    const auto& sqTbl  = osc1Wavetables[1];
    const auto& subTbl = osc1Wavetables[2];

    const int numSamples     = buffer.getNumSamples();
    const int numOutChannels = getTotalNumOutputChannels();

    // Pre-fetch wet-buffer pointers — filled per-sample, processed block-level by reverb.
    float* const wetBufL = atmosphereEngine.wetBuf.getWritePointer (0);
    float* const wetBufR = atmosphereEngine.wetBuf.getWritePointer (1);

    auto midiIter = midiMessages.begin();
    auto midiEnd  = midiMessages.end();

    for (int i = 0; i < numSamples; ++i)
    {
        // Sample-accurate MIDI dispatch
        while (midiIter != midiEnd && (*midiIter).samplePosition <= i)
        {
            auto msg = (*midiIter).getMessage();

            if (msg.isNoteOn())
            {
                const int   note = msg.getNoteNumber();
                const float freq = 440.0f * std::pow (2.0f, ((float) note - 69.0f) / 12.0f);

                // Force-kill any existing instance of this note.
                // Calling noteOff() on a near-zero release recalculates the rate from the
                // current tiny output, making it effectively infinite — the ghost note bug.
                if (auto* existing = findVoiceForNote (note))
                {
                    existing->active     = false;
                    existing->noteNumber = -1;
                    existing->envelope.reset();
                }

                if (auto* v = findFreeVoice())
                {
                    // Spread: give even voices a tiny phase lead for micro-stereo texture.
                    float phaseOffset = 0.0f;
                    if (spreadAmt > 0.001f && v->voiceIndex % 2 == 0)
                        phaseOffset = spreadAmt * 0.04f * (float) wavetableSize;

                    v->start (note, freq, currentSampleRate, wavetableSize, padParams, phaseOffset);
                }
            }
            else if (msg.isNoteOff())
            {
                if (auto* v = findVoiceForNote (msg.getNoteNumber()))
                    v->release();
            }
            else if (msg.isAllNotesOff() || msg.isAllSoundOff())
            {
                for (auto& v : voices) v.active = false;
            }

            ++midiIter;
        }

        // Render all voices into a stereo sum.
        float sumL = 0.0f, sumR = 0.0f;
        for (auto& v : voices)
        {
            auto [l, r] = v.renderStereo (sawTbl, sqTbl, subTbl,
                                          morphAmt, subBlend, tiltParam, spreadAmt);
            sumL += l;
            sumR += r;
        }

        // ── BBD Chorus (OSC 1 voice sum → stereo widening) ───────────────────
        auto [cL, cR] = chorus.process (sumL, sumR);
        sumL = cL;  sumR = cR;

        // ── Rear pinna notch (biquad, Direct Form II Transposed) ─────────────
        // Transparent when rearAmt=0 (A=1 → b0=b2=1, b1=a1, b2=a2 → all-pass).
        if (rearAmt > 0.001f)
        {
            const float yL    = notchB0 * sumL + spatializer.rearZ1_L;
            spatializer.rearZ1_L = notchB1 * sumL - notchA1 * yL + spatializer.rearZ2_L;
            spatializer.rearZ2_L = notchB2 * sumL - notchA2 * yL;
            sumL = yL;

            const float yR    = notchB0 * sumR + spatializer.rearZ1_R;
            spatializer.rearZ1_R = notchB1 * sumR - notchA1 * yR + spatializer.rearZ2_R;
            spatializer.rearZ2_R = notchB2 * sumR - notchA2 * yR;
            sumR = yR;
        }

        // ── Binaural spatial processing ──────────────────────────────────────

        // Write current sum into the circular delay buffers.
        spatializer.delayBufL[spatializer.writePos] = sumL;
        spatializer.delayBufR[spatializer.writePos] = sumR;

        // ITD: delay the contralateral ear.
        float spatL, spatR;
        if (panRight)
        {
            spatR = sumR;
            spatL = spatializer.readDelayed (spatializer.delayBufL, itdDelaySmp);
        }
        else if (panLeft)
        {
            spatL = sumL;
            spatR = spatializer.readDelayed (spatializer.delayBufR, itdDelaySmp);
        }
        else { spatL = sumL; spatR = sumR; }

        spatializer.writePos = (spatializer.writePos + 1) % Spatializer::kDelayBuf;

        // ILD: 1-pole LP on the contralateral ear.
        // Keep the ipsilateral state warm to avoid transients during rapid panning.
        if (panRight)
        {
            spatializer.ildZ_L = ildCoef * spatializer.ildZ_L + (1.0f - ildCoef) * spatL;
            spatL += ildAmt * (spatializer.ildZ_L - spatL);
            spatializer.ildZ_R = spatR;
        }
        else if (panLeft)
        {
            spatializer.ildZ_R = ildCoef * spatializer.ildZ_R + (1.0f - ildCoef) * spatR;
            spatR += ildAmt * (spatializer.ildZ_R - spatR);
            spatializer.ildZ_L = spatL;
        }
        else
        {
            spatializer.ildZ_L = spatL; spatializer.ildZ_R = spatR;
        }

        // Wet: accumulate into pre-allocated buffer for block-level atmosphere reverb.
        wetBufL[i] = spatL * wetGain;
        wetBufR[i] = spatR * wetGain;

        // Dry: distance air-absorption LPF + DRR gain.
        // Tanh/masterGain deferred until after reverb is mixed in (post-loop).
        spatializer.distZ_L = distCoef * spatializer.distZ_L + (1.0f - distCoef) * spatL;
        spatializer.distZ_R = distCoef * spatializer.distZ_R + (1.0f - distCoef) * spatR;

        if (numOutChannels >= 1) buffer.getWritePointer (0)[i] = spatializer.distZ_L * dryGain;
        if (numOutChannels >= 2) buffer.getWritePointer (1)[i] = spatializer.distZ_R * dryGain;
    }

    // ── Abbey Road HPF: cut sub-200 Hz mud from the reverb send ─────────────
    wetHpf.processBlock (atmosphereEngine.wetBuf.getWritePointer (0),
                         atmosphereEngine.wetBuf.getWritePointer (1),
                         numSamples);

    // ── Atmosphere reverb (wet path, block-level) ────────────────────────────
    const int atmoState = (int) apvts.getRawParameterValue ("atmosphere_state")->load();
    atmosphereEngine.processWetBlock (numSamples, atmoState);

    // ── Master bus: sum dry + reverb, then soft-clip and gain ────────────────
    if (numOutChannels >= 1)
    {
        auto*       chanL = buffer.getWritePointer (0);
        const auto* revL  = atmosphereEngine.wetBuf.getReadPointer (0);
        for (int i = 0; i < numSamples; ++i)
            chanL[i] = std::tanh ((chanL[i] + revL[i]) * 0.3f) * masterGain;
    }
    if (numOutChannels >= 2)
    {
        auto*       chanR = buffer.getWritePointer (1);
        const auto* revR  = atmosphereEngine.wetBuf.getReadPointer (1);
        for (int i = 0; i < numSamples; ++i)
            chanR[i] = std::tanh ((chanR[i] + revR[i]) * 0.3f) * masterGain;
    }

    // Leaky peak follower — feeds the editor's radar visualiser (message thread reads this).
    float peak = 0.0f;
    if (numOutChannels > 0)
    {
        auto* ch0 = buffer.getReadPointer (0);
        for (int i = 0; i < numSamples; ++i)
            peak = std::max (peak, std::abs (ch0[i]));
    }
    const float prev = outputLevel.load (std::memory_order_relaxed);
    outputLevel.store (prev * 0.85f + peak * 0.15f, std::memory_order_relaxed);
}

// ---------------------------------------------------------------------------
// Editor / state
// ---------------------------------------------------------------------------
bool OraclePadAudioProcessor::hasEditor() const { return true; }
juce::AudioProcessorEditor* OraclePadAudioProcessor::createEditor()
{
    return new OraclePadAudioProcessorEditor (*this);
}

void OraclePadAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void OraclePadAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState != nullptr && xmlState->hasTagName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new OraclePadAudioProcessor(); }
