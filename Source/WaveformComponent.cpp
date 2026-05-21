#include "WaveformComponent.h"
#include <cmath>

WaveformComponent::WaveformComponent (juce::AudioProcessorValueTreeState& apvts,
                                       GranularEngine& engine)
    : apvts_ (apvts), engine_ (engine)
{
    addAndMakeVisible (loopButton_);
    loopButton_.setClickingTogglesState (true);
    loopButton_.setColour (juce::TextButton::buttonColourId,   juce::Colour (0xFF252830));
    loopButton_.setColour (juce::TextButton::buttonOnColourId, juce::Colour (kCyanARGB).withAlpha (0.25f));
    loopButton_.setColour (juce::TextButton::textColourOffId,  juce::Colour (0xFF777788));
    loopButton_.setColour (juce::TextButton::textColourOnId,   juce::Colour (kCyanARGB));

    if (auto* p = apvts_.getParameter ("gran_loop"))
        loopAttachment_ = std::make_unique<juce::ButtonParameterAttachment> (*p, loopButton_);

    grainDisplayPos_.fill (-1.0f);
    startTimerHz (30);
}

WaveformComponent::~WaveformComponent()
{
    stopTimer();
}

void WaveformComponent::setWaveformData (GranularSampleBuffer::Ptr buffer)
{
    waveformBuffer_ = std::move (buffer);
    viewStart_ = 0.0;
    viewEnd_   = 1.0;
    repaint();
}

juce::Rectangle<int> WaveformComponent::waveArea() const
{
    return getLocalBounds().withTrimmedTop (kLoopBarH);
}

juce::Rectangle<int> WaveformComponent::buttonArea() const
{
    return getLocalBounds().withHeight (kLoopBarH).reduced (2, 2);
}

float WaveformComponent::normToPixel (float norm) const noexcept
{
    const float w    = static_cast<float> (waveArea().getWidth());
    const float vS   = static_cast<float> (viewStart_);
    const float vE   = static_cast<float> (viewEnd_);
    const float span = vE - vS;
    if (span <= 0.0f) return 0.0f;
    return (norm - vS) / span * w;
}

float WaveformComponent::pixelToNorm (float pixel) const noexcept
{
    const float w  = static_cast<float> (waveArea().getWidth());
    const float vS = static_cast<float> (viewStart_);
    const float vE = static_cast<float> (viewEnd_);
    if (w <= 0.0f) return vS;
    return vS + (pixel / w) * (vE - vS);
}

float WaveformComponent::markerNorm (const char* paramId) const noexcept
{
    if (auto* p = apvts_.getRawParameterValue (paramId))
        return p->load();
    return 0.0f;
}

void WaveformComponent::setMarkerNorm (const char* paramId, float value) noexcept
{
    if (auto* param = dynamic_cast<juce::RangedAudioParameter*> (
            apvts_.getParameter (paramId)))
        param->setValueNotifyingHost (param->convertTo0to1 (value));
}

float WaveformComponent::markerPixel (DragTarget t) const noexcept
{
    const float sN = markerNorm (kIdStart);
    const float eN = markerNorm (kIdEnd);
    const float rL = eN - sN;

    switch (t)
    {
        case DragTarget::Start:   return normToPixel (sN);
        case DragTarget::End:     return normToPixel (eN);
        case DragTarget::FadeIn:  return normToPixel (sN + markerNorm (kIdFadeIn)  * rL);
        case DragTarget::FadeOut: return normToPixel (eN - markerNorm (kIdFadeOut) * rL);
        default:                  return -1.0f;
    }
}

WaveformComponent::DragTarget WaveformComponent::hitTestMarker (int x) const
{
    const float fx = static_cast<float> (x);

    struct { DragTarget t; } order[] = {
        { DragTarget::Start   },
        { DragTarget::End     },
        { DragTarget::FadeIn  },
        { DragTarget::FadeOut },
    };

    for (auto& entry : order)
    {
        const float px = markerPixel (entry.t);
        if (px >= 0.0f && std::abs (fx - px) <= kMarkerHitZone)
            return entry.t;
    }
    return DragTarget::None;
}

