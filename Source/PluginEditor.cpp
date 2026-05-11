#include "PluginProcessor.h"
#include "PluginEditor.h"

OraclePadAudioProcessorEditor::OraclePadAudioProcessorEditor (OraclePadAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    // Use standard clear fonts for faceplate text (looks printed/engraved)
    auto setupFaceplateLabel = [this](juce::Label& l, juce::String text, float size) {
        l.setText(text, juce::dontSendNotification);
        l.setFont (juce::FontOptions ("Futura", size, juce::Font::bold));
        l.setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.6f));
        addAndMakeVisible(l);
    };

    setupFaceplateLabel(osc1Label, "OSC 1: WAVE", 13.0f);
    setupFaceplateLabel(osc2Label, "OSC 2: SAMPLER", 13.0f);
    setupFaceplateLabel(arpLabel, "ORACLE ARPEGGIATOR", 13.0f);
    setupFaceplateLabel(globalSettingsLabel, "GLOBAL SETTINGS", 11.0f);

    // Use distinct "Retro Tech" font for Screen Titles (Banner & Radar)
    auto setupScreenLabel = [this](juce::Label& l, juce::String text, float size) {
        l.setText(text, juce::dontSendNotification);
        l.setFont (juce::FontOptions ("Futura", size, juce::Font::bold)); 
        l.setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.8f));
        addAndMakeVisible(l);
    };

    setupScreenLabel(bannerLabel, "ORACLE DISPLAY // SYSTEM ACTIVE", 18.0f);
    setupScreenLabel(radarLabel, "SPATIAL RADAR", 14.0f);

    // Configure the main knob (skeuomorphic) - renamed to gainKnob!
    gainKnob.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    gainKnob.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    gainKnob.setRange(0.0, 1.0, 0.01);
    gainKnob.setValue(0.5);
    gainKnob.setTextValueSuffix (" dB");
    // CRITICAL: Attach the custom look and feel
    gainKnob.setLookAndFeel (&customLookAndFeel);
    addAndMakeVisible(gainKnob);

    setSize (900, 500);
}

OraclePadAudioProcessorEditor::~OraclePadAudioProcessorEditor()
{
    // Detach look and feel on destruction
    gainKnob.setLookAndFeel (nullptr);
}

