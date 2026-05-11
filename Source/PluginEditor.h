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
        setColour (juce::Slider::rotarySliderFillColourId,    juce::Colour (0xFF00F0FF));
        setColour (juce::Slider::rotarySliderOutlineColourId, juce::Colours::black.withAlpha (0.5f));
    }

    void drawButtonBackground (juce::Graphics& g, juce::Button& btn, const juce::Colour&,
                               bool highlighted, bool down) override
    {
        const auto  b      = btn.getLocalBounds().toFloat().reduced (0.5f);
        const float corner = 3.0f;
        // Inset shell shadow
        g.setColour (juce::Colours::black.withAlpha (0.65f));
        g.fillRoundedRectangle (b, corner);
        if (down)
        {
            g.setColour (juce::Colour (0xff040808));
            g.fillRoundedRectangle (b.reduced (1.0f), corner);
            // Pressed inset shadow at top edge
            g.setColour (juce::Colours::black.withAlpha (0.55f));
            g.drawLine (b.getX() + 2.0f, b.getY() + 1.5f, b.getRight() - 2.0f, b.getY() + 1.5f, 1.0f);
        }
        else
        {
            juce::ColourGradient grad (
                juce::Colour (highlighted ? 0xff1f2d2a : 0xff162220), 0.0f, b.getY(),
                juce::Colour (highlighted ? 0xff0d1614 : 0xff090f0e), 0.0f, b.getBottom(), false);
            g.setGradientFill (grad);
            g.fillRoundedRectangle (b.reduced (1.0f), corner);
            // Top bevel highlight
            g.setColour (juce::Colours::white.withAlpha (0.09f));
            g.drawLine (b.getX() + 2.5f, b.getY() + 1.5f, b.getRight() - 2.5f, b.getY() + 1.5f, 1.0f);
            // Bottom shadow
            g.setColour (juce::Colours::black.withAlpha (0.30f));
            g.drawLine (b.getX() + 2.5f, b.getBottom() - 1.5f, b.getRight() - 2.5f, b.getBottom() - 1.5f, 1.0f);
        }
        // Cyan border glow
        g.setColour (juce::Colour (0xFF00F0FF).withAlpha (highlighted ? 0.38f : 0.20f));
        g.drawRoundedRectangle (b.reduced (0.5f), corner - 0.5f, 0.75f);
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
                           const float rotaryStartAngle, const float rotaryEndAngle, juce::Slider& slider) override
    {
        auto outline = slider.findColour (juce::Slider::rotarySliderOutlineColourId);
        auto fill    = slider.findColour (juce::Slider::rotarySliderFillColourId);

        auto bounds = juce::Rectangle<float> (x, y, width, height).reduced (8.0f);
        auto size   = juce::jmin (bounds.getWidth(), bounds.getHeight());
        auto squareBounds = bounds.withSizeKeepingCentre (size, size);

        auto radius  = size / 2.0f;
        auto toAngle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);
        auto centreX = squareBounds.getCentreX();
        auto centreY = squareBounds.getCentreY();
        auto lineW   = 3.0f;
        auto arcBounds = squareBounds.reduced (lineW);

        g.setColour (juce::Colours::black.withAlpha (0.4f));
        g.drawEllipse (arcBounds.expanded (1.0f), 1.5f);
        g.setColour (juce::Colours::white.withAlpha (0.05f));
        g.drawEllipse (arcBounds.expanded (2.5f), 1.0f);

        juce::Path backgroundArc;
        backgroundArc.addCentredArc (centreX, centreY, radius - lineW * 2.0f, radius - lineW * 2.0f,
                                     0.0f, rotaryStartAngle, rotaryEndAngle, true);
        g.setColour (outline);
        g.strokePath (backgroundArc, juce::PathStrokeType (lineW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        if (sliderPos > 0.0f)
        {
            juce::Path valueArc;
            valueArc.addCentredArc (centreX, centreY, radius - lineW * 2.0f, radius - lineW * 2.0f,
                                    0.0f, rotaryStartAngle, toAngle, true);
            g.setColour (fill);
            g.strokePath (valueArc, juce::PathStrokeType (lineW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            g.setColour (fill.withAlpha (0.2f));
            g.strokePath (valueArc, juce::PathStrokeType (lineW * 2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        auto knobBounds = arcBounds.reduced (lineW * 2.5f);
        juce::Path knobPath;
        knobPath.addEllipse (knobBounds);

        juce::ColourGradient chassisKnob (juce::Colour (0xff70757e), 0, knobBounds.getY(),
                                          juce::Colour (0xff2c2e35), 0, knobBounds.getBottom(), false);
        g.setGradientFill (chassisKnob);
        g.fillPath (knobPath);

        g.setColour (juce::Colours::white.withAlpha (0.15f));
        g.drawEllipse (knobBounds.reduced (1.0f), 1.0f);
        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.drawEllipse (knobBounds.reduced (0.5f), 1.0f);

        juce::ColourGradient glaze (juce::Colours::white.withAlpha (0.1f),
                                    knobBounds.getCentreX() - radius * 0.3f, knobBounds.getY(),
                                    juce::Colours::transparentWhite, centreX, centreY, true);
        g.setGradientFill (glaze);
        g.fillEllipse (knobBounds);

        juce::Path p;
        auto pointerLength    = radius * 0.4f;
        auto pointerThickness = 3.0f;
        p.addRoundedRectangle (-pointerThickness * 0.5f, -radius * 0.8f, pointerThickness, pointerLength, 1.5f);
        p.applyTransform (juce::AffineTransform::rotation (toAngle).translated (centreX, centreY));

        g.setColour (juce::Colours::black.withAlpha (0.7f));
        g.fillPath (p);
        g.setColour (juce::Colours::white.withAlpha (0.05f));
        g.strokePath (p, juce::PathStrokeType (0.5f));
    }
};

// ==============================================================================
// ENVELOPE MONITOR — non-interactable ADSR shape display (green OEL screen)
// ==============================================================================
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

        // Screen pit
        g.setColour (juce::Colour (0xff020202));
        g.fillRoundedRectangle (b, 3.0f);

        // Green border glow
        g.setColour (green.withAlpha (0.5f));
        g.drawRoundedRectangle (b.reduced (0.5f), 3.0f, 1.0f);
        g.setColour (green.withAlpha (0.12f));
        g.drawRoundedRectangle (b.expanded (1.5f), 4.0f, 3.0f);

        // Scan lines
        g.setColour (green.withAlpha (0.04f));
        for (float scanY = b.getY() + 2.0f; scanY < b.getBottom(); scanY += 3.0f)
            g.drawHorizontalLine ((int) scanY, b.getX() + 2.0f, b.getRight() - 2.0f);

        // ADSR path — proportional time segments; S shown as fixed 0.4 s equivalent
        const float sustainDisplayTime = 0.4f;
        const float total = attack + decay + sustainDisplayTime + release;
        const float w     = b.getWidth() - 4.0f;
        const float x0    = b.getX() + 2.0f;
        const float yBot  = b.getBottom() - 3.0f;
        const float yPeak = b.getY()      + 3.0f;
        const float ySust = yPeak + (1.0f - sustain) * (yBot - yPeak);

        const float aW = w * (attack             / total);
        const float dW = w * (decay              / total);
        const float sW = w * (sustainDisplayTime / total);
        const float rW = w * (release            / total);

        juce::Path env;
        env.startNewSubPath (x0,                             yBot);
        env.lineTo          (x0 + aW,                        yPeak);
        env.lineTo          (x0 + aW + dW,                   ySust);
        env.lineTo          (x0 + aW + dW + sW,              ySust);
        env.lineTo          (x0 + aW + dW + sW + rW,         yBot);

        // Filled area under curve
        juce::Path fill = env;
        fill.lineTo (x0 + w, yBot);
        fill.closeSubPath();
        g.setColour (green.withAlpha (0.07f));
        g.fillPath (fill);

        // Outer bloom
        g.setColour (green.withAlpha (0.2f));
        g.strokePath (env, juce::PathStrokeType (3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Main line
        g.setColour (green.withAlpha (0.9f));
        g.strokePath (env, juce::PathStrokeType (1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

private:
    juce::AudioProcessorValueTreeState& apvts;
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

    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp   (const juce::MouseEvent& e) override;

private:
    juce::Rectangle<int> getRadarScreenBounds() const;
    void updateSpatialFromMouse (juce::Point<int> screenPos);
    juce::Colour getAtmosphereTint() const;

    OraclePadAudioProcessor& audioProcessor;

    // Sliders must be declared before their SliderAttachments (init order).
    juce::Slider gainKnob;
    juce::Slider morphKnob, subKnob, timbreKnob, spreadKnob;
    juce::Slider attackKnob, decayKnob, sustainKnob, releaseKnob;

    juce::AudioProcessorValueTreeState::SliderAttachment gainAttachment;
    juce::AudioProcessorValueTreeState::SliderAttachment morphAttach, subAttach, timbreAttach, spreadAttach;
    juce::AudioProcessorValueTreeState::SliderAttachment attackAttach, decayAttach, sustainAttach, releaseAttach;

    SkeuomorphicLookAndFeel customLookAndFeel;
    EnvelopeMonitor         envelopeMonitor;

    juce::TextButton prevPresetButton, nextPresetButton, savePresetButton;
    juce::TextButton prevAtmoButton, nextAtmoButton;

    std::unique_ptr<juce::FileChooser> savePresetChooser;

    juce::Label bannerLabel, osc1Label, osc2Label, arpLabel, radarLabel, globalSettingsLabel;
    juce::Label morphLabel, subLabel, timbreLabel, spreadLabel;
    juce::Label adsrLabel;
    juce::Label attackLabel, decayLabel, sustainLabel, releaseLabel;

    juce::Colour cyberBlue = juce::Colour (0xFF00F0FF);

    static constexpr int kPad = 10;  // uniform padding for all layout boundaries

    float lastOutputLevel = 0.0f;
    bool  isDraggingOrb   = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OraclePadAudioProcessorEditor)
};