void WaveformComponent::timerCallback()
{
    numActiveGrains_ = engine_.getActiveGrainPositions (
        grainDisplayPos_.data(),
        static_cast<int> (grainDisplayPos_.size()));
    repaint();
}

void WaveformComponent::resized()
{
    loopButton_.setBounds (buttonArea());
}

void WaveformComponent::paint (juce::Graphics& g)
{
    paintBackground  (g);
    paintWaveform    (g);
    paintMarkers     (g);
    paintGrainLines  (g);
    if (isDragOver_) paintDropOverlay (g);
}

void WaveformComponent::mouseDown (const juce::MouseEvent& e)
{
    const int localX = e.x - waveArea().getX();
    activeDrag_ = hitTestMarker (localX);
}

void WaveformComponent::mouseDrag (const juce::MouseEvent& e)
{
    if (activeDrag_ == DragTarget::None) return;

    const float localX = static_cast<float> (e.x - waveArea().getX());
    const float norm   = juce::jlimit (0.0f, 1.0f, pixelToNorm (localX));

    const float sN = markerNorm (kIdStart);
    const float eN = markerNorm (kIdEnd);
    const float rL = eN - sN;

    switch (activeDrag_)
    {
        case DragTarget::Start:
            setMarkerNorm (kIdStart, juce::jlimit (0.0f, eN - 0.005f, norm));
            break;

        case DragTarget::End:
            setMarkerNorm (kIdEnd, juce::jlimit (sN + 0.005f, 1.0f, norm));
            break;

        case DragTarget::FadeIn:
            if (rL > 0.0f)
                setMarkerNorm (kIdFadeIn,
                    juce::jlimit (0.0f, 0.5f, (norm - sN) / rL));
            break;

        case DragTarget::FadeOut:
            if (rL > 0.0f)
                setMarkerNorm (kIdFadeOut,
                    juce::jlimit (0.0f, 0.5f, (eN - norm) / rL));
            break;

        default: break;
    }
}

void WaveformComponent::mouseUp (const juce::MouseEvent&)
{
    activeDrag_ = DragTarget::None;
}

void WaveformComponent::mouseWheelMove (const juce::MouseEvent& e,
                                         const juce::MouseWheelDetails& wheel)
{
    if (waveformBuffer_ == nullptr) return;

    const float localX     = static_cast<float> (e.x - waveArea().getX());
    const float waveWidth  = static_cast<float> (waveArea().getWidth());
    const float cursorFrac = (waveWidth > 0.0f) ? localX / waveWidth : 0.5f;
    const float mouseNorm  = pixelToNorm (localX);

    const double zoomFactor = (wheel.deltaY > 0.0f) ? 0.75 : 1.333;
    const double newSpan    = juce::jlimit (kMinViewSpan,
                                            1.0,
                                            (viewEnd_ - viewStart_) * zoomFactor);

    viewStart_ = static_cast<double> (mouseNorm) - cursorFrac * newSpan;
    viewEnd_   = viewStart_ + newSpan;

    if (viewStart_ < 0.0) { viewEnd_ -= viewStart_; viewStart_ = 0.0; }
    if (viewEnd_   > 1.0) { viewStart_ -= (viewEnd_ - 1.0); viewEnd_ = 1.0; }
    viewStart_ = std::max (0.0, viewStart_);
    viewEnd_   = std::min (1.0, viewEnd_);

    repaint();
}

bool WaveformComponent::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (auto& path : files)
    {
        const juce::String ext = juce::File (path).getFileExtension().toLowerCase();
        if (ext == ".wav" || ext == ".mp3" || ext == ".aif" || ext == ".aiff"
            || ext == ".flac" || ext == ".ogg")
            return true;
    }
    return false;
}

void WaveformComponent::fileDragEnter (const juce::StringArray&, int, int)
{
    isDragOver_ = true;
    repaint();
}

void WaveformComponent::fileDragExit (const juce::StringArray&)
{
    isDragOver_ = false;
    repaint();
}

