#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"

// ==============================================================================
// CUSTOM HARDWARE LOOKANDFEEL (SKEUOMORPHIC CONTROLS)
// ==============================================================================
class SkeuomorphicLookAndFeel : public juce::LookAndFeel_V4
{
public:
    SkeuomorphicLookAndFeel()
    {
        // Define the default Cyber Blue glow color for control rings
        setColour (juce::Slider::rotarySliderFillColourId, juce::Colour (0xFF00F0FF));
        setColour (juce::Slider::rotarySliderOutlineColourId, juce::Colours::black.withAlpha (0.5f));
    }

    // Renders a physical metallic knob with a glow ring and pointer
    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
                           const float rotaryStartAngle, const float rotaryEndAngle, juce::Slider& slider) override
    {
        auto outline = slider.findColour (juce::Slider::rotarySliderOutlineColourId);
        auto fill    = slider.findColour (juce::Slider::rotarySliderFillColourId);

        // 1. Setup variables and bounds
        auto bounds = juce::Rectangle<float>(x, y, width, height).reduced (8.0f);
        
        // --- FIX: Force the bounds to be a PERFECT SQUARE in the center ---
        auto size = juce::jmin (bounds.getWidth(), bounds.getHeight());
        auto squareBounds = bounds.withSizeKeepingCentre (size, size);

        auto radius = size / 2.0f;
        auto toAngle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);
        auto centreX = squareBounds.getCentreX();
        auto centreY = squareBounds.getCentreY();
        auto lineW = 3.0f; // Width of the Cyber Blue glow ring

        auto arcBounds = squareBounds.reduced (lineW);

        // 2. Draw outer physical ring (the part sunken into the chassis)
        g.setColour (juce::Colours::black.withAlpha (0.4f));
        g.drawEllipse (arcBounds.expanded (1.0f), 1.5f);
        g.setColour (juce::Colours::white.withAlpha (0.05f));
        g.drawEllipse (arcBounds.expanded (2.5f), 1.0f);

        // 3. Draw Cyber Blue glow background arc
        juce::Path backgroundArc;
        backgroundArc.addCentredArc (centreX, centreY, radius - lineW * 2.0f, radius - lineW * 2.0f,
                                     0.0f, rotaryStartAngle, rotaryEndAngle, true);
        g.setColour (outline);
        g.strokePath (backgroundArc, juce::PathStrokeType (lineW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // 4. Draw Cyber Blue active/fill arc
        if (sliderPos > 0.0f)
        {
            juce::Path valueArc;
            valueArc.addCentredArc (centreX, centreY, radius - lineW * 2.0f, radius - lineW * 2.0f,
                                    0.0f, rotaryStartAngle, toAngle, true);
            g.setColour (fill);
            g.strokePath (valueArc, juce::PathStrokeType (lineW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

            // SOFT BLOOM EFFECT
            g.setColour (fill.withAlpha (0.2f));
            g.strokePath (valueArc, juce::PathStrokeType (lineW * 2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        // 5. Draw the physical metallic Knob cap
        auto knobBounds = arcBounds.reduced (lineW * 2.5f);
        juce::Path knobPath;
        knobPath.addEllipse (knobBounds);

        // Chrome gradient (lighter top, darker bottom)
        juce::ColourGradient chassisKnob(juce::Colour (0xff70757e), 0, knobBounds.getY(),
                                        juce::Colour (0xff2c2e35), 0, knobBounds.getBottom(), false);
        g.setGradientFill (chassisKnob);
        g.fillPath (knobPath);

        // Subtle inner reflection ring
        g.setColour (juce::Colours::white.withAlpha (0.15f));
        g.drawEllipse (knobBounds.reduced (1.0f), 1.0f);
        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.drawEllipse (knobBounds.reduced (0.5f), 1.0f);

        // Realistic Top Glaze (catching light from top-left)
        juce::ColourGradient glaze(juce::Colours::white.withAlpha (0.1f), knobBounds.getCentreX() - radius*0.3f, knobBounds.getY(),
                                   juce::Colours::transparentWhite, centreX, centreY, true);
        g.setGradientFill (glaze);
        g.fillEllipse (knobBounds);

        // 6. Draw the indicator line (pointer)
        juce::Path p;
        auto pointerLength = radius * 0.4f;
        auto pointerThickness = 3.0f;
        p.addRoundedRectangle (-pointerThickness * 0.5f, -radius * 0.8f, pointerThickness, pointerLength, 1.5f);
        p.applyTransform (juce::AffineTransform::rotation (toAngle).translated (centreX, centreY));

        // Dark etched groove
        g.setColour (juce::Colours::black.withAlpha (0.7f));
        g.fillPath (p);
        // Subtle white edge where light catches (opposite side of rotation)
        g.setColour (juce::Colours::white.withAlpha (0.05f));
        g.strokePath (p, juce::PathStrokeType (0.5f));
    }
};

// ==============================================================================
// MAIN EDITOR CLASS
// ==============================================================================
class OraclePadAudioProcessorEditor : public juce::AudioProcessorEditor,
                                       public juce::Timer
{
public:
    OraclePadAudioProcessorEditor (OraclePadAudioProcessor&);
    ~OraclePadAudioProcessorEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;

private:
    OraclePadAudioProcessor& audioProcessor;

    // gainKnob must be declared before gainAttachment (initialisation order)
    juce::Slider gainKnob;
    juce::AudioProcessorValueTreeState::SliderAttachment gainAttachment;
    SkeuomorphicLookAndFeel customLookAndFeel;

    juce::Label statusLabel;
    juce::Label bannerLabel, osc1Label, osc2Label, arpLabel, radarLabel, globalSettingsLabel;
    juce::Colour cyberBlue = juce::Colour(0xFF00F0FF);

    float lastOutputLevel = 0.0f; // used by timerCallback to detect changes

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OraclePadAudioProcessorEditor)
};