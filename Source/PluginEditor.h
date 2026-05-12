#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"

// ============================================================================
//  LOOKANDFEEL — LARGE SKEUOMORPHIC (Global dome knobs, ~58px)
//  Stage 1: Satin-chrome chassis aesthetic.
//  Radial brushed-metal body, specular at 11-o'clock, Electric Cyan pointer.
// ============================================================================
class SkeuomorphicLookAndFeel : public juce::LookAndFeel_V4
{
public:
    SkeuomorphicLookAndFeel()
    {
        setColour (juce::Slider::rotarySliderFillColourId,    juce::Colour (0xFF00F0FF));
        setColour (juce::Slider::rotarySliderOutlineColourId, juce::Colours::black.withAlpha (0.5f));
    }

    void drawButtonBackground (juce::Graphics& g, juce::Button& btn, const juce::Colour&,
                               bool highlighted, bool down) override
    {
        const auto  b      = btn.getLocalBounds().toFloat().reduced (0.5f);
        const float corner = 8.0f;   // Design Bible: internal components 8px radius
        g.setColour (juce::Colours::black.withAlpha (0.65f));
        g.fillRoundedRectangle (b, corner);
        if (down)
        {
            g.setColour (juce::Colour (0xff040808));
            g.fillRoundedRectangle (b.reduced (1.0f), corner);
        }
        else
        {
            juce::ColourGradient grad (
                juce::Colour (highlighted ? 0xff1f2d2a : 0xff162220), 0.0f, b.getY(),
                juce::Colour (highlighted ? 0xff0d1614 : 0xff090f0e), 0.0f, b.getBottom(), false);
            g.setGradientFill (grad);
            g.fillRoundedRectangle (b.reduced (1.0f), corner);
        }
        // Bevel: top-left white 40%, bottom-right black 50%
        juce::ColourGradient bevel (
            juce::Colours::white.withAlpha (0.40f), b.getX(), b.getY(),
            juce::Colours::black.withAlpha (0.50f), b.getRight(), b.getBottom(), false);
        g.setGradientFill (bevel);
        g.drawRoundedRectangle (b.reduced (0.5f), corner - 0.5f, 1.5f);
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
                           const float rotaryStartAngle, const float rotaryEndAngle,
                           juce::Slider& slider) override
    {
        const auto  fill    = slider.findColour (juce::Slider::rotarySliderFillColourId);
        const auto  outline = slider.findColour (juce::Slider::rotarySliderOutlineColourId);
        const auto  bounds  = juce::Rectangle<float> (x, y, width, height).reduced (6.0f);
        const float size    = juce::jmin (bounds.getWidth(), bounds.getHeight());
        const auto  sq      = bounds.withSizeKeepingCentre (size, size);
        const float r       = size * 0.5f;
        const float cx      = sq.getCentreX();
        const float cy      = sq.getCentreY();
        const float lineW   = 3.0f;
        const float arcR    = r - lineW * 2.0f;
        const float toAngle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

        // Track arc
        juce::Path trackArc;
        trackArc.addCentredArc (cx, cy, arcR, arcR, 0.0f, rotaryStartAngle, rotaryEndAngle, true);
        g.setColour (outline);
        g.strokePath (trackArc, juce::PathStrokeType (lineW, juce::PathStrokeType::curved,
                                                       juce::PathStrokeType::rounded));

        // Value arc
        if (sliderPos > 0.0f)
        {
            juce::Path valueArc;
            valueArc.addCentredArc (cx, cy, arcR, arcR, 0.0f, rotaryStartAngle, toAngle, true);
            g.setColour (fill);
            g.strokePath (valueArc, juce::PathStrokeType (lineW, juce::PathStrokeType::curved,
                                                           juce::PathStrokeType::rounded));
            g.setColour (fill.withAlpha (0.22f));
            g.strokePath (valueArc, juce::PathStrokeType (lineW * 2.2f, juce::PathStrokeType::curved,
                                                           juce::PathStrokeType::rounded));
        }

        // Knob body — radial brushed-metal gradient
        const auto kb = sq.reduced (lineW * 3.5f);
        const float kr = kb.getHeight() * 0.5f;

        juce::ColourGradient bodyGrad (
            juce::Colour (0xFF707478), cx, cy,
            juce::Colour (0xFF1C1E24), cx + kr * 0.85f, cy + kr * 0.85f, true);
        g.setGradientFill (bodyGrad);
        g.fillEllipse (kb);

        // Specular at 11 o'clock (≈ -150° from +X axis in screen coords)
        const float specAngle = -juce::MathConstants<float>::pi * 5.0f / 6.0f;
        const float specX = cx + kr * 0.42f * std::cos (specAngle);
        const float specY = cy + kr * 0.42f * std::sin (specAngle);
        juce::ColourGradient specGrad (
            juce::Colours::white.withAlpha (0.50f), specX, specY,
            juce::Colours::transparentWhite, cx + kr * 0.1f, cy, true);
        g.setGradientFill (specGrad);
        g.fillEllipse (kb);

        // Bevel ring
        g.setColour (juce::Colours::white.withAlpha (0.15f));
        g.drawEllipse (kb.reduced (0.5f), 1.0f);
        g.setColour (juce::Colours::black.withAlpha (0.55f));
        g.drawEllipse (kb, 0.5f);

        // Pointer — Electric Cyan line
        const float pLen   = kr * 0.50f;
        const float pStart = -kr * 0.88f;
        juce::Path  p;
        p.addRoundedRectangle (-1.4f, pStart, 2.8f, pLen, 0.8f);
        p.applyTransform (juce::AffineTransform::rotation (toAngle).translated (cx, cy));
        g.setColour (juce::Colour (0xFF00F0FF));
        g.fillPath (p);
        g.setColour (juce::Colour (0xFF00F0FF).withAlpha (0.32f));
        g.strokePath (p, juce::PathStrokeType (3.0f));
    }
};

