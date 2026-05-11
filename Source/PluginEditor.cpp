#include "PluginProcessor.h"
#include "PluginEditor.h"

OraclePadAudioProcessorEditor::OraclePadAudioProcessorEditor (OraclePadAudioProcessor& p)
    : AudioProcessorEditor (&p),
      audioProcessor (p),
      gainAttachment  (p.apvts, "master_gain",  gainKnob),
      morphAttach     (p.apvts, "osc1_morph",   morphKnob),
      subAttach       (p.apvts, "osc1_sub",     subKnob),
      timbreAttach    (p.apvts, "osc1_tilt",    timbreKnob),
      spreadAttach    (p.apvts, "osc1_spread",  spreadKnob),
      attackAttach    (p.apvts, "adsr_attack",  attackKnob),
      decayAttach     (p.apvts, "adsr_decay",   decayKnob),
      sustainAttach   (p.apvts, "adsr_sustain", sustainKnob),
      releaseAttach   (p.apvts, "adsr_release", releaseKnob),
      envelopeMonitor (p.apvts)
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

    auto setupKnobLabel = [this](juce::Label& l, juce::String text) {
        l.setText (text, juce::dontSendNotification);
        l.setFont (juce::FontOptions ("Futura", 10.0f, juce::Font::bold));
        l.setColour (juce::Label::textColourId, juce::Colour (0xFF00F0FF).withAlpha (0.7f));
        l.setJustificationType (juce::Justification::centred);
        addAndMakeVisible (l);
    };

    auto setupEnvKnobLabel = [this](juce::Label& l, juce::String text) {
        l.setText (text, juce::dontSendNotification);
        l.setFont (juce::FontOptions ("Futura", 9.0f, juce::Font::bold));
        l.setColour (juce::Label::textColourId, juce::Colour (0xFF00FF66).withAlpha (0.75f));
        l.setJustificationType (juce::Justification::centred);
        addAndMakeVisible (l);
    };

    setupScreenLabel    (bannerLabel,         "", 18.0f);   // text drawn in paint() — keep component for layout compat
    setupScreenLabel    (radarLabel,          "SPATIAL RADAR",                   13.0f);
    setupFaceplateLabel (osc1Label,           "OSC 1: WAVE",                     13.0f);
    setupFaceplateLabel (osc2Label,           "OSC 2: SAMPLER",                  13.0f);
    setupFaceplateLabel (adsrLabel,           "ENVELOPE",                        11.0f);
    setupFaceplateLabel (globalSettingsLabel, "GLOBAL",                          11.0f);
    setupFaceplateLabel (arpLabel,            "",                                  1.0f); // hidden

    setupKnobLabel (morphLabel,  "MORPH");
    setupKnobLabel (subLabel,    "SUB");
    setupKnobLabel (timbreLabel, "TIMBRE");
    setupKnobLabel (spreadLabel, "SPREAD");

    setupEnvKnobLabel (attackLabel,  "ATTACK");
    setupEnvKnobLabel (decayLabel,   "DECAY");
    setupEnvKnobLabel (sustainLabel, "SUSTAIN");
    setupEnvKnobLabel (releaseLabel, "RELEASE");

    auto setupOscKnob = [this](juce::Slider& k) {
        k.setSliderStyle  (juce::Slider::RotaryHorizontalVerticalDrag);
        k.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        k.setLookAndFeel  (&customLookAndFeel);
        addAndMakeVisible (k);
    };
    setupOscKnob (morphKnob);
    setupOscKnob (subKnob);
    setupOscKnob (timbreKnob);
    setupOscKnob (spreadKnob);

    auto setupEnvKnob = [this](juce::Slider& k) {
        k.setSliderStyle  (juce::Slider::RotaryHorizontalVerticalDrag);
        k.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        k.setLookAndFeel  (&customLookAndFeel);
        k.setColour (juce::Slider::rotarySliderFillColourId,    juce::Colour (0xFF00FF66));
        k.setColour (juce::Slider::rotarySliderOutlineColourId, juce::Colours::black.withAlpha (0.5f));
        addAndMakeVisible (k);
    };
    setupEnvKnob (attackKnob);
    setupEnvKnob (decayKnob);
    setupEnvKnob (sustainKnob);
    setupEnvKnob (releaseKnob);

    gainKnob.setSliderStyle  (juce::Slider::RotaryHorizontalVerticalDrag);
    gainKnob.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    gainKnob.setLookAndFeel  (&customLookAndFeel);
    addAndMakeVisible (gainKnob);

    addAndMakeVisible (envelopeMonitor);

    // ── Hardware-style buttons (atmosphere + preset) ─────────────────────
    auto setupHwBtn = [this](juce::TextButton& btn, const juce::String& text) {
        btn.setButtonText (text);
        btn.setLookAndFeel (&customLookAndFeel);
        btn.setColour (juce::TextButton::textColourOffId, cyberBlue);
        btn.setColour (juce::TextButton::textColourOnId,  cyberBlue.brighter (0.4f));
        addAndMakeVisible (btn);
    };

    // Atmosphere cycle — modulo wrapping
    setupHwBtn (prevAtmoButton, "<");
    setupHwBtn (nextAtmoButton, ">");
    prevAtmoButton.onClick = [this]() {
        if (auto* p = audioProcessor.apvts.getParameter ("atmosphere_state")) {
            const int cur = juce::roundToInt (
                audioProcessor.apvts.getRawParameterValue ("atmosphere_state")->load());
            p->setValueNotifyingHost (p->convertTo0to1 ((float) ((cur - 1 + 5) % 5)));
        }
        repaint();
    };
    nextAtmoButton.onClick = [this]() {
        if (auto* p = audioProcessor.apvts.getParameter ("atmosphere_state")) {
            const int cur = juce::roundToInt (
                audioProcessor.apvts.getRawParameterValue ("atmosphere_state")->load());
            p->setValueNotifyingHost (p->convertTo0to1 ((float) ((cur + 1) % 5)));
        }
        repaint();
    };

    // Preset cycle + save
    setupHwBtn (prevPresetButton, "<");
    setupHwBtn (nextPresetButton, ">");
    setupHwBtn (savePresetButton, "SAVE");
    prevPresetButton.onClick = [this]() {
        audioProcessor.presetManager.cyclePreset (-1);
        repaint();
    };
    nextPresetButton.onClick = [this]() {
        audioProcessor.presetManager.cyclePreset (1);
        repaint();
    };
    savePresetButton.onClick = [this]() {
        savePresetChooser = std::make_unique<juce::FileChooser> (
            "Save Oracle Patch",
            audioProcessor.presetManager.getPresetsDirectory(),
            "*.oracle");
        savePresetChooser->launchAsync (
            juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
            [this](const juce::FileChooser& fc) {
                if (fc.getResult() != juce::File{})
                {
                    audioProcessor.presetManager.savePreset (
                        fc.getResult().getFileNameWithoutExtension());
                    repaint();
                }
            });
    };

    setSize (900, 500);
    startTimerHz (20);
}

