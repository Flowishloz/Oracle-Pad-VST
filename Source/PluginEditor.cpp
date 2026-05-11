#include "PluginProcessor.h"
#include "PluginEditor.h"

OraclePadAudioProcessorEditor::OraclePadAudioProcessorEditor (OraclePadAudioProcessor& p)
    : AudioProcessorEditor (&p),
      audioProcessor (p),
      gainAttachment (p.apvts, "master_gain", gainKnob)  // binds knob to APVTS parameter
{
    auto setupFaceplateLabel = [this](juce::Label& l, juce::String text, float size) {
        l.setText (text, juce::dontSendNotification);
        l.setFont (juce::FontOptions ("Futura", size, juce::Font::bold));
        l.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.6f));
        addAndMakeVisible (l);
    };

    auto setupScreenLabel = [this](juce::Label& l, juce::String text, float size) {
        l.setText (text, juce::dontSendNotification);
        l.setFont (juce::FontOptions ("Futura", size, juce::Font::bold));
        l.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.8f));
        addAndMakeVisible (l);
    };

    setupScreenLabel  (bannerLabel,         "ORACLE DISPLAY // SYSTEM ACTIVE", 18.0f);
    setupScreenLabel  (radarLabel,          "SPATIAL RADAR",                   14.0f);
    setupFaceplateLabel (osc1Label,         "OSC 1: WAVE",                     13.0f);
    setupFaceplateLabel (osc2Label,         "OSC 2: SAMPLER",                  13.0f);
    setupFaceplateLabel (arpLabel,          "ORACLE ARPEGGIATOR",              13.0f);
    setupFaceplateLabel (globalSettingsLabel,"GLOBAL SETTINGS",                11.0f);

    gainKnob.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    gainKnob.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    gainKnob.setLookAndFeel (&customLookAndFeel);
    addAndMakeVisible (gainKnob);

    setSize (900, 500);
    startTimerHz (20); // 20fps — matches retro hardware display refresh rate
}

OraclePadAudioProcessorEditor::~OraclePadAudioProcessorEditor()
{
    stopTimer();
    gainKnob.setLookAndFeel (nullptr);
}

// ---------------------------------------------------------------------------
// Timer — reads audio level and triggers a repaint when it changes
// ---------------------------------------------------------------------------
void OraclePadAudioProcessorEditor::timerCallback()
{
    float level = audioProcessor.outputLevel.load (std::memory_order_relaxed);
    if (std::abs (level - lastOutputLevel) > 0.005f)
    {
        lastOutputLevel = level;
        repaint();
    }
}