// ============================================================================
//  LOOKANDFEEL — MICRO KNOB (7-knob rows, ~30px)
//  Derives from SkeuomorphicLookAndFeel so button rendering is inherited.
//  Stage 1: radial brushed metal, specular at 11 o'clock, cyan pointer.
// ============================================================================
class MicroKnobLAF : public SkeuomorphicLookAndFeel
{
public:
    void setArcColour (juce::Colour c) { arcColour = c; }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                           float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                           juce::Slider& /*slider*/) override
    {
        const auto  bounds  = juce::Rectangle<float> (x, y, width, height).reduced (2.0f);
        const float sz      = juce::jmin (bounds.getWidth(), bounds.getHeight());
        const auto  sq      = bounds.withSizeKeepingCentre (sz, sz);
        const float r       = sz * 0.5f;
        const float cx      = sq.getCentreX();
        const float cy      = sq.getCentreY();
        const float lineW   = 1.5f;
        const float arcR    = r - lineW * 2.5f;
        const float toAngle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

        // Track arc
        juce::Path trackArc;
        trackArc.addCentredArc (cx, cy, arcR, arcR, 0.0f, rotaryStartAngle, rotaryEndAngle, true);
        g.setColour (juce::Colours::black.withAlpha (0.50f));
        g.strokePath (trackArc, juce::PathStrokeType (lineW));

        // Value arc + outer glow
        if (sliderPos > 0.001f)
        {
            juce::Path valueArc;
            valueArc.addCentredArc (cx, cy, arcR, arcR, 0.0f, rotaryStartAngle, toAngle, true);
            g.setColour (arcColour);
            g.strokePath (valueArc, juce::PathStrokeType (lineW, juce::PathStrokeType::curved,
                                                           juce::PathStrokeType::rounded));
            g.setColour (arcColour.withAlpha (0.22f));
            g.strokePath (valueArc, juce::PathStrokeType (lineW * 3.0f, juce::PathStrokeType::curved,
                                                           juce::PathStrokeType::rounded));
        }

        // Knob body — radial brushed-metal
        const auto  kb = sq.reduced (lineW * 3.5f);
        const float kr = kb.getHeight() * 0.5f;

        juce::ColourGradient bodyGrad (
            juce::Colour (0xFF606468), cx, cy,
            juce::Colour (0xFF1E2024), cx + kr * 0.85f, cy + kr * 0.85f, true);
        g.setGradientFill (bodyGrad);
        g.fillEllipse (kb);

        // Specular at 11 o'clock
        const float specAngle = -juce::MathConstants<float>::pi * 5.0f / 6.0f;
        const float specX = cx + kr * 0.38f * std::cos (specAngle);
        const float specY = cy + kr * 0.38f * std::sin (specAngle);
        juce::ColourGradient specGrad (
            juce::Colours::white.withAlpha (0.42f), specX, specY,
            juce::Colours::transparentWhite, cx + kr * 0.15f, cy, true);
        g.setGradientFill (specGrad);
        g.fillEllipse (kb);

        // Bevel ring
        g.setColour (juce::Colours::white.withAlpha (0.10f));
        g.drawEllipse (kb.reduced (0.5f), 0.75f);
        g.setColour (juce::Colours::black.withAlpha (0.50f));
        g.drawEllipse (kb, 0.5f);

        // Pointer — Electric Cyan
        const float pLen   = kr * 0.55f;
        const float pStart = -kr * 0.90f;
        juce::Path  p;
        p.addRoundedRectangle (-1.0f, pStart, 2.0f, pLen, 0.5f);
        p.applyTransform (juce::AffineTransform::rotation (toAngle).translated (cx, cy));
        g.setColour (juce::Colour (0xFF00F0FF));
        g.fillPath (p);
        g.setColour (juce::Colour (0xFF00F0FF).withAlpha (0.30f));
        g.strokePath (p, juce::PathStrokeType (2.0f));
    }

