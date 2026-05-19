#include "GranularEngine.h"
#include <cmath>

void GranularEngine::prepare (double sampleRate, int /*blockSize*/)
{
    sampleRate_ = sampleRate;
    currentMidiNote_ = 60;
    reset();
}

void GranularEngine::reset()
{
    for (auto& g : grains_)
        g = { false, 0.0, 0.0, 0, 0, 0.0f };

    for (auto& a : grainPosAtomic_)
        a.store (-1.0f, std::memory_order_relaxed);

    schedulerPhase_ = 0.0;
}

void GranularEngine::setNote (int note) noexcept
{
    currentMidiNote_ = note;
}

void GranularEngine::loadBuffer (GranularSampleBuffer::Ptr newBuffer)
{
    {
        const juce::SpinLock::ScopedLockType sl (bufferLock_);
        currentBuffer_ = std::move (newBuffer);
    }
    reset();
}

void GranularEngine::processBlock (juce::AudioBuffer<float>& output,
                                    float density,
                                    float sizeSec,
                                    float spray,
                                    float pitch,
                                    float level,
                                    float scatter,
                                    float startNorm,
                                    float endNorm,
                                    float fadeInFrac,
                                    float fadeOutFrac,
                                    bool  loop)
{
    if (density <= 0.01f)
    {
        for (auto& a : grainPosAtomic_)
            a.store (-1.0f, std::memory_order_relaxed);
        return;
    }

    GranularSampleBuffer::Ptr buf;
    {
        const juce::SpinLock::ScopedLockType sl (bufferLock_);
        buf = currentBuffer_;
    }

    if (buf == nullptr || buf->audio.getNumSamples() < 2) return;

    const int bufLen = buf->audio.getNumSamples();

    startNorm = juce::jlimit (0.0f, 0.99f, startNorm);
    endNorm   = juce::jlimit (startNorm + 0.01f, 1.0f, endNorm);

    const int numOut     = output.getNumChannels();
    const int numSamples = output.getNumSamples();
    const int srcCh      = buf->audio.getNumChannels();

    for (int s = 0; s < numSamples; ++s)
    {
        schedulerPhase_ += static_cast<double> (density) / sampleRate_;

        while (schedulerPhase_ >= 1.0)
        {
            schedulerPhase_ -= 1.0;
            spawnGrain (*buf, sizeSec, spray, pitch, scatter,
                        startNorm, endNorm, loop);
        }

        for (int gi = 0; gi < kMaxGrains; ++gi)
        {
            Grain& g = grains_[gi];
            if (!g.active) continue;

            const float env = grainEnvelope (g.samplesDone, g.totalSamples,
                                              fadeInFrac, fadeOutFrac)
                               * g.gain * level;

            for (int ch = 0; ch < numOut; ++ch)
            {
                const int src = (ch < srcCh) ? ch : 0;
                output.addSample (ch, s, readLinear (buf->audio, src, g.readPos) * env);
            }

            g.readPos  += g.readSpeed;
            g.samplesDone++;

            if (g.samplesDone >= g.totalSamples)
            {
                g.active = false;
                grainPosAtomic_[gi].store (-1.0f, std::memory_order_relaxed);
            }
            else
            {
                const float normPos = static_cast<float> (g.readPos) / static_cast<float> (bufLen);
                grainPosAtomic_[gi].store (juce::jlimit (0.0f, 1.0f, normPos),
                                           std::memory_order_relaxed);
            }
        }
    }
}