// ---------------------------------------------------------------------------
// Paint — Chrome Core / Cassette Futurism chassis
// ---------------------------------------------------------------------------
void OraclePadAudioProcessorEditor::paint (juce::Graphics& g)
{
    auto& random = juce::Random::getSystemRandom();

    // 1. Main hardware chassis — dark cold metal gradient
    juce::ColourGradient chassis (juce::Colour (0xff2A2D34), 0, 0,
                                  juce::Colour (0xff111215), 0, (float) getHeight(), false);
    chassis.addColour (0.5, juce::Colour (0xff1A1C20));
    g.setGradientFill (chassis);
    g.fillAll();

    // 2. Brushed metal grain — horizontal noise strokes
    for (int i = 0; i < 5000; ++i)
    {
        float noiseAlpha = random.nextFloat() * 0.03f;
        g.setColour (juce::Colours::black.withAlpha (noiseAlpha));
        g.drawHorizontalLine (random.nextInt (getHeight()),
                              (float) random.nextInt (getWidth()),
                              (float) random.nextInt (getWidth()) + random.nextInt (4));
    }

    // 3. Top bevel — light catching the top edge
    g.setColour (juce::Colours::white.withAlpha (0.1f));
    g.drawHorizontalLine (0, 0.0f, (float) getWidth());

    // Layout rects — must match resized() exactly
    auto bannerRect = getLocalBounds().removeFromTop (75).reduced (12);
    auto area       = getLocalBounds().withTrimmedTop (75);
    auto leftArea   = area.removeFromLeft ((int) (area.getWidth() * 0.7f));
    auto rightArea  = area;
    auto spatialRadarRect = rightArea.removeFromTop (rightArea.getWidth()).reduced (12);

    // 4. Hardware panel grooves — dark cut + offset light highlight
    auto drawGroove = [&](int x, int y, int w, int h) {
        g.setColour (juce::Colours::black.withAlpha (0.6f));
        g.fillRect (x, y, w, h);
        g.setColour (juce::Colours::white.withAlpha (0.04f));
        g.fillRect (x + 1, y + 1, w, h);
    };
    drawGroove (leftArea.getRight() - 1, leftArea.getY(), 2, leftArea.getHeight());
    drawGroove (0, 75, getWidth(), 2);

    // 5. Corner screws
    auto drawScrew = [&](int x, int y) {
        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.fillEllipse ((float) x, (float) y, 6.0f, 6.0f);
        g.setColour (juce::Colours::white.withAlpha (0.1f));
        g.drawEllipse ((float) x + 0.5f, (float) y + 0.5f, 5.0f, 5.0f, 1.0f);
        g.setColour (juce::Colours::black.withAlpha (0.8f));
        g.drawLine ((float) x + 1.5f, (float) y + 3.0f, (float) x + 4.5f, (float) y + 3.0f, 1.0f);
    };
    drawScrew (10, 10);
    drawScrew (getWidth() - 16, 10);
    drawScrew (10, getHeight() - 16);
    drawScrew (getWidth() - 16, getHeight() - 16);

    // 6. Recessed OEL screen — glowLevel drives radar pulse from audio output
    const float glowLevel = juce::jlimit (0.0f, 1.0f, lastOutputLevel);

    auto drawRecessedScreen = [&](juce::Rectangle<int> r, float glow) {
        // Deep black screen pit
        g.setColour (juce::Colour (0xff020202));
        g.fillRoundedRectangle (r.toFloat(), 5.0f);

        // Sunken inner shadow
        g.setColour (juce::Colours::black.withAlpha (0.8f));
        g.drawRoundedRectangle (r.toFloat(), 5.0f, 2.0f);

        // Plastic lip catching light below screen
        g.setColour (juce::Colours::white.withAlpha (0.05f));
        g.drawHorizontalLine (r.getBottom() + 1, (float) r.getX() + 2, (float) r.getRight() - 2);

        // OEL scan lines — brighten with audio level
        float scanAlpha = 0.03f + glow * 0.09f;
        g.setColour (cyberBlue.withAlpha (scanAlpha));
        for (int y = r.getY() + 2; y < r.getBottom() - 2; y += 3)
            g.drawHorizontalLine (y, (float) r.getX() + 2, (float) r.getRight() - 2);

        // Glass glare — curved plastic lens reflection
        juce::ColourGradient glare (juce::Colours::white.withAlpha (0.06f), 0, (float) r.getY(),
                                    juce::Colours::transparentWhite, 0,
                                    (float) (r.getY() + r.getHeight() * 0.45f), false);
        g.setGradientFill (glare);
        g.fillRoundedRectangle (r.withHeight ((int) (r.getHeight() * 0.5f)).reduced (1).toFloat(), 5.0f);

        // OEL edge glow — pulses with audio level
        float edgeAlpha = 0.2f + glow * 0.55f;
        g.setColour (cyberBlue.withAlpha (edgeAlpha));
        g.drawRoundedRectangle (r.reduced (1).toFloat(), 4.0f, 1.0f);

        // Outer bloom halo when signal is active
        if (glow > 0.04f)
        {
            g.setColour (cyberBlue.withAlpha (glow * 0.18f));
            g.drawRoundedRectangle (r.toFloat().expanded (3.0f), 7.0f, 4.0f);
        }
    };

    drawRecessedScreen (bannerRect,       0.0f);      // banner is static
    drawRecessedScreen (spatialRadarRect, glowLevel); // radar pulses with audio
}

// ---------------------------------------------------------------------------
// Layout
// ---------------------------------------------------------------------------
void OraclePadAudioProcessorEditor::resized()
{
    auto area = getLocalBounds();

    auto bannerArea = area.removeFromTop (75);
    bannerLabel.setBounds (bannerArea.reduced (20));

    auto leftArea     = area.removeFromLeft ((int) (area.getWidth() * 0.7f));
    auto sectionHeight = leftArea.getHeight() / 3;
    auto getLabelBounds = [](juce::Rectangle<int> r) { return r.reduced (20, 0).withX (r.getX() + 20); };

    osc1Label.setBounds (getLabelBounds (leftArea.removeFromTop (sectionHeight)));
    osc2Label.setBounds (getLabelBounds (leftArea.removeFromTop (sectionHeight)));
    arpLabel.setBounds  (getLabelBounds (leftArea));

    auto rightArea = area;
    auto radarArea = rightArea.removeFromTop (rightArea.getWidth());
    radarLabel.setBounds (radarArea.reduced (20));

    auto globalArea = rightArea;
    globalSettingsLabel.setBounds (globalArea.removeFromTop (30).reduced (10, 0));
    gainKnob.setBounds (globalArea.reduced (15).withTrimmedBottom (10));
}