OraclePadAudioProcessorEditor::~OraclePadAudioProcessorEditor()
{
    stopTimer();
    gainKnob.setLookAndFeel       (nullptr);
    morphKnob.setLookAndFeel      (nullptr);
    subKnob.setLookAndFeel        (nullptr);
    timbreKnob.setLookAndFeel     (nullptr);
    spreadKnob.setLookAndFeel     (nullptr);
    attackKnob.setLookAndFeel     (nullptr);
    decayKnob.setLookAndFeel      (nullptr);
    sustainKnob.setLookAndFeel    (nullptr);
    releaseKnob.setLookAndFeel    (nullptr);
    prevAtmoButton.setLookAndFeel   (nullptr);
    nextAtmoButton.setLookAndFeel   (nullptr);
    prevPresetButton.setLookAndFeel (nullptr);
    nextPresetButton.setLookAndFeel (nullptr);
    savePresetButton.setLookAndFeel (nullptr);
}

void OraclePadAudioProcessorEditor::timerCallback()
{
    envelopeMonitor.repaint();

    float level = audioProcessor.outputLevel.load (std::memory_order_relaxed);
    if (std::abs (level - lastOutputLevel) > 0.005f)
    {
        lastOutputLevel = level;
        repaint();
    }
}

// ---------------------------------------------------------------------------
// Paint — Chrome Core / Cassette Futurism chassis
//
// Grid (% of 900×500 window):
//   Banner           :  full width, y = 0..75
//   Left col (OSC)   :  x = 0..60%=540,  y = 75..500
//     OSC 1          :  y = 75..50%=250
//     OSC 2          :  y = 250..85%=425
//   Right col        :  x = 540..900,    y = 75..500
//     Radar (square) :  y = 75..60%=300  → min(360,225)=225px square, centred
//     Gain           :  y = 300..425
//   ADSR strip       :  full width, y = 85%=425..500
//
// All constants below must match resized() exactly.
// ---------------------------------------------------------------------------
void OraclePadAudioProcessorEditor::paint (juce::Graphics& g)
{
    auto& rng = juce::Random::getSystemRandom();

    // ── Derived grid constants ────────────────────────────────────────────
    const int bannerH   = 75;
    const int stripH    = 75;
    const int stripY    = getHeight() - stripH;                         // 425
    const int leftW     = (int) (getWidth()  * 0.60f);                 // 540
    const int rightX    = leftW;
    const int rightW    = getWidth() - leftW;                           // 360
    const int osc2Y     = getHeight() / 2;                             // 250
    const int radarBotY = (int) (getHeight() * 0.60f);                 // 300
    const int radarH    = radarBotY - bannerH;                         // 225
    const int radarSide = juce::jmin (rightW, radarH);                 // 225
    const int radarX    = rightX + (rightW - radarSide) / 2;           // 607

    // 1. Main chassis gradient
    juce::ColourGradient chassis (juce::Colour (0xff2A2D34), 0, 0,
                                  juce::Colour (0xff111215), 0, (float) getHeight(), false);
    chassis.addColour (0.5, juce::Colour (0xff1A1C20));
    g.setGradientFill (chassis);
    g.fillAll();

    // 2. ADSR strip — deeper chassis tier
    g.setColour (juce::Colour (0xff0b0c0f));
    g.fillRect (0, stripY, getWidth(), stripH);

    // 3. Brushed metal grain
    for (int i = 0; i < 5000; ++i)
    {
        g.setColour (juce::Colours::black.withAlpha (rng.nextFloat() * 0.03f));
        g.drawHorizontalLine (rng.nextInt (getHeight()),
                              (float) rng.nextInt (getWidth()),
                              (float) rng.nextInt (getWidth()) + rng.nextInt (4));
    }

    // 4. Top bevel
    g.setColour (juce::Colours::white.withAlpha (0.1f));
    g.drawHorizontalLine (0, 0.0f, (float) getWidth());

    // 5. Hardware grooves — 1px dark cut + 1px inner-shadow highlight offset
    auto drawGroove = [&](int x, int y, int w, int h) {
        g.setColour (juce::Colour (0xff040507));
        g.fillRect (x, y, w, h);
        // Offset the highlight inward (below for horizontal, right for vertical)
        g.setColour (juce::Colours::white.withAlpha (0.045f));
        g.fillRect (x + (w == 1 ? 1 : 0),
                    y + (h == 1 ? 1 : 0), w, h);
    };
    drawGroove (0,      bannerH,  getWidth(), 1);          // banner / main
    drawGroove (0,      stripY,   getWidth(), 1);          // main / ADSR strip
    drawGroove (leftW,  bannerH,  1, stripY - bannerH);    // OSC col | right col
    drawGroove (0,      osc2Y,    leftW, 1);               // OSC 1 | OSC 2
    drawGroove (rightX, radarBotY, rightW, 1);             // radar | gain

    // 6. OSC 2 recessed placeholder — cold, powered-off
    {
        const int panelTop = osc2Y + 24 + kPad;
        auto panel = juce::Rectangle<int> (kPad, panelTop,
                                           leftW - kPad * 2,
                                           stripY - panelTop - kPad);
        g.setColour (juce::Colour (0xff060709));
        g.fillRoundedRectangle (panel.toFloat(), 5.0f);
        g.setColour (juce::Colours::white.withAlpha (0.025f));
        g.drawRoundedRectangle (panel.toFloat(), 5.0f, 1.0f);
        g.setColour (juce::Colours::white.withAlpha (0.008f));
        for (int y = panel.getY() + 2; y < panel.getBottom(); y += 3)
            g.drawHorizontalLine (y, (float) panel.getX() + 2, (float) panel.getRight() - 2);
    }

    // 7. Corner screws
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

    // 8. OEL recessed screens
    const float glowLevel = juce::jlimit (0.0f, 1.0f, lastOutputLevel);

    auto drawRecessedScreen = [&](juce::Rectangle<int> r, float glow, juce::Colour tint) {
        g.setColour (juce::Colour (0xff020202));
        g.fillRoundedRectangle (r.toFloat(), 5.0f);
        g.setColour (juce::Colours::black.withAlpha (0.8f));
        g.drawRoundedRectangle (r.toFloat(), 5.0f, 2.0f);
        g.setColour (juce::Colours::white.withAlpha (0.05f));
        g.drawHorizontalLine (r.getBottom() + 1, (float) r.getX() + 2, (float) r.getRight() - 2);

        g.setColour (tint.withAlpha (0.03f + glow * 0.09f));
        for (int y = r.getY() + 2; y < r.getBottom() - 2; y += 3)
            g.drawHorizontalLine (y, (float) r.getX() + 2, (float) r.getRight() - 2);

        juce::ColourGradient glare (juce::Colours::white.withAlpha (0.06f), 0, (float) r.getY(),
                                    juce::Colours::transparentWhite, 0,
                                    (float) (r.getY() + r.getHeight() * 0.45f), false);
        g.setGradientFill (glare);
        g.fillRoundedRectangle (r.withHeight ((int) (r.getHeight() * 0.5f)).reduced (1).toFloat(), 5.0f);

        g.setColour (tint.withAlpha (0.2f + glow * 0.55f));
        g.drawRoundedRectangle (r.reduced (1).toFloat(), 4.0f, 1.0f);

        if (glow > 0.04f)
        {
            g.setColour (tint.withAlpha (glow * 0.18f));
            g.drawRoundedRectangle (r.toFloat().expanded (3.0f), 7.0f, 4.0f);
        }
    };

    // ── Y2K Two-Panel Banner ──────────────────────────────────────────────────
    auto bannerScreen = juce::Rectangle<int> (0, 0, getWidth(), bannerH).reduced (kPad);
    drawRecessedScreen (bannerScreen, 0.0f, cyberBlue);  // outer chassis

    const int atmoState = juce::jlimit (0, 4,
        (int) audioProcessor.apvts.getRawParameterValue ("atmosphere_state")->load());
    static const char* const kAtmoNames[] =
        { "RAINFOREST", "TEMPLE", "VALLEY", "CITY", "THE SILO" };

    auto drawGlassPanel = [&](juce::Rectangle<int> area) {
        g.setColour (juce::Colour (0xff001a18));
        g.fillRoundedRectangle (area.toFloat(), 3.0f);
        juce::ColourGradient sheen (juce::Colours::white.withAlpha (0.06f), 0.0f, (float) area.getY(),
                                    juce::Colours::transparentWhite, 0.0f,
                                    (float) (area.getY() + area.getHeight() * 0.4f), false);
        g.setGradientFill (sheen);
        g.fillRoundedRectangle (area.toFloat(), 3.0f);
        g.setColour (cyberBlue.withAlpha (0.40f));
        g.drawRoundedRectangle (area.reduced (1).toFloat(), 2.0f, 1.0f);
        g.setColour (cyberBlue.withAlpha (0.04f));
        for (int sy = area.getY() + 2; sy < area.getBottom() - 2; sy += 3)
            g.drawHorizontalLine (sy, (float) area.getX() + 2.0f, (float) area.getRight() - 2.0f);
    };

    // ── Left panel: Memory Core ───────────────────────────────────────────
    auto memPanel = juce::Rectangle<int> (bannerScreen.getX() + 4, bannerScreen.getY() + 4,
                                          442, bannerScreen.getHeight() - 8);
    drawGlassPanel (memPanel);
    // Brand
    g.setColour (cyberBlue.withAlpha (0.42f));
    g.setFont (juce::FontOptions ("Futura", 8.5f, juce::Font::bold));
    g.drawText ("ORACLE-PAD", juce::Rectangle<int> (memPanel.getX() + 6, memPanel.getY() + 5,
                108, 17), juce::Justification::centredLeft, false);
    g.setColour (juce::Colour (0xff005544));
    g.setFont (juce::FontOptions ("Futura", 7.0f, juce::Font::plain));
    g.drawText ("MEM CORE", juce::Rectangle<int> (memPanel.getX() + 6, memPanel.getCentreY(),
                108, 13), juce::Justification::centredLeft, false);
    // Divider
    g.setColour (cyberBlue.withAlpha (0.16f));
    g.drawVerticalLine (memPanel.getX() + 122,
                        (float) (memPanel.getY() + 5), (float) (memPanel.getBottom() - 5));
    // Preset name readout — dot-matrix style
    // (Buttons prevPreset, nextPreset, savePreset are positioned over this area in resized())
    {
        const juce::String name = audioProcessor.presetManager.currentPresetName;
        const juce::String txt  = juce::String (">  ") + name.toUpperCase();
        auto readArea = juce::Rectangle<int> (memPanel.getX() + 163, memPanel.getY(),
                                              memPanel.getRight() - (memPanel.getX() + 163) - 58,
                                              memPanel.getHeight());
        g.setColour (cyberBlue.withAlpha (0.17f));
        g.setFont (juce::FontOptions ("Futura", 16.0f, juce::Font::bold));
        g.drawText (txt, readArea.reduced (2, 0), juce::Justification::centredLeft, false);
        g.setColour (cyberBlue);
        g.setFont (juce::FontOptions ("Futura", 14.0f, juce::Font::bold));
        g.drawText (txt, readArea.reduced (2, 0), juce::Justification::centredLeft, false);
    }

    // ── Right panel: Atmosphere readout ──────────────────────────────────
    auto atmoPanel = juce::Rectangle<int> (bannerScreen.getX() + 452, bannerScreen.getY() + 4,
                                           bannerScreen.getRight() - (bannerScreen.getX() + 452) - 4,
                                           bannerScreen.getHeight() - 8);
    drawGlassPanel (atmoPanel);
    // Brand
    g.setColour (cyberBlue.withAlpha (0.38f));
    g.setFont (juce::FontOptions ("Futura", 8.0f, juce::Font::bold));
    g.drawText ("ATMO ENG", juce::Rectangle<int> (atmoPanel.getX() + 6, atmoPanel.getY() + 5,
                90, 17), juce::Justification::centredLeft, false);
    g.setColour (juce::Colour (0xff004433));
    g.setFont (juce::FontOptions ("Futura", 7.0f, juce::Font::plain));
    g.drawText ("ENCLOSURE", juce::Rectangle<int> (atmoPanel.getX() + 6, atmoPanel.getCentreY(),
                90, 13), juce::Justification::centredLeft, false);
    // Divider
    g.setColour (cyberBlue.withAlpha (0.15f));
    g.drawVerticalLine (atmoPanel.getX() + 100,
                        (float) (atmoPanel.getY() + 5), (float) (atmoPanel.getBottom() - 5));
    // Atmosphere preset name
    {
        const juce::String txt = juce::String (">  ") + kAtmoNames[atmoState];
        auto readArea = juce::Rectangle<int> (atmoPanel.getX() + 106, atmoPanel.getY(),
                                              atmoPanel.getWidth() - 110, atmoPanel.getHeight());
        g.setColour (cyberBlue.withAlpha (0.17f));
        g.setFont (juce::FontOptions ("Futura", 16.0f, juce::Font::bold));
        g.drawText (txt, readArea.reduced (2, 0), juce::Justification::centredLeft, false);
        g.setColour (cyberBlue);
        g.setFont (juce::FontOptions ("Futura", 14.0f, juce::Font::bold));
        g.drawText (txt, readArea.reduced (2, 0), juce::Justification::centredLeft, false);
        // 5 indicator dots
        const float dotY  = (float) (atmoPanel.getBottom() - 8);
        const float dotX0 = (float) (atmoPanel.getX() + 108);
        for (int d = 0; d < 5; ++d)
        {
            const float dotX = dotX0 + (float) d * 9.0f;
            const float r    = (d == atmoState) ? 2.5f : 1.8f;
            g.setColour (d == atmoState ? cyberBlue : cyberBlue.withAlpha (0.22f));
            g.fillEllipse (dotX, dotY - r, r * 2.0f, r * 2.0f);
        }
    }

    // 3-D chrome bezel highlights on banner frame
    g.setColour (juce::Colours::white.withAlpha (0.09f));
    g.drawLine ((float) bannerScreen.getX(), (float) bannerScreen.getY(),
                (float) bannerScreen.getRight(), (float) bannerScreen.getY(), 1.5f);
    g.setColour (juce::Colours::black.withAlpha (0.32f));
    g.drawLine ((float) bannerScreen.getX(), (float) bannerScreen.getBottom(),
                (float) bannerScreen.getRight(), (float) bannerScreen.getBottom(), 1.5f);

    // Radar screen — perfect square, centred in right col / radar alloc area.
    // Tint shifts with atmosphere preset to reinforce the environmental vibe.
    auto radarScreen = juce::Rectangle<int> (radarX, bannerH, radarSide, radarSide).reduced (kPad);
    drawRecessedScreen (radarScreen, glowLevel, getAtmosphereTint());

    // 9. Radar content — listener head wireframe + source orb
    {
        const float cx = (float) radarScreen.getCentreX();
        const float cy = (float) radarScreen.getCentreY();
        const float hw = radarScreen.getWidth()  * 0.5f;
        const float hh = radarScreen.getHeight() * 0.5f;
        const juce::Colour headGreen (0xFF00AA55);

        // Range rings
        for (float rf : {0.33f, 0.66f, 1.0f})
        {
            g.setColour (headGreen.withAlpha (0.06f));
            g.drawEllipse (cx - hw * rf, cy - hh * rf, hw * rf * 2.0f, hh * rf * 2.0f, 0.5f);
        }

        // Crosshair
        g.setColour (headGreen.withAlpha (0.07f));
        g.drawLine (cx - hw, cy, cx + hw, cy, 0.5f);
        g.drawLine (cx, cy - hh, cx, cy + hh, 0.5f);

        // Listener head wireframe (top-down)
        const float hr = hw * 0.14f;
        g.setColour (headGreen.withAlpha (0.28f));
        g.drawEllipse (cx - hr, cy - hr, hr * 2.0f, hr * 2.0f, 1.0f);
        g.drawLine (cx, cy - hr, cx, cy - hr * 1.8f, 1.0f);              // nose
        g.drawLine (cx - hr, cy - hr * 0.15f, cx - hr * 1.5f, cy - hr * 0.15f, 1.0f); // left ear
        g.drawLine (cx + hr, cy - hr * 0.15f, cx + hr * 1.5f, cy - hr * 0.15f, 1.0f); // right ear

        // Source orb — position driven by spatial_x / spatial_y parameters
        const float spatX = audioProcessor.apvts.getRawParameterValue ("spatial_x")->load();
        const float spatY = audioProcessor.apvts.getRawParameterValue ("spatial_y")->load();
        const float orbX  = cx + spatX * hw;
        const float orbY  = cy - spatY * hh;   // Y=0 → centre, Y=1 → top of screen

        // Bloom halo
        g.setColour (cyberBlue.withAlpha (0.28f));
        g.fillEllipse (orbX - 7.0f, orbY - 7.0f, 14.0f, 14.0f);

        // 5×5 solid orb
        g.setColour (cyberBlue);
        g.fillRect (juce::Rectangle<float> (orbX - 2.5f, orbY - 2.5f, 5.0f, 5.0f));

        // Coordinate readout beneath the orb
        auto coordText = juce::String (spatX, 2) + " / " + juce::String (spatY, 2);
        g.setColour (cyberBlue.withAlpha (0.6f));
        g.setFont (juce::FontOptions (8.0f));
        g.drawText (coordText, (int) (orbX - 28.0f), (int) (orbY + 7.0f),
                    56, 10, juce::Justification::centred, false);
    }
}

