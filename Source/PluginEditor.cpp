// PluginEditor.cpp — Stage 1: Ergonomic Refinement
// Design Bible: Satin Silver chassis, 20px module containers, 2px inner bevel,
// 3×2 knob grids (OSC1/OSC2), Electric Cyan (#00F0FF) pointer, 13pt bold labels.

#include "PluginProcessor.h"
#include "PluginEditor.h"

// ============================================================================
//  OraclePadAudioProcessorEditor
// ============================================================================

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------
OraclePadAudioProcessorEditor::OraclePadAudioProcessorEditor (OraclePadAudioProcessor& p)
    : AudioProcessorEditor (&p),
      audioProcessor (p),
      envelopeMonitor (p.apvts),
      radarComponent  (p.apvts, p.outputLevel),
      // ── OSC 1 attachments ──────────────────────────────────────────────────
      osc1VolAtt    (p.apvts, "osc1_vol",    osc1VolKnob),
      osc1MorphAtt  (p.apvts, "osc1_morph",  osc1MorphKnob),
      osc1MixAtt    (p.apvts, "osc1_mix",    osc1MixKnob),     // attached, hidden in UI
      osc1TiltAtt   (p.apvts, "osc1_tilt",   osc1TiltKnob),
      osc1SpreadAtt (p.apvts, "osc1_spread", osc1SpreadKnob),
      osc1CutoffAtt (p.apvts, "osc1_cutoff", osc1CutoffKnob),
      osc1ResAtt    (p.apvts, "osc1_res",    osc1ResKnob),
      // ── Sub attachments ────────────────────────────────────────────────────
      subVolAtt   (p.apvts, "sub_vol",   subVolKnob),
      subShapeAtt (p.apvts, "sub_shape", subShapeKnob),
      // ── OSC 2 attachments ──────────────────────────────────────────────────
      osc2VolAtt    (p.apvts, "osc2_vol",     osc2VolKnob),
      osc2CutoffAtt (p.apvts, "osc2_cutoff",  osc2CutoffKnob),
      osc2ResAtt    (p.apvts, "osc2_res",     osc2ResKnob),
      granDensityAtt(p.apvts, "gran_density", granDensityKnob),
      granSizeAtt   (p.apvts, "gran_size",    granSizeKnob),
      granJitterAtt (p.apvts, "gran_jitter",  granJitterKnob),  // attached, hidden
      granSpeedAtt  (p.apvts, "gran_speed",   granSpeedKnob),
      // ── Global attachments ─────────────────────────────────────────────────
      masterGainAtt  (p.apvts, "master_gain",  masterGainKnob),
      vintageModeAtt (p.apvts, "vintage_mode", vintageModeKnob),
      // ── ADSR attachments ───────────────────────────────────────────────────
      attackAtt  (p.apvts, "adsr_attack",  attackKnob),
      decayAtt   (p.apvts, "adsr_decay",   decayKnob),
      sustainAtt (p.apvts, "adsr_sustain", sustainKnob),
      releaseAtt (p.apvts, "adsr_release", releaseKnob)
{
    // ── LookAndFeel colours ───────────────────────────────────────────────────
    microLAF.setArcColour (juce::Colour (kAmberOsc1));
    subLAF  .setArcColour (juce::Colour (kGreenSub));
    osc2LAF .setArcColour (juce::Colour (kPurpleOsc2));
    adsrLAF .setArcColour (juce::Colour (kElecCyan));

    // ── Slider setup lambdas ─────────────────────────────────────────────────
    auto setupMicro = [this](juce::Slider& k, MicroKnobLAF& laf) {
        k.setSliderStyle  (juce::Slider::RotaryVerticalDrag);
        k.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        k.setLookAndFeel  (&laf);
        addAndMakeVisible (k);
    };
    auto setupLarge = [this](juce::Slider& k) {
        k.setSliderStyle  (juce::Slider::RotaryVerticalDrag);
        k.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        k.setLookAndFeel  (&largeLAF);
        addAndMakeVisible (k);
    };

    // OSC 1 knobs (osc1MixKnob attached but will be zero-bounded in resized)
    setupMicro (osc1VolKnob,    microLAF);
    setupMicro (osc1MorphKnob,  microLAF);
    setupMicro (osc1MixKnob,    microLAF);   // hidden
    setupMicro (osc1TiltKnob,   microLAF);
    setupMicro (osc1SpreadKnob, microLAF);
    setupMicro (osc1CutoffKnob, microLAF);
    setupMicro (osc1ResKnob,    microLAF);

    // Sub knobs
    setupMicro (subVolKnob,   subLAF);
    setupMicro (subShapeKnob, subLAF);

    // OSC 2 knobs (granJitterKnob hidden)
    setupMicro (osc2VolKnob,     osc2LAF);
    setupMicro (osc2CutoffKnob,  osc2LAF);
    setupMicro (osc2ResKnob,     osc2LAF);
    setupMicro (granDensityKnob, osc2LAF);
    setupMicro (granSizeKnob,    osc2LAF);
    setupMicro (granJitterKnob,  osc2LAF);   // hidden
    setupMicro (granSpeedKnob,   osc2LAF);

    // Global dome knobs
    setupLarge (masterGainKnob);
    setupLarge (vintageModeKnob);

    // ADSR knobs
    setupMicro (attackKnob,  adsrLAF);
    setupMicro (decayKnob,   adsrLAF);
    setupMicro (sustainKnob, adsrLAF);
    setupMicro (releaseKnob, adsrLAF);
    addAndMakeVisible (envelopeMonitor);

    // ── Knob labels — Design Bible: 13pt Bold Monospace, #E0E0E0 ────────────
    // OSC1 order after removing Mix: VOL MORPH TILT SPREAD CUTOFF RES
    // Index map: [0]=VOL [1]=MORPH [2]=MIX(hidden) [3]=TILT [4]=SPREAD [5]=CUT [6]=RES
    static const char* const kOsc1Txt[] = { "VOL","MORPH","MIX","TILT","SPRD","CUT","RES" };
    static const char* const kSubTxt[]  = { "LEVEL","SHAPE" };
    // OSC2: [0]=VOL [1]=CUT [2]=RES [3]=DENS [4]=SIZE [5]=JITR(hidden) [6]=SPD
    static const char* const kOsc2Txt[] = { "VOL","CUT","RES","DENS","SIZE","JITR","SPD" };
    static const char* const kAdsrTxt[] = { "ATK","DEC","SUS","REL" };

    auto setupLabel = [this](juce::Label& lbl, const char* txt, juce::Colour col) {
        lbl.setText (txt, juce::dontSendNotification);
        lbl.setFont (juce::FontOptions ("Courier New", 11.0f, juce::Font::bold));
        lbl.setColour (juce::Label::textColourId, col);
        lbl.setJustificationType (juce::Justification::centred);
        addAndMakeVisible (lbl);
    };

    const juce::Colour lblAmber  = juce::Colour (kAmberOsc1);
    const juce::Colour lblPurple = juce::Colour (kPurpleOsc2);
    const juce::Colour lblBlue   = juce::Colour (kElecCyan);
    const juce::Colour lblGreen  = juce::Colour (kGreenSub);
    const juce::Colour lblText   = juce::Colour (kTextColor);

    for (int i = 0; i < 7; ++i) setupLabel (osc1Labels[i], kOsc1Txt[i], lblAmber.withAlpha (0.85f));
    for (int i = 0; i < 2; ++i) setupLabel (subLabels[i],  kSubTxt[i],  lblGreen.withAlpha (0.85f));
    for (int i = 0; i < 7; ++i) setupLabel (osc2Labels[i], kOsc2Txt[i], lblPurple.withAlpha (0.85f));
    for (int i = 0; i < 4; ++i) setupLabel (adsrLabels[i], kAdsrTxt[i], lblBlue.withAlpha (0.85f));

    // Radar component
    addAndMakeVisible (radarComponent);

    // ── Hardware buttons ──────────────────────────────────────────────────────
    const juce::Colour btnBlue = juce::Colour (kElecCyan);
    auto setupBtn = [&](juce::TextButton& btn, const juce::String& text) {
        btn.setButtonText (text);
        btn.setLookAndFeel (&largeLAF);
        btn.setColour (juce::TextButton::textColourOffId, btnBlue);
        btn.setColour (juce::TextButton::textColourOnId,  btnBlue.brighter (0.4f));
        addAndMakeVisible (btn);
    };
    setupBtn (prevAtmoBtn,   "<");
    setupBtn (nextAtmoBtn,   ">");
    setupBtn (prevPresetBtn, "<");
    setupBtn (nextPresetBtn, ">");
    setupBtn (savePresetBtn, "SAVE");

    prevAtmoBtn.onClick = [this]() {
        if (auto* p2 = audioProcessor.apvts.getParameter ("atmosphere_state")) {
            const int cur = juce::roundToInt (
                audioProcessor.apvts.getRawParameterValue ("atmosphere_state")->load());
            p2->setValueNotifyingHost (p2->convertTo0to1 ((float) ((cur - 1 + 5) % 5)));
        }
        repaint();
    };
    nextAtmoBtn.onClick = [this]() {
        if (auto* p2 = audioProcessor.apvts.getParameter ("atmosphere_state")) {
            const int cur = juce::roundToInt (
                audioProcessor.apvts.getRawParameterValue ("atmosphere_state")->load());
            p2->setValueNotifyingHost (p2->convertTo0to1 ((float) ((cur + 1) % 5)));
        }
        repaint();
    };
    prevPresetBtn.onClick = [this]() { audioProcessor.presetManager.cyclePreset (-1); repaint(); };
    nextPresetBtn.onClick = [this]() { audioProcessor.presetManager.cyclePreset  (1); repaint(); };
    savePresetBtn.onClick = [this]() {
        savePresetChooser = std::make_unique<juce::FileChooser> (
            "Save Oracle Patch",
            audioProcessor.presetManager.getPresetsDirectory(),
            "*.oracle");
        savePresetChooser->launchAsync (
            juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
            [this](const juce::FileChooser& fc) {
                if (fc.getResult() != juce::File{})
                    audioProcessor.presetManager.savePreset (
                        fc.getResult().getFileNameWithoutExtension());
            });
    };

    setSize (kW, kH);
    startTimerHz (30);
}