void WaveformComponent::filesDropped (const juce::StringArray& files, int, int)
{
    isDragOver_ = false;
    for (auto& path : files)
    {
        const juce::File f (path);
        const juce::String ext = f.getFileExtension().toLowerCase();
        if (ext == ".wav" || ext == ".mp3" || ext == ".aif" || ext == ".aiff"
            || ext == ".flac" || ext == ".ogg")
        {
            if (onFileDropped) onFileDropped (f);
            break;
        }
    }
}

void WaveformComponent::paintBackground (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();

    g.setColour (juce::Colour (kPanelARGB));
    g.fillRoundedRectangle (bounds, 10.0f);

    g.setColour (juce::Colour (0xFF24262E));
    g.fillRect  (0, kLoopBarH - 1, getWidth(), 1);

    const auto wa = waveArea();
    g.setColour (juce::Colour (0xFF22242C));
    g.drawHorizontalLine (wa.getY() + wa.getHeight() / 2,
                          (float)wa.getX(), (float)wa.getRight());
}

void WaveformComponent::paintWaveform (juce::Graphics& g)
{
    const auto wa = waveArea();

    if (waveformBuffer_ == nullptr || waveformBuffer_->audio.getNumSamples() < 2)
    {
        g.setColour (juce::Colour (kCyanARGB).withAlpha (0.25f));
        g.setFont   (juce::Font (juce::FontOptions{}.withHeight (12.0f)));
        g.drawText  ("Drop a .wav / .mp3 here",
                     wa, juce::Justification::centred, false);
        return;
    }

    const auto&  audio    = waveformBuffer_->audio;
    const int    bufLen   = audio.getNumSamples();
    const int    numCh    = audio.getNumChannels();
    const float  midY     = static_cast<float> (wa.getCentreY());
    const float  halfH    = static_cast<float> (wa.getHeight()) * 0.46f;
    const float  waveOffX = static_cast<float> (wa.getX());

    g.setColour (juce::Colour (kCyanARGB).withAlpha (0.75f));

    constexpr int kMaxSamplesPerPx = 256;

    for (int px = 0; px < wa.getWidth(); ++px)
    {
        const float nL = pixelToNorm (static_cast<float> (px));
        const float nR = pixelToNorm (static_cast<float> (px + 1));

        if (nR < 0.0f || nL > 1.0f) continue;

        const int s0 = juce::jlimit (0, bufLen - 1, static_cast<int> (nL * bufLen));
        const int s1 = juce::jlimit (s0 + 1, bufLen, static_cast<int> (nR * bufLen));

        float minV = 0.0f, maxV = 0.0f;
        const int blockLen = s1 - s0;
        const int step     = juce::jmax (1, blockLen / kMaxSamplesPerPx);

        for (int ch = 0; ch < numCh; ++ch)
        {
            const float* data = audio.getReadPointer (ch);
            for (int s = s0; s < s1; s += step)
            {
                const float v = data[s];
                if (v < minV) minV = v;
                if (v > maxV) maxV = v;
            }
        }

        const float yTop = midY - maxV * halfH;
        const float yBot = midY - minV * halfH;
        g.fillRect (juce::Rectangle<float> (
            waveOffX + static_cast<float> (px),
            yTop,
            1.0f,
            juce::jmax (1.0f, yBot - yTop)));
    }
}