void OraclePadAudioProcessorEditor::paint (juce::Graphics& g)
{
    // *** DIAGNOSTIC: proves the DAW loaded this build — remove after confirmed ***
    g.fillAll (juce::Colours::red);
    return;
    // *** END DIAGNOSTIC ***

    auto& random = juce::Random::getSystemRandom();

    // 1. The Main Hardware Chassis (Dark cold metal gradient)
    juce::ColourGradient chassis(juce::Colour(0xff2A2D34), 0, 0, juce::Colour(0xff111215), 0, (float)getHeight(), false);
    chassis.addColour(0.5, juce::Colour(0xff1A1C20)); // Subtle central rise
    g.setGradientFill(chassis);
    g.fillAll();

    // --- NEW: Add physical texture (brushed metal grain/noise) ---
    for (int i = 0; i < 5000; ++i)
    {
        auto x = random.nextInt (getWidth());
        auto y = random.nextInt (getHeight());
        auto noiseAlpha = random.nextFloat() * 0.03f; // Very subtle
        g.setColour (juce::Colours::black.withAlpha (noiseAlpha));
        g.drawHorizontalLine (y, (float)x, (float)x + random.nextInt(4));
    }

    // 2. Chassis Bevel / Top Highlight (Light catching top edge)
    g.setColour(juce::Colours::white.withAlpha(0.1f));
    g.drawHorizontalLine(0.0f, 0.0f, (float)getWidth());

    // Layout Rects (Matching resized exactly)
    auto bannerRect = getLocalBounds().removeFromTop(75).reduced(12);
    auto area = getLocalBounds().withTrimmedTop(75);
    auto leftArea = area.removeFromLeft((int)(area.getWidth() * 0.7f));
    auto rightArea = area;

    // Perfect Square Radar
    auto spatialRadarRect = rightArea.removeFromTop(rightArea.getWidth()).reduced(12);

    // 3. Hardware Panel Grooves (Instead of flat lines, we draw a dark line next to a light line)
    auto drawHardwareGroove = [&](int x, int y, int width, int height) {
        g.setColour(juce::Colours::black.withAlpha(0.6f));
        g.fillRect(x, y, width, height);
        g.setColour(juce::Colours::white.withAlpha(0.04f));
        g.fillRect(x + 1, y + 1, width, height); // Offset highlight catching the other edge
    };

    // Vertical groove separating the left and right wings
    drawHardwareGroove(leftArea.getRight() - 1, leftArea.getY(), 2, leftArea.getHeight());
    // Horizontal groove under the banner
    drawHardwareGroove(0, 75, getWidth(), 2);

    // --- NEW: Physical 'Screw' details on the faceplate ---
    auto drawScrew = [&](int x, int y) {
        g.setColour(juce::Colours::black.withAlpha(0.5f));
        g.fillEllipse((float)x, (float)y, 6.0f, 6.0f);
        g.setColour(juce::Colours::white.withAlpha(0.1f));
        g.drawEllipse((float)x+0.5f, (float)y+0.5f, 5.0f, 5.0f, 1.0f);
        g.setColour(juce::Colours::black.withAlpha(0.8f));
        g.drawLine((float)x+1.5f, (float)y+3.0f, (float)x+4.5f, (float)y+3.0f, 1.0f); // Screw slot
    };
    drawScrew(10, 10);
    drawScrew(getWidth()-16, 10);
    drawScrew(10, getHeight()-16);
    drawScrew(getWidth()-16, getHeight()-16);

    // 4. The Recessed OEL Screen Generator
    auto drawRecessedScreen = [&](juce::Rectangle<int> r) {
        // The deep black screen pit
        g.setColour(juce::Colour(0xff020202));
        g.fillRoundedRectangle(r.toFloat(), 5.0f);

        // Inner shadow (sunken depth)
        g.setColour(juce::Colours::black.withAlpha(0.8f));
        g.drawRoundedRectangle(r.toFloat(), 5.0f, 2.0f);

        // Bottom plastic cutout lip catching light
        g.setColour(juce::Colours::white.withAlpha(0.05f));
        g.drawHorizontalLine(r.getBottom() + 1, (float)r.getX() + 2, (float)r.getRight() - 2);

        // OEL Scan-lines
        g.setColour(cyberBlue.withAlpha(0.03f));
        for (int y = r.getY() + 2; y < r.getBottom() - 2; y += 3)
            g.drawHorizontalLine(y, (float)r.getX() + 2, (float)r.getRight() - 2);

        // 3D Glass Glare (curved plastic lens effect)
        juce::ColourGradient glare(juce::Colours::white.withAlpha(0.06f), 0, (float)r.getY(),
                                   juce::Colours::transparentWhite, 0, (float)(r.getY() + r.getHeight() * 0.45f), false);
        g.setGradientFill(glare);
        g.fillRoundedRectangle(r.withHeight((int)(r.getHeight() * 0.5f)).reduced(1).toFloat(), 5.0f);

        // OEL Edge Glow
        g.setColour(cyberBlue.withAlpha(0.2f));
        g.drawRoundedRectangle(r.reduced(1).toFloat(), 4.0f, 1.0f);
    };

    drawRecessedScreen(bannerRect);
    drawRecessedScreen(spatialRadarRect);
}

void OraclePadAudioProcessorEditor::resized()
{
    auto area = getLocalBounds();

    // Top Banner (Chooser)
    auto bannerArea = area.removeFromTop(75);
    bannerLabel.setBounds(bannerArea.reduced(20));

    // Left Wing (Generators) - 70% Width
    auto leftArea = area.removeFromLeft((int)(area.getWidth() * 0.7f));
    auto sectionHeight = leftArea.getHeight() / 3;

    // Align text left for physical hardware look (padding: 20px)
    auto getLabelBounds = [](juce::Rectangle<int> r) { return r.reduced(20, 0).withX(r.getX() + 20); };

    osc1Label.setBounds(getLabelBounds(leftArea.removeFromTop(sectionHeight)));
    osc2Label.setBounds(getLabelBounds(leftArea.removeFromTop(sectionHeight)));
    arpLabel.setBounds(getLabelBounds(leftArea));

    // Right Wing (Radar & Global) - 30% Width
    auto rightArea = area;

    // PERFECT SQUARE Radar area calculation matching paint()
    auto radarArea = rightArea.removeFromTop(rightArea.getWidth());
    radarLabel.setBounds(radarArea.reduced(20));

    // Global Settings Area takes whatever is left at the bottom
    auto globalArea = rightArea;
    globalSettingsLabel.setBounds(globalArea.removeFromTop(30).reduced(10, 0));

    // Position the custom knob in the global area
    gainKnob.setBounds(globalArea.reduced(15).withTrimmedBottom(10));
}