// ---------------------------------------------------------------------------
// Destructor
// ---------------------------------------------------------------------------
OraclePadAudioProcessorEditor::~OraclePadAudioProcessorEditor()
{
    stopTimer();
    for (auto* k : { &osc1VolKnob, &osc1MorphKnob, &osc1MixKnob, &osc1TiltKnob,
                     &osc1SpreadKnob, &osc1CutoffKnob, &osc1ResKnob })
        k->setLookAndFeel (nullptr);
    subVolKnob  .setLookAndFeel (nullptr);
    subShapeKnob.setLookAndFeel (nullptr);
    for (auto* k : { &osc2VolKnob, &osc2CutoffKnob, &osc2ResKnob,
                     &granDensityKnob, &granSizeKnob, &granJitterKnob, &granSpeedKnob })
        k->setLookAndFeel (nullptr);
    masterGainKnob .setLookAndFeel (nullptr);
    vintageModeKnob.setLookAndFeel (nullptr);
    for (auto* k : { &attackKnob, &decayKnob, &sustainKnob, &releaseKnob })
        k->setLookAndFeel (nullptr);
    for (auto* b : { &prevAtmoBtn, &nextAtmoBtn, &prevPresetBtn, &nextPresetBtn, &savePresetBtn })
        b->setLookAndFeel (nullptr);
}