private:
    juce::Colour arcColour { juce::Colour (0xFF00F0FF) };
};

// ============================================================================
//  ENVELOPE MONITOR — non-interactable ADSR shape display (green OEL screen)
//  Preserved from Phase 5.
// ============================================================================
class EnvelopeMonitor : public juce::Component
{
public:
    EnvelopeMonitor (juce::AudioProcessorValueTreeState& a) : apvts (a) {}

    void paint (juce::Graphics& g) override
    {
        const float attack  = apvts.getRawParameterValue ("adsr_attack")->load();
        const float decay   = apvts.getRawParameterValue ("adsr_decay")->load();
        const float sustain = apvts.getRawParameterValue ("adsr_sustain")->load();
        const float release = apvts.getRawParameterValue ("adsr_release")->load();

        auto b = getLocalBounds().reduced (2).toFloat();
        const juce::Colour green (0xFF00FF66);

        g.setColour (juce::Colour (0xff020202));
        g.fillRoundedRectangle (b, 4.0f);
        g.setColour (green.withAlpha (0.5f));
        g.drawRoundedRectangle (b.reduced (0.5f), 4.0f, 1.0f);

        g.setColour (green.withAlpha (0.04f));
        for (float scanY = b.getY() + 2.0f; scanY < b.getBottom(); scanY += 3.0f)
            g.drawHorizontalLine ((int) scanY, b.getX() + 2.0f, b.getRight() - 2.0f);

        const float sustainDisplayTime = 0.4f;
        const float total = attack + decay + sustainDisplayTime + release;
        const float w     = b.getWidth() - 4.0f;
        const float x0    = b.getX() + 2.0f;
        const float yBot  = b.getBottom() - 3.0f;
        const float yPeak = b.getY() + 3.0f;
        const float ySust = yPeak + (1.0f - sustain) * (yBot - yPeak);

        const float aW = w * (attack             / total);
        const float dW = w * (decay              / total);
        const float sW = w * (sustainDisplayTime / total);
        const float rW = w * (release            / total);

        juce::Path env;
        env.startNewSubPath (x0,                           yBot);
        env.lineTo          (x0 + aW,                      yPeak);
        env.lineTo          (x0 + aW + dW,                 ySust);
        env.lineTo          (x0 + aW + dW + sW,            ySust);
        env.lineTo          (x0 + aW + dW + sW + rW,       yBot);

        juce::Path fill = env;
        fill.lineTo (x0 + w, yBot);
        fill.closeSubPath();
        g.setColour (green.withAlpha (0.07f));
        g.fillPath (fill);

        g.setColour (green.withAlpha (0.20f));
        g.strokePath (env, juce::PathStrokeType (3.5f, juce::PathStrokeType::curved,
                                                  juce::PathStrokeType::rounded));
        g.setColour (green.withAlpha (0.90f));
        g.strokePath (env, juce::PathStrokeType (1.5f, juce::PathStrokeType::curved,
                                                  juce::PathStrokeType::rounded));
    }

private:
    juce::AudioProcessorValueTreeState& apvts;
};