void WaveformComponent::paintMarkers (juce::Graphics& g)
{
    const auto wa = waveArea();

    struct MarkerDef { DragTarget t; juce::uint32 colour; const char* name; };
    const MarkerDef defs[] = {
        { DragTarget::Start,   kStartARGB,   "S"  },
        { DragTarget::End,     kEndARGB,     "E"  },
        { DragTarget::FadeIn,  kFadeInARGB,  "FI" },
        { DragTarget::FadeOut, kFadeOutARGB, "FO" },
    };

    const float top    = static_cast<float> (wa.getY());
    const float bottom = static_cast<float> (wa.getBottom());

    for (auto& d : defs)
    {
        const float px = markerPixel (d.t);
        const float ax = static_cast<float> (wa.getX()) + px;

        if (px < -kMarkerHitZone || px > static_cast<float> (wa.getWidth()) + kMarkerHitZone)
            continue;

        const juce::Colour col (d.colour);

        g.setColour (col.withAlpha (0.85f));
        g.drawVerticalLine (static_cast<int> (ax), top, bottom);

        juce::Path tri;
        tri.addTriangle (ax - 5.0f, top, ax + 5.0f, top, ax, top + 8.0f);
        g.setColour (col);
        g.fillPath  (tri);

        g.setColour (col);
        g.setFont   (juce::Font (juce::FontOptions{}.withHeight (9.0f).withStyle ("Bold")));
        g.drawText  (d.name,
                     juce::Rectangle<float> (ax - 12.0f, top + 10.0f, 24.0f, 12.0f),
                     juce::Justification::centred, false);
    }

    {
        const float sX  = static_cast<float> (wa.getX()) + markerPixel (DragTarget::Start);
        const float fiX = static_cast<float> (wa.getX()) + markerPixel (DragTarget::FadeIn);
        if (fiX > sX)
        {
            g.setColour (juce::Colour (kFadeInARGB).withAlpha (0.08f));
            g.fillRect  (juce::Rectangle<float> (sX, top, fiX - sX, bottom - top));
        }
    }
    {
        const float foX = static_cast<float> (wa.getX()) + markerPixel (DragTarget::FadeOut);
        const float eX  = static_cast<float> (wa.getX()) + markerPixel (DragTarget::End);
        if (eX > foX)
        {
            g.setColour (juce::Colour (kFadeOutARGB).withAlpha (0.08f));
            g.fillRect  (juce::Rectangle<float> (foX, top, eX - foX, bottom - top));
        }
    }
}

void WaveformComponent::paintGrainLines (juce::Graphics& g)
{
    if (numActiveGrains_ <= 0) return;

    const auto   wa    = waveArea();
    const float  top   = static_cast<float> (wa.getY());
    const float  bot   = static_cast<float> (wa.getBottom());
    const float  wOffX = static_cast<float> (wa.getX());

    const double t = juce::Time::getMillisecondCounterHiRes() * 0.001;
    constexpr double kPhi = 1.6180339887;

    for (int i = 0; i < numActiveGrains_ && i < kGrainDisplaySlots; ++i)
    {
        const float pos = grainDisplayPos_[i];
        if (pos < 0.0f) continue;

        const float px = normToPixel (pos);
        if (px < 0.0f || px > static_cast<float> (wa.getWidth())) continue;

        const double phase = t * juce::MathConstants<double>::twoPi * 3.0
                             + static_cast<double> (i) * kPhi;
        const float alpha  = 0.55f + 0.40f * static_cast<float> (std::sin (phase));

        g.setColour (juce::Colour (kCyanARGB).withAlpha (juce::jlimit (0.15f, 1.0f, alpha)));
        g.drawVerticalLine (static_cast<int> (wOffX + px), top, bot);
    }
}

void WaveformComponent::paintDropOverlay (juce::Graphics& g)
{
    const auto wa = waveArea();

    g.setColour (juce::Colour (kCyanARGB).withAlpha (0.12f));
    g.fillRoundedRectangle (wa.toFloat(), 6.0f);

    g.setColour (juce::Colour (kCyanARGB).withAlpha (0.9f));
    g.setFont   (juce::Font (juce::FontOptions{}.withHeight (14.0f).withStyle ("Bold")));
    g.drawText  ("Drop .wav / .mp3", wa, juce::Justification::centred, false);

    juce::Path border;
    border.addRoundedRectangle (wa.toFloat().reduced (2.0f), 6.0f);
    juce::PathStrokeType stroke (1.5f);
    stroke.createDashedStroke (border, border,
                                std::array<float, 2>{ 6.0f, 4.0f }.data(), 2);
    g.setColour (juce::Colour (kCyanARGB).withAlpha (0.6f));
    g.fillPath  (border);
}