// ---------------------------------------------------------------------------
// Timer
// ---------------------------------------------------------------------------
void OraclePadAudioProcessorEditor::timerCallback()
{
    const float level = audioProcessor.outputLevel.load (std::memory_order_relaxed);
    if (std::abs (level - lastOutputLevel) > 0.004f)
    {
        lastOutputLevel = level;
        repaint();
    }
    envelopeMonitor.repaint();
    radarComponent.repaint();

    if (bannerCountdown > 0)
    {
        --bannerCountdown;
        repaint();
    }
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
void OraclePadAudioProcessorEditor::flashBanner (const juce::String& name,
                                                   float               value,
                                                   const juce::String& unit)
{
    bannerText      = "> " + name.toUpperCase() + ": " + juce::String (value, 2) + " " + unit;
    bannerCountdown = 60;
}

juce::Colour OraclePadAudioProcessorEditor::getAtmosphereTint() const
{
    const int s = juce::jlimit (0, 4,
        (int) audioProcessor.apvts.getRawParameterValue ("atmosphere_state")->load());
    switch (s)
    {
        case 0:  return juce::Colour (0xFF003311);
        case 1:  return juce::Colour (0xFF112233);
        case 2:  return juce::Colour (0xFF332200);
        case 3:  return juce::Colour (0xFF220033);
        default: return juce::Colour (0xFF002233);
    }
}

// Design Bible module container: 20px radius, 2px inner bevel, dark panel.
void OraclePadAudioProcessorEditor::drawModuleContainer (juce::Graphics& g,
                                                          juce::Rectangle<int> bounds,
                                                          juce::Colour         accentColour,
                                                          const char*          title) const
{
    const auto  rf  = bounds.toFloat();
    const float rad = (float) kModR;

    // Dark module panel body
    g.setColour (juce::Colour (kModuleBg));
    g.fillRoundedRectangle (rf, rad);

    // Header accent strip (only top corners rounded)
    if (title != nullptr)
    {
        const auto headerStrip = juce::Rectangle<float> (
            rf.getX(), rf.getY(), rf.getWidth(), 24.0f);
        juce::Path headerPath;
        headerPath.addRoundedRectangle (headerStrip.getX(), headerStrip.getY(),
                                         headerStrip.getWidth(), headerStrip.getHeight(),
                                         rad, rad, true, true, false, false);
        g.setColour (accentColour.withAlpha (0.10f));
        g.fillPath (headerPath);

        // Title
        g.setColour (accentColour.withAlpha (0.85f));
        g.setFont (juce::FontOptions ("Courier New", 9.5f, juce::Font::bold));
        g.drawText (juce::String (title),
                    bounds.withHeight (24).withTrimmedLeft (12),
                    juce::Justification::centredLeft, false);

        // Separator
        g.setColour (accentColour.withAlpha (0.18f));
        g.drawHorizontalLine (bounds.getY() + 24,
                               rf.getX() + 10.0f, rf.getRight() - 10.0f);
    }

    // 2-px inner bevel: gradient from white (top-left) to black (bottom-right)
    juce::ColourGradient bevel (
        juce::Colours::white.withAlpha (0.40f), rf.getX(),    rf.getY(),
        juce::Colours::black.withAlpha (0.50f), rf.getRight(), rf.getBottom(), false);
    g.setGradientFill (bevel);
    g.drawRoundedRectangle (rf.reduced (0.5f), rad - 0.5f, 2.0f);
}

// ---------------------------------------------------------------------------
// Paint — Stage 1 satin-chrome chassis with embedded module containers.
// ---------------------------------------------------------------------------
void OraclePadAudioProcessorEditor::paint (juce::Graphics& g)
{
    const juce::Colour cyberBlue = juce::Colour (kElecCyan);
    const juce::Colour amber     = juce::Colour (kAmberOsc1);
    const juce::Colour purple    = juce::Colour (kPurpleOsc2);
    const juce::Colour green     = juce::Colour (kGreenSub);

    // ── 1. Satin Silver chassis background ───────────────────────────────────
    juce::ColourGradient chassis (
        juce::Colour (0xFFCECECE), 0.0f, 0.0f,
        juce::Colour (0xFFAAAAAA), 0.0f, (float) getHeight(), false);
    chassis.addColour (0.5, juce::Colour (0xFFC0C0C0));
    g.setGradientFill (chassis);
    g.fillAll();

    // Brushed metal horizontal grain
    for (int y = 0; y < getHeight(); y += 2)
    {
        g.setColour (juce::Colours::white.withAlpha (0.04f));
        g.drawHorizontalLine (y, 0.0f, (float) getWidth());
    }

    // ── 2. Module containers ─────────────────────────────────────────────────
    drawModuleContainer (g, { kOsc1X, kOsc1Y, kOsc1W, kOsc1H }, amber,   "OSC 1");
    drawModuleContainer (g, { kSubX,  kSubY,  kSubW,  kSubH  }, green,   "SUB");
    drawModuleContainer (g, { kOsc2X, kOsc2Y, kOsc2W, kOsc2H }, purple,  "OSC 2");
    drawModuleContainer (g, { kRadarX, kRadarY, kRadarSz, kRadarSz }, cyberBlue, "SPATIAL");
    drawModuleContainer (g, { kOutX,  kOutY,  kOutW,  kOutH  }, cyberBlue, "OUTPUT");
    drawModuleContainer (g, { kAdsrX, kAdsrY, kAdsrW, kAdsrH }, cyberBlue, "ADSR");

    // ── 3. Output knob labels (VINTAGE / MASTER) ─────────────────────────────
    {
        constexpr int cx       = kOutX + kOutW / 2;
        constexpr int knoY     = kOutY + (kOutH - kLarge) / 2;
        constexpr int lblY     = knoY + kLarge + 3;
        constexpr int vX       = cx - kLarge - 12;
        constexpr int mX       = cx + 12;

        g.setFont (juce::FontOptions ("Courier New", 9.0f, juce::Font::bold));
        g.setColour (cyberBlue.withAlpha (0.70f));
        g.drawText ("VINTAGE", juce::Rectangle<int> (vX - 2, lblY, kLarge + 4, 12),
                    juce::Justification::centred, false);
        g.drawText ("MASTER",  juce::Rectangle<int> (mX - 2, lblY, kLarge + 4, 12),
                    juce::Justification::centred, false);
    }

    // ── 4. Corner screws ──────────────────────────────────────────────────────
    auto drawScrew = [&](int x, int y) {
        g.setColour (juce::Colour (0xFF888888));
        g.fillEllipse ((float) x, (float) y, 8.0f, 8.0f);
        g.setColour (juce::Colours::white.withAlpha (0.35f));
        g.drawEllipse ((float) x + 0.5f, (float) y + 0.5f, 7.0f, 7.0f, 1.0f);
        g.setColour (juce::Colour (0xFF404040));
        g.drawLine ((float) x + 2.0f, (float) y + 4.0f, (float) x + 6.0f, (float) y + 4.0f, 1.2f);
        g.setColour (juce::Colours::white.withAlpha (0.2f));
        g.drawLine ((float) x + 2.0f, (float) y + 3.5f, (float) x + 6.0f, (float) y + 3.5f, 0.5f);
    };
    drawScrew (8,  8);
    drawScrew (getWidth() - 16, 8);
    drawScrew (8,  getHeight() - 16);
    drawScrew (getWidth() - 16, getHeight() - 16);

    // ── 5. Banner ─────────────────────────────────────────────────────────────
    const int atmoState = juce::jlimit (0, 4,
        (int) audioProcessor.apvts.getRawParameterValue ("atmosphere_state")->load());
    static const char* const kAtmoNames[] =
        { "RAINFOREST", "TEMPLE", "VALLEY", "CITY", "THE SILO" };

    // Banner module container
    const auto bannerBounds = juce::Rectangle<int> (kPad, kPad, kW - kPad * 2, kBannerH - kPad * 2);
    drawModuleContainer (g, bannerBounds, cyberBlue, nullptr);

    // Left panel — Preset display
    auto memPanel = juce::Rectangle<int> (
        bannerBounds.getX() + 4, bannerBounds.getY() + 4,
        bannerBounds.getWidth() / 2 - 8,
        bannerBounds.getHeight() - 8);

    g.setColour (juce::Colour (0xFF001A18));
    g.fillRoundedRectangle (memPanel.toFloat(), 4.0f);
    g.setColour (cyberBlue.withAlpha (0.30f));
    g.drawRoundedRectangle (memPanel.reduced (1).toFloat(), 4.0f, 1.0f);

    g.setColour (cyberBlue.withAlpha (0.45f));
    g.setFont (juce::FontOptions ("Courier New", 9.0f, juce::Font::bold));
    g.drawText ("ORACLE-PAD  OEL-90",
                juce::Rectangle<int> (memPanel.getX() + 8, memPanel.getY() + 5, 170, 15),
                juce::Justification::centredLeft, false);
    g.setColour (cyberBlue.withAlpha (0.22f));
    g.setFont (juce::FontOptions ("Courier New", 7.0f, juce::Font::plain));
    g.drawText ("STAGE 1  ERGONOMIC",
                juce::Rectangle<int> (memPanel.getX() + 8, memPanel.getY() + 22, 170, 12),
                juce::Justification::centredLeft, false);
    {
        const juce::String txt = audioProcessor.presetManager.currentPresetName.toUpperCase();
        auto readArea = juce::Rectangle<int> (
            memPanel.getX() + 178, memPanel.getY(),
            memPanel.getRight() - (memPanel.getX() + 182), memPanel.getHeight());
        g.setColour (cyberBlue.withAlpha (0.18f));
        g.setFont (juce::FontOptions ("Courier New", 18.0f, juce::Font::bold));
        g.drawText (txt, readArea.reduced (2, 0), juce::Justification::centredLeft, false);
        g.setColour (cyberBlue);
        g.setFont (juce::FontOptions ("Courier New", 16.0f, juce::Font::bold));
        g.drawText (txt, readArea.reduced (2, 0), juce::Justification::centredLeft, false);
    }

    // Right panel — Atmosphere
    auto atmoPanel = juce::Rectangle<int> (
        bannerBounds.getX() + bannerBounds.getWidth() / 2 + 4,
        bannerBounds.getY() + 4,
        bannerBounds.getWidth() / 2 - 8,
        bannerBounds.getHeight() - 8);

    g.setColour (juce::Colour (0xFF001A18));
    g.fillRoundedRectangle (atmoPanel.toFloat(), 4.0f);
    g.setColour (cyberBlue.withAlpha (0.30f));
    g.drawRoundedRectangle (atmoPanel.reduced (1).toFloat(), 4.0f, 1.0f);

    g.setColour (cyberBlue.withAlpha (0.45f));
    g.setFont (juce::FontOptions ("Courier New", 9.0f, juce::Font::bold));
    g.drawText ("ATMO ENGINE",
                juce::Rectangle<int> (atmoPanel.getX() + 8, atmoPanel.getY() + 5, 110, 15),
                juce::Justification::centredLeft, false);
    g.setColour (juce::Colour (0xff004433));
    g.setFont (juce::FontOptions ("Courier New", 7.0f, juce::Font::plain));
    g.drawText ("ENCLOSURE SIM",
                juce::Rectangle<int> (atmoPanel.getX() + 8, atmoPanel.getY() + 22, 110, 12),
                juce::Justification::centredLeft, false);
    g.setColour (cyberBlue.withAlpha (0.15f));
    g.drawVerticalLine (atmoPanel.getX() + 122,
                        (float) (atmoPanel.getY() + 5), (float) (atmoPanel.getBottom() - 5));
    {
        const juce::String txt = kAtmoNames[atmoState];
        auto readArea = juce::Rectangle<int> (
            atmoPanel.getX() + 128, atmoPanel.getY(),
            atmoPanel.getWidth() - 132, atmoPanel.getHeight());
        g.setColour (cyberBlue.withAlpha (0.18f));
        g.setFont (juce::FontOptions ("Courier New", 18.0f, juce::Font::bold));
        g.drawText (txt, readArea.reduced (2, 0), juce::Justification::centred, false);
        g.setColour (cyberBlue);
        g.setFont (juce::FontOptions ("Courier New", 16.0f, juce::Font::bold));
        g.drawText (txt, readArea.reduced (2, 0), juce::Justification::centred, false);
        // Atmosphere dots
        const float dotY  = (float) (atmoPanel.getBottom() - 9);
        const float dotX0 = (float) (atmoPanel.getCentreX() - 20);
        for (int d = 0; d < 5; ++d)
        {
            const float dotX = dotX0 + (float) d * 10.0f;
            const float dr   = (d == atmoState) ? 2.5f : 1.8f;
            g.setColour (d == atmoState ? cyberBlue : cyberBlue.withAlpha (0.22f));
            g.fillEllipse (dotX, dotY - dr, dr * 2.0f, dr * 2.0f);
        }
    }
}

// ---------------------------------------------------------------------------
// Layout — Stage 1 full grid placement.
// ---------------------------------------------------------------------------
void OraclePadAudioProcessorEditor::resized()
{
    // ── Banner buttons (y 0-65) ───────────────────────────────────────────────
    {
        const int btnY   = (kBannerH - 26) / 2;
        const int arrowY = (kBannerH - 22) / 2;
        prevPresetBtn.setBounds (22,           btnY, 28, 26);
        nextPresetBtn.setBounds (kW / 2 - 88,  btnY, 28, 26);
        savePresetBtn.setBounds (kW / 2 - 56,  btnY, 50, 26);
        prevAtmoBtn.setBounds (kW / 2 + 12,  arrowY, 22, 22);
        nextAtmoBtn.setBounds (kW - 30,       arrowY, 22, 22);
    }

    // ── OSC 1 — 3×2 grid (x 8, y 73, w 330, h 165) ─────────────────────────
    // Visible: Vol Morph Tilt / Spread Cutoff Res   (Mix hidden)
    {
        constexpr int innerX = kOsc1X + 12;          // 20
        constexpr int innerY = kOsc1Y + 26;           // 99
        constexpr int gW     = kOsc1W - 24;           // 306
        constexpr int gH     = kOsc1H - 38;           // 127
        constexpr int slotW  = gW / 3;                // 102
        constexpr int slotH  = gH / 2;                // 63

        juce::Slider* grid[6] = {
            &osc1VolKnob, &osc1MorphKnob, &osc1TiltKnob,
            &osc1SpreadKnob, &osc1CutoffKnob, &osc1ResKnob
        };
        juce::Label* lbls[6] = {
            &osc1Labels[0], &osc1Labels[1], &osc1Labels[3],
            &osc1Labels[4], &osc1Labels[5], &osc1Labels[6]
        };

        for (int i = 0; i < 6; ++i)
        {
            const int col = i % 3;
            const int row = i / 3;
            const int kx  = innerX + col * slotW + (slotW - kMicro) / 2;
            const int ky  = innerY + row * slotH + (slotH - kMicro - kLabelH) / 2;
            grid[i]->setBounds (kx, ky, kMicro, kMicro);
            lbls[i]->setBounds (kx - 4, ky + kMicro + 2, kMicro + 8, kLabelH);
        }
        osc1MixKnob.setBounds (0, 0, 0, 0);  // hidden — attached for preset compat
    }

    // ── SUB module (x 346, y 73, w 186, h 165) ──────────────────────────────
    {
        constexpr int innerX = kSubX + 12;
        constexpr int innerY = kSubY + 26;
        constexpr int gW     = kSubW - 24;    // 162
        constexpr int gH     = kSubH - 38;    // 127
        constexpr int slotW  = gW / 2;        // 81

        const int ky = innerY + (gH - kMicro - kLabelH) / 2;

        subVolKnob.setBounds   (innerX + (slotW - kMicro) / 2,        ky, kMicro, kMicro);
        subLabels[0].setBounds (innerX + (slotW - kMicro) / 2 - 4,    ky + kMicro + 2, kMicro + 8, kLabelH);
        subShapeKnob.setBounds (innerX + slotW + (slotW - kMicro) / 2, ky, kMicro, kMicro);
        subLabels[1].setBounds (innerX + slotW + (slotW - kMicro) / 2 - 4, ky + kMicro + 2, kMicro + 8, kLabelH);
    }

    // ── OSC 2 — 3×2 grid (x 8, y 246, w 524, h 165) ────────────────────────
    // Visible: Vol Cutoff Res / Density Size Speed   (Jitter hidden)
    {
        constexpr int innerX = kOsc2X + 12;
        constexpr int innerY = kOsc2Y + 26;
        constexpr int gW     = kOsc2W - 24;   // 500
        constexpr int gH     = kOsc2H - 38;   // 127
        constexpr int slotW  = gW / 3;         // 166
        constexpr int slotH  = gH / 2;         // 63

        juce::Slider* grid[6] = {
            &osc2VolKnob, &osc2CutoffKnob, &osc2ResKnob,
            &granDensityKnob, &granSizeKnob, &granSpeedKnob
        };
        juce::Label* lbls[6] = {
            &osc2Labels[0], &osc2Labels[1], &osc2Labels[2],
            &osc2Labels[3], &osc2Labels[4], &osc2Labels[6]  // skip [5]=JITR
        };

        for (int i = 0; i < 6; ++i)
        {
            const int col = i % 3;
            const int row = i / 3;
            const int kx  = innerX + col * slotW + (slotW - kMicro) / 2;
            const int ky  = innerY + row * slotH + (slotH - kMicro - kLabelH) / 2;
            grid[i]->setBounds (kx, ky, kMicro, kMicro);
            lbls[i]->setBounds (kx - 4, ky + kMicro + 2, kMicro + 8, kLabelH);
        }
        granJitterKnob.setBounds (0, 0, 0, 0);  // hidden
    }

    // ── RADAR (x 540, y 73, sz 372) ──────────────────────────────────────────
    // Inner field with 12px inset from module edge
    radarComponent.setBounds (kRadarX + 12, kRadarY + 26, kRadarSz - 24, kRadarSz - 38);

    // ── OUTPUT (x 540, y 453, w 372, h 88) ──────────────────────────────────
    {
        constexpr int cx   = kOutX + kOutW / 2;
        constexpr int knoY = kOutY + (kOutH - kLarge) / 2;
        vintageModeKnob.setBounds (cx - kLarge - 12, knoY, kLarge, kLarge);
        masterGainKnob .setBounds (cx + 12,          knoY, kLarge, kLarge);
    }

    // ── ADSR (x 8, y 549, w 904, h 63) ──────────────────────────────────────
    // 4 knobs tight-left, 4:3 envelope monitor immediately to their right.
    {
        constexpr int innerX  = kAdsrX + 12;
        constexpr int innerY  = kAdsrY + 26;
        constexpr int innerH  = kAdsrH - 34;   // available height for knobs
        constexpr int slotW   = 52;
        const int     knoY    = innerY + (innerH - kMicro - kLabelH) / 2;

        juce::Slider* row[] = { &attackKnob, &decayKnob, &sustainKnob, &releaseKnob };
        for (int i = 0; i < 4; ++i)
        {
            const int kx = innerX + i * slotW + (slotW - kMicro) / 2;
            row[i]->setBounds (kx, knoY, kMicro, kMicro);
            adsrLabels[i].setBounds (kx - 4, knoY + kMicro + 2, kMicro + 8, kLabelH);
        }

        // 4:3 envelope monitor — starts right of 4 knobs, fills remainder
        constexpr int monitorX = innerX + 4 * slotW + 8;
        const int monitorH = kAdsrH - 12;
        const int monitorW = monitorH * 4 / 3;
        envelopeMonitor.setBounds (monitorX, kAdsrY + 6, monitorW, monitorH);
    }
}
