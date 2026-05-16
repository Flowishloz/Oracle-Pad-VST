#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "WaveformComponent.h"

// ============================================================================
// SkeuomorphicLookAndFeel — base LAF shared by all OEL-90 knobs.
// Defined inline to prevent LNK2001 (no separate .cpp needed).
// ============================================================================
class SkeuomorphicLookAndFeel : public juce::LookAndFeel_V4
{
public:
    SkeuomorphicLookAndFeel()
    {
        setColour (juce::Slider::thumbColourId,               juce::Colour (0xFF00D1FF));
        setColour (juce::Slider::rotarySliderFillColourId,    juce::Colour (0xFF00D1FF));
        setColour (juce::Slider::backgroundColourId,          juce::Colour (0xFF1A1C22));
        setColour (juce::Label::textColourId,                 juce::Colour (0xFF00D1FF));
        setColour (juce::ResizableWindow::backgroundColourId, juce::Colour (0xFF0D0F14));
    }
};

// ============================================================================
// MicroKnobLAF — thin-arc rotary knob, Pioneer-Blue by default.
// Defined inline to prevent LNK2001 unresolved external errors.
// ============================================================================
class MicroKnobLAF : public SkeuomorphicLookAndFeel
{
public:
    void setArcColour (juce::Colour c) { arcColour = c; }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                           float sliderPos, float rotaryStartAngle,
                           float rotaryEndAngle, juce::Slider&) override
    {
        auto b = juce::Rectangle<float> ((float)x, (float)y,
                                          (float)width, (float)height).reduced (2.0f);
        float cx = b.getCentreX();
        float cy = b.getCentreY();
        float r  = (juce::jmin (b.getWidth(), b.getHeight()) * 0.5f) - 2.0f;
        float toAngle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.drawEllipse (cx - r, cy - r, r * 2.0f, r * 2.0f, 1.0f);

        juce::Path p;
        p.addCentredArc (cx, cy, r, r, 0.0f, rotaryStartAngle, toAngle, true);
        g.setColour (arcColour);
        g.strokePath (p, juce::PathStrokeType (2.0f,
                         juce::PathStrokeType::curved,
                         juce::PathStrokeType::rounded));
    }

private:
    juce::Colour arcColour { 0xFF00D1FF };
};

// ============================================================================
// RadarComponent — Spatial Radar UI (class declaration only).
// Implementation lives in RadarComponent.cpp.
// ============================================================================
class RadarComponent : public juce::Component
{
public:
    RadarComponent (juce::AudioProcessorValueTreeState& apvts,
                    std::atomic<float>&                 outputLevel);

    void paint     (juce::Graphics& g)         override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp   (const juce::MouseEvent&)   override;

private:
    juce::AudioProcessorValueTreeState& apvts;
    std::atomic<float>&                 outputLevel;
    bool                                isDragging = false;

    juce::Colour getAtmosphereTint()                    const;
    void         updateSpatialFromPoint (juce::Point<float> pos);
};

// ============================================================================
// OraclePadAudioProcessorEditor — OEL-90 main plugin window  (Phase 1)
// ============================================================================
class OraclePadAudioProcessorEditor : public juce::AudioProcessorEditor,
                                       public juce::Timer
{
public:
    OraclePadAudioProcessorEditor (OraclePadAudioProcessor&);
    ~OraclePadAudioProcessorEditor() override;

    void paint      (juce::Graphics&) override;
    void resized    ()                override;
    void timerCallback()              override;

    WaveformComponent waveformComponent;

private:
    OraclePadAudioProcessor& audioProcessor;

    MicroKnobLAF microLAF;

    RadarComponent radarComponent;

    // ── Header ───────────────────────────────────────────────────────────────
    // Main patch preset browser (top-left)
    juce::Label      titleLabel, presetLabel;
    juce::TextButton prevPresetBtn { "<" }, nextPresetBtn { ">" };
    // Atmosphere preset selector (top-right)
    juce::Label      atmoPresetLabel;
    juce::TextButton prevAtmoBtn { "<" }, nextAtmoBtn { ">" };

    // ── OSC 1 — 3-over-2 staggered grid (vol/morph/mix top | spread/cut bottom) ─
    juce::Slider osc1VolKnob,   osc1MorphKnob,  osc1MixKnob,
                 osc1SpreadKnob, osc1CutKnob;
    juce::Label  osc1VolLbl,    osc1MorphLbl,   osc1MixLbl,
                 osc1SpreadLbl, osc1CutLbl;

    // ── Sub section ───────────────────────────────────────────────────────────
    juce::Slider subLevelKnob, subShapeKnob, subOctaveKnob;
    juce::Label  subLevelLbl,  subShapeLbl,  subOctaveLbl;

    // ── OSC 2 granular control panel — 7 knobs below the waveform display ─────
    juce::Slider osc2VolKnob,    osc2CutoffKnob, osc2ResKnob;
    juce::Slider granDensityKnob, granSizeKnob,  granJitterKnob, granSpeedKnob;
    juce::Label  osc2VolLbl,     osc2CutoffLbl,  osc2ResLbl;
    juce::Label  granDensityLbl, granSizeLbl,    granJitterLbl,  granSpeedLbl;

    // ── ADSR section ──────────────────────────────────────────────────────────
    juce::Slider adsrAttackKnob, adsrDecayKnob, adsrSustainKnob, adsrReleaseKnob;
    juce::Label  adsrAttackLbl,  adsrDecayLbl,  adsrSustainLbl,  adsrReleaseLbl;
    // Envelope monitor — centred in the ADSR strip, flanked by A/D left, S/R right
    juce::Label  envelopeMonitor;

    // ── Atmosphere engine + dedicated Master block ────────────────────────────
    juce::Slider atmosMixKnob, atmosStateKnob;
    juce::Label  atmosMixLbl,  atmosStateLbl;
    juce::Slider vintageKnob,  masterGainKnob;
    juce::Label  vintageLbl,   masterGainLbl;

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::unique_ptr<SliderAttachment>
        aOsc1Vol, aOsc1Morph, aOsc1Mix, aOsc1Cut, aOsc1Spread,
        aSubLevel, aSubShape, aSubOctave,
        aGranDensity, aGranSize,
        aAdsrA, aAdsrD, aAdsrS, aAdsrR,
        aAtmosMix, aAtmosState, aVintage, aMasterGain;

    void setupKnob  (juce::Slider& s, const juce::String& tooltip);
    void setupLabel (juce::Label& l,  const juce::String& text);
    void updatePresetLabel();
    void updateAtmoLabel();
    void drawModuleContainer (juce::Graphics& g, int x, int y, int w, int h,
                              const juce::String& name);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OraclePadAudioProcessorEditor)
};