// ============================================================================
//  RADAR COMPONENT — interactive binaural spatial field (Stage 1)
//
//  Square module container with 20px corner radius.
//  Source orb constrained within the rounded boundary.
//  Implementation lives in RadarComponent.cpp.
// ============================================================================
class RadarComponent : public juce::Component
{
public:
    RadarComponent (juce::AudioProcessorValueTreeState& apvts,
                    std::atomic<float>&                 outputLevelRef);

    void paint     (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp   (const juce::MouseEvent&) override;

private:
    juce::Colour getAtmosphereTint() const;
    void         updateSpatialFromPoint (juce::Point<float> localPos);

    juce::AudioProcessorValueTreeState& apvts;
    std::atomic<float>&                 outputLevel;
    bool                                isDragging = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RadarComponent)
};

// ============================================================================
//  MAIN EDITOR
// ============================================================================
class OraclePadAudioProcessorEditor : public juce::AudioProcessorEditor,
                                       public juce::Timer
{
public:
    OraclePadAudioProcessorEditor (OraclePadAudioProcessor&);
    ~OraclePadAudioProcessorEditor() override;

    void paint   (juce::Graphics&) override;
    void resized () override;
    void timerCallback() override;

private:
    // ── Helpers ──────────────────────────────────────────────────────────────
    juce::Colour getAtmosphereTint() const;
    void         flashBanner (const juce::String& paramName, float value,
                              const juce::String& unit);

    // Draws a Design Bible module container (20px radius, 2px inner bevel).
    void drawModuleContainer (juce::Graphics& g,
                              juce::Rectangle<int> bounds,
                              juce::Colour         accentColour,
                              const char*          title) const;

    // ── Processor reference ───────────────────────────────────────────────────
    OraclePadAudioProcessor& audioProcessor;

    // ── LookAndFeel instances (must outlive every control) ────────────────────
    SkeuomorphicLookAndFeel largeLAF;   // Global dome knobs (~58px)
    MicroKnobLAF            microLAF;   // OSC 1 row — amber
    MicroKnobLAF            subLAF;     // Sub strip — green
    MicroKnobLAF            osc2LAF;    // OSC 2 row — purple
    MicroKnobLAF            adsrLAF;    // ADSR strip — cyan

    // ── Sub-components ────────────────────────────────────────────────────────
    EnvelopeMonitor envelopeMonitor;
    RadarComponent  radarComponent;

    // ── OSC 1 — 3×2 Grid (VOL MORPH TILT / SPREAD CUTOFF RES) ───────────────
    // osc1MixKnob is attached (preset compat) but hidden in Stage 1.
    juce::Slider osc1VolKnob, osc1MorphKnob, osc1MixKnob, osc1TiltKnob,
                 osc1SpreadKnob, osc1CutoffKnob, osc1ResKnob;

    // ── Sub OSC Module ────────────────────────────────────────────────────────
    juce::Slider subVolKnob;
    juce::Slider subShapeKnob;

    // ── OSC 2 — 3×2 Grid (VOL CUTOFF RES / DENSITY SIZE SPEED) ─────────────
    // granJitterKnob is attached (preset compat) but hidden in Stage 1.
    juce::Slider osc2VolKnob, osc2CutoffKnob, osc2ResKnob,
                 granDensityKnob, granSizeKnob, granJitterKnob, granSpeedKnob;

    // ── Global ────────────────────────────────────────────────────────────────
    juce::Slider     masterGainKnob, vintageModeKnob;
    juce::TextButton prevAtmoBtn, nextAtmoBtn;
    juce::TextButton prevPresetBtn, nextPresetBtn, savePresetBtn;

    // ── ADSR ──────────────────────────────────────────────────────────────────
    juce::Slider attackKnob, decayKnob, sustainKnob, releaseKnob;

    // ── APVTS Attachments ─────────────────────────────────────────────────────
    using SliderAtt = juce::AudioProcessorValueTreeState::SliderAttachment;

    // OSC 1
    SliderAtt osc1VolAtt, osc1MorphAtt, osc1MixAtt, osc1TiltAtt,
              osc1SpreadAtt, osc1CutoffAtt, osc1ResAtt;
    // Sub
    SliderAtt subVolAtt;
    SliderAtt subShapeAtt;
    // OSC 2
    SliderAtt osc2VolAtt, osc2CutoffAtt, osc2ResAtt,
              granDensityAtt, granSizeAtt, granJitterAtt, granSpeedAtt;
    // Global
    SliderAtt masterGainAtt, vintageModeAtt;
    // ADSR
    SliderAtt attackAtt, decayAtt, sustainAtt, releaseAtt;

    // ── Knob Labels ───────────────────────────────────────────────────────────
    juce::Label osc1Labels[7];   // VOL MORPH MIX(hidden) TILT SPREAD CUTOFF RES
    juce::Label subLabels[2];    // LEVEL  SHAPE
    juce::Label osc2Labels[7];   // VOL CUT RES DENS SIZE JITR(hidden) SPD
    juce::Label adsrLabels[4];   // ATK DEC SUS REL

    // ── Banner flash state ────────────────────────────────────────────────────
    juce::String bannerText;
    int          bannerCountdown = 0;

    // ── Runtime ───────────────────────────────────────────────────────────────
    float lastOutputLevel = 0.0f;

    std::unique_ptr<juce::FileChooser> savePresetChooser;

    // ── Stage 1 Design Bible — OEL-90 Rack Manifesto ─────────────────────────
    static constexpr int kW       = 920;
    static constexpr int kH       = 620;
    static constexpr int kBannerH = 65;
    static constexpr int kPad     = 8;
    static constexpr int kModR    = 20;   // Design Bible: 20px module corner radius
    static constexpr int kMicro   = 30;
    static constexpr int kLarge   = 58;
    static constexpr int kLabelH  = 16;

    // Module regions (absolute window coordinates)
    static constexpr int kOsc1X = kPad;
    static constexpr int kOsc1Y = kBannerH + kPad;       // 73
    static constexpr int kOsc1W = 330;
    static constexpr int kOsc1H = 165;

    static constexpr int kSubX  = kOsc1X + kOsc1W + kPad;  // 346
    static constexpr int kSubY  = kBannerH + kPad;
    static constexpr int kSubW  = 186;
    static constexpr int kSubH  = 165;

    static constexpr int kOsc2X = kPad;
    static constexpr int kOsc2Y = kOsc1Y + kOsc1H + kPad;  // 246
    static constexpr int kOsc2W = 524;
    static constexpr int kOsc2H = 165;

    // Radar square: fills right column
    static constexpr int kRadarX  = kSubX + kSubW + kPad;   // 540
    static constexpr int kRadarY  = kBannerH + kPad;         // 73
    static constexpr int kRadarSz = kW - kRadarX - kPad;     // 372

    static constexpr int kOutX  = kRadarX;
    static constexpr int kOutY  = kRadarY + kRadarSz + kPad;  // 453
    static constexpr int kOutW  = kRadarSz;
    static constexpr int kOutH  = 88;

    static constexpr int kAdsrX = kPad;
    static constexpr int kAdsrY = kOutY + kOutH + kPad;       // 549
    static constexpr int kAdsrW = kW - kPad * 2;               // 904
    static constexpr int kAdsrH = kH - kAdsrY - kPad;         // 63

    // Design Bible colour constants
    static constexpr juce::uint32 kSatinSilver = 0xFFC0C0C0;
    static constexpr juce::uint32 kModuleBg    = 0xFF1A1C22;
    static constexpr juce::uint32 kElecCyan    = 0xFF00F0FF;
    static constexpr juce::uint32 kTextColor   = 0xFFE0E0E0;
    static constexpr juce::uint32 kAmberOsc1   = 0xFFFFAA00;
    static constexpr juce::uint32 kPurpleOsc2  = 0xFFAA44FF;
    static constexpr juce::uint32 kGreenSub    = 0xFF00FF88;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OraclePadAudioProcessorEditor)
};