void GranularEngine::spawnGrain (const GranularSampleBuffer& buf,
                                  float sizeSec,
                                  float spray,
                                  float pitch,
                                  float scatter,
                                  float startNorm,
                                  float endNorm,
                                  bool  loop) noexcept
{
    int slot = -1;
    for (int i = 0; i < kMaxGrains; ++i)
        if (!grains_[i].active) { slot = i; break; }

    if (slot < 0) return;

    const int bufLen = buf.audio.getNumSamples();
    const float regionLen = endNorm - startNorm;

    float rawPos = startNorm + (nextRandom() * 0.5f + 0.5f) * regionLen * spray;

    if (loop)
    {
        while (rawPos > endNorm)   rawPos -= regionLen;
        while (rawPos < startNorm) rawPos += regionLen;
    }
    else
    {
        rawPos = juce::jlimit (startNorm, endNorm, rawPos);
    }

    const double readPos = static_cast<double> (rawPos) * bufLen;
    const int totalSamples = juce::jmax (16, static_cast<int> (sizeSec * sampleRate_));

    const double srcRatio   = buf.sourceSampleRate / sampleRate_;
    const double midiPitch  = std::pow (2.0, (static_cast<double> (currentMidiNote_) - 60.0) / 12.0);
    const float  jitterSemi = scatter * nextRandom();
    const double pitchJitter = std::pow (2.0, static_cast<double> (jitterSemi) / 12.0);
    const double readSpeed  = midiPitch * static_cast<double> (pitch) * pitchJitter * srcRatio;

    grains_[slot] = { true, readPos, readSpeed, totalSamples, 0, 1.0f };
}

float GranularEngine::grainEnvelope (int n, int N, float fadeInFrac, float fadeOutFrac) noexcept
{
    if (N <= 1) return 0.0f;

    const float minFade = 0.02f;
    fadeInFrac  = juce::jmax (minFade, fadeInFrac);
    fadeOutFrac = juce::jmax (minFade, fadeOutFrac);

    const float fadeTot = fadeInFrac + fadeOutFrac;
    if (fadeTot > 1.0f)
    {
        const float inv = 1.0f / fadeTot;
        fadeInFrac  *= inv;
        fadeOutFrac *= inv;
    }

    const int fadeInSamples  = static_cast<int> (fadeInFrac  * N);
    const int fadeOutSamples = static_cast<int> (fadeOutFrac * N);
    const int releaseStart   = N - fadeOutSamples;

    if (n < fadeInSamples)
    {
        const float t = static_cast<float> (n) / static_cast<float> (fadeInSamples);
        return 0.5f * (1.0f - std::cos (juce::MathConstants<float>::pi * t));
    }

    if (n >= releaseStart)
    {
        const float t = static_cast<float> (n - releaseStart)
                        / static_cast<float> (fadeOutSamples);
        return 0.5f * (1.0f + std::cos (juce::MathConstants<float>::pi * t));
    }

    return 1.0f;
}

float GranularEngine::readLinear (const juce::AudioBuffer<float>& buf,
                                   int channel, double pos) noexcept
{
    const int N = buf.getNumSamples();
    pos = juce::jlimit (0.0, static_cast<double> (N - 1), pos);

    const int   i0   = static_cast<int> (pos);
    const int   i1   = juce::jmin (i0 + 1, N - 1);
    const float frac = static_cast<float> (pos - static_cast<double> (i0));

    const float* data = buf.getReadPointer (channel);
    return data[i0] + frac * (data[i1] - data[i0]);
}

float GranularEngine::nextRandom() noexcept
{
    rngState_ ^= rngState_ << 13;
    rngState_ ^= rngState_ >> 17;
    rngState_ ^= rngState_ << 5;
    return static_cast<float> (static_cast<int32_t> (rngState_)) * (1.0f / 2147483648.0f);
}

int GranularEngine::getActiveGrainPositions (float* posOut, int maxOut) const noexcept
{
    int count = 0;
    for (int i = 0; i < kMaxGrains && count < maxOut; ++i)
    {
        const float p = grainPosAtomic_[i].load (std::memory_order_relaxed);
        if (p >= 0.0f) posOut[count++] = p;
    }
    return count;
}

bool GranularEngine::hasBuffer() const noexcept
{
    const juce::SpinLock::ScopedLockType sl (bufferLock_);
    return currentBuffer_ != nullptr;
}

int GranularEngine::getBufferNumSamples() const noexcept
{
    const juce::SpinLock::ScopedLockType sl (bufferLock_);
    return currentBuffer_ != nullptr ? currentBuffer_->audio.getNumSamples() : 0;
}

double GranularEngine::getBufferSampleRate() const noexcept
{
    const juce::SpinLock::ScopedLockType sl (bufferLock_);
    return currentBuffer_ != nullptr ? currentBuffer_->sourceSampleRate : 44100.0;
}

GranularSampleBuffer::Ptr GranularEngine::getDisplayBuffer() const noexcept
{
    const juce::SpinLock::ScopedLockType sl (bufferLock_);
    return currentBuffer_;
}