// ---------------------------------------------------------------------------
// Layout — strict percentage grid, all spacing via kPad
// ---------------------------------------------------------------------------
void OraclePadAudioProcessorEditor::resized()
{
    // ── Grid constants (must mirror paint() exactly) ──────────────────────
    const int bannerH   = 75;
    const int stripH    = 75;
    const int stripY    = getHeight() - stripH;                         // 425
    const int leftW     = (int) (getWidth()  * 0.60f);                 // 540
    const int rightX    = leftW;
    const int rightW    = getWidth() - leftW;                           // 360
    const int osc2Y     = getHeight() / 2;                             // 250
    const int radarBotY = (int) (getHeight() * 0.60f);                 // 300

    // ── Banner ────────────────────────────────────────────────────────────
    bannerLabel.setBounds ({});   // drawn in paint()

    // Preset memory buttons — inside Memory Core panel (x≈15..455, y=0..75)
    const int btnY  = (bannerH - 26) / 2;   // vertically centred: 24
    prevPresetButton.setBounds (135, btnY, 28, 26);
    nextPresetButton.setBounds (370, btnY, 28, 26);
    savePresetButton.setBounds (403, btnY, 50, 26);

    // Atmosphere < / > buttons — flanking the radar screen in the right column,
    // just below the banner groove (y = bannerH + kPad).
    const int atmoY = bannerH + kPad;
    prevAtmoButton.setBounds (rightX + kPad,              atmoY, 52, 28);
    nextAtmoButton.setBounds (rightX + rightW - kPad - 52, atmoY, 52, 28);

    // ── OSC 1 (left col, top half: y=75..250) ────────────────────────────
    {
        auto osc1 = juce::Rectangle<int> (0, bannerH, leftW, osc2Y - bannerH); // 360×175
        osc1Label.setBounds (osc1.removeFromTop (22).withTrimmedLeft (kPad));

        auto row = osc1.reduced (kPad, kPad / 2);
        const int slotW = row.getWidth() / 4;

        auto placeKnob = [&](juce::Slider& k, juce::Label& lbl) {
            auto slot = row.removeFromLeft (slotW);
            lbl.setBounds (slot.removeFromBottom (14));
            k.setBounds   (slot.reduced (kPad / 2));
        };
        placeKnob (morphKnob,  morphLabel);
        placeKnob (subKnob,    subLabel);
        placeKnob (timbreKnob, timbreLabel);
        placeKnob (spreadKnob, spreadLabel);
    }

    // ── OSC 2 placeholder (left col, bottom half: y=250..425) ─────────────
    {
        auto osc2 = juce::Rectangle<int> (0, osc2Y, leftW, stripY - osc2Y);  // 360×175
        osc2Label.setBounds (osc2.removeFromTop (22).withTrimmedLeft (kPad));
        // Remainder drawn as cold recessed panel in paint()
    }

    // ── Spatial Radar — label hidden; radar is drawn entirely in paint()
    radarLabel.setBounds ({});

    // ── Global gain (compact knob, 50% size) below radar: y=300..425 ──────
    {
        auto gainArea = juce::Rectangle<int> (rightX + kPad, radarBotY + kPad,
                                              rightW - kPad * 2, stripY - radarBotY - kPad * 2);
        globalSettingsLabel.setBounds (gainArea.removeFromTop (20));
        // Centre an 80×80 knob (≈50% of the area's available size)
        gainKnob.setBounds (gainArea.withSizeKeepingCentre (80, 80));
    }

    // ── ADSR strip — centred cluster at y=425..500 ────────────────────────
    {
        const int slotW    = 90;
        const int monitorW = 130;
        const int totalW   = slotW * 4 + monitorW;           // 490 px
        const int xOff     = (getWidth() - totalW) / 2;      // 205 px

        auto strip = juce::Rectangle<int> (xOff, stripY, totalW, stripH);

        auto placeEnv = [&](juce::Slider& k, juce::Label& lbl) {
            auto slot = strip.removeFromLeft (slotW);
            lbl.setBounds (slot.removeFromBottom (14));
            k.setBounds   (slot.reduced (kPad / 2, 2));
        };
        placeEnv (attackKnob,  attackLabel);
        placeEnv (decayKnob,   decayLabel);
        placeEnv (sustainKnob, sustainLabel);
        placeEnv (releaseKnob, releaseLabel);

        envelopeMonitor.setBounds (strip.reduced (kPad / 2, kPad));

        // Not placed in this layout
        adsrLabel.setBounds ({});
        arpLabel.setBounds  ({});
    }
}

// ---------------------------------------------------------------------------
// Radar interaction — mouse methods
// ---------------------------------------------------------------------------
juce::Rectangle<int> OraclePadAudioProcessorEditor::getRadarScreenBounds() const
{
    const int bannerH   = 75;
    const int leftW     = (int) (getWidth()  * 0.60f);
    const int rightW    = getWidth() - leftW;
    const int radarBotY = (int) (getHeight() * 0.60f);
    const int radarH    = radarBotY - bannerH;
    const int radarSide = juce::jmin (rightW, radarH);
    const int radarX    = leftW + (rightW - radarSide) / 2;
    return juce::Rectangle<int> (radarX, bannerH, radarSide, radarSide).reduced (kPad);
}

void OraclePadAudioProcessorEditor::updateSpatialFromMouse (juce::Point<int> pos)
{
    auto screen   = getRadarScreenBounds();
    const float cx = (float) screen.getCentreX();
    const float cy = (float) screen.getCentreY();
    const float hw = screen.getWidth()  * 0.5f;
    const float hh = screen.getHeight() * 0.5f;

    const float x = juce::jlimit (-1.0f, 1.0f, ((float) pos.x - cx) / hw);
    const float y = juce::jlimit (-1.0f, 1.0f,  (cy - (float) pos.y) / hh);

    if (auto* px = audioProcessor.apvts.getParameter ("spatial_x"))
        px->setValueNotifyingHost (px->convertTo0to1 (x));
    if (auto* py = audioProcessor.apvts.getParameter ("spatial_y"))
        py->setValueNotifyingHost (py->convertTo0to1 (y));

    repaint();
}

void OraclePadAudioProcessorEditor::mouseDown (const juce::MouseEvent& e)
{
    if (getRadarScreenBounds().contains (e.getPosition()))
    {
        isDraggingOrb = true;
        updateSpatialFromMouse (e.getPosition());
    }
}

void OraclePadAudioProcessorEditor::mouseDrag (const juce::MouseEvent& e)
{
    if (isDraggingOrb)
        updateSpatialFromMouse (e.getPosition());
}

void OraclePadAudioProcessorEditor::mouseUp (const juce::MouseEvent&)
{
    isDraggingOrb = false;
}

juce::Colour OraclePadAudioProcessorEditor::getAtmosphereTint() const
{
    const int s = juce::jlimit (0, 4,
        (int) audioProcessor.apvts.getRawParameterValue ("atmosphere_state")->load());
    switch (s)
    {
        case 0:  return juce::Colour (0xFF003311); // Rainforest — dark emerald
        case 1:  return juce::Colour (0xFF112233); // Temple     — deep stone blue
        case 2:  return juce::Colour (0xFF332200); // Valley     — dusty amber
        case 3:  return juce::Colour (0xFF220033); // City       — neon purple
        default: return juce::Colour (0xFF002233); // The Silo   — cold cyan
    }
}
