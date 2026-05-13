// PluginEditor.cpp — Phase 7: UI Refinement
// Design Bible v2: Pioneer-Blue / Chrome-Core / High-Tech Skeuomorphism.
// Changes: Pioneer palette (no amber/purple/green), kMicro=36, kLarge=70,
//          kH=680, kHdrH=20 thin banners, 13pt labels, ADSR clipping fixed.

#include "PluginProcessor.h"
#include "PluginEditor.h"

// ============================================================================
//  Constructor
// ============================================================================
OraclePadAudioProcessorEditor::OraclePadAudioProcessorEditor (OraclePadAudioProcessor& p)
    : AudioProcessorEditor (&p),
      audioProcessor (p),
      envelopeMonitor (p.apvts),
      radarComponent  (p.apvts, p.outputLevel),
      // ── OSC 1 attachments ──────────────────────────────────────────────────
      osc1VolAtt    (p.apvts, "osc1_vol",    osc1VolKnob),
      osc1MorphAtt  (p.apvts, "osc1_morph",  osc1MorphKnob),
      osc1MixAtt    (p.apvts, "osc1_mix",    osc1MixKnob),
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
      granJitterAtt (p.apvts, "gran_jitter",  granJitterKnob),
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
    // ── Pioneer-Blue LAF arc colours ──────────────────────────────────────────
    microLAF.setArcColour (juce::Colour (kPioneerBlue));  // OSC 1 — deep electric blue
    subLAF  .setArcColour (juce::Colour (kElecCyan));     // Sub  — warm cyan
    osc2LAF .setArcColour (juce::Colour (kPioneerMid));   // OSC 2 — mid blue
    adsrLAF .setArcColour (juce::Colour (kElecCyan));     // ADSR — warm cyan

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

    // OSC 1 (osc1MixKnob attached but zero-bounded in resized)
    setupMicro (osc1VolKnob,    microLAF);
    setupMicro (osc1MorphKnob,  microLAF);
    setupMicro (osc1MixKnob,    microLAF);
    setupMicro (osc1TiltKnob,   microLAF);
    setupMicro (osc1SpreadKnob, microLAF);
    setupMicro (osc1CutoffKnob, microLAF);
    setupMicro (osc1ResKnob,    microLAF);

    // Sub
    setupMicro (subVolKnob,   subLAF);
    setupMicro (subShapeKnob, subLAF);

    // OSC 2 (granJitterKnob attached but zero-bounded)
    setupMicro (osc2VolKnob,     osc2LAF);
    setupMicro (osc2CutoffKnob,  osc2LAF);
    setupMicro (osc2ResKnob,     osc2LAF);
    setupMicro (granDensityKnob, osc2LAF);
    setupMicro (granSizeKnob,    osc2LAF);
    setupMicro (granJitterKnob,  osc2LAF);
    setupMicro (granSpeedKnob,   osc2LAF);

    // Global dome knobs
    setupLarge (masterGainKnob);
    setupLarge (vintageModeKnob);

    // ADSR
    setupMicro (attackKnob,  adsrLAF);
    setupMicro (decayKnob,   adsrLAF);
    setupMicro (sustainKnob, adsrLAF);
    setupMicro (releaseKnob, adsrLAF);
    addAndMakeVisible (envelopeMonitor);

    // ── Labels — Design Bible v2: 13pt Bold Monospace, high legibility ────────
    // OSC1 index: [0]=VOL [1]=MORPH [2]=MIX(hidden) [3]=TILT [4]=SPRD [5]=CUT [6]=RES
    static const char* const kOsc1Txt[] = { "VOL","MORPH","MIX","TILT","SPRD","CUT","RES" };
    static const char* const kSubTxt[]  = { "LEVEL","SHAPE" };
    // OSC2 index: [0]=VOL [1]=CUT [2]=RES [3]=DENS [4]=SIZE [5]=JITR(hidden) [6]=SPD
    static const char* const kOsc2Txt[] = { "VOL","CUT","RES","DENS","SIZE","JITR","SPD" };
    static const char* const kAdsrTxt[] = { "ATK","DEC","SUS","REL" };

    auto setupLabel = [this](juce::Label& lbl, const char* txt, juce::Colour col) {
        lbl.setText (txt, juce::dontSendNotification);
        lbl.setFont (juce::FontOptions ("Courier New", 13.0f, juce::Font::bold));
        lbl.setColour (juce::Label::textColourId, col);
        lbl.setJustificationType (juce::Justification::centred);
        addAndMakeVisible (lbl);
    };

    // Pioneer-Blue palette — no amber, purple, or green
    const juce::Colour lblOsc1 = juce::Colour (kPioneerBlue).withAlpha (0.90f);
    const juce::Colour lblSub  = juce::Colour (kElecCyan).withAlpha (0.90f);
    const juce::Colour lblOsc2 = juce::Colour (kPioneerMid).withAlpha (0.90f);
    const juce::Colour lblAdsr = juce::Colour (kElecCyan).withAlpha (0.90f);

    for (int i = 0; i < 7; ++i) setupLabel (osc1Labels[i], kOsc1Txt[i], lblOsc1);
    for (int i = 0; i < 2; ++i) setupLabel (subLabels[i],  kSubTxt[i],  lblSub);
    for (int i = 0; i < 7; ++i) setupLabel (osc2Labels[i], kOsc2Txt[i], lblOsc2);
    for (int i = 0; i < 4; ++i) setupLabel (adsrLabels[i], kAdsrTxt[i], lblAdsr);

    // Radar
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

// ============================================================================
//  Destructor
// ============================================================================
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

// ============================================================================
//  Timer
// ============================================================================
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

// ============================================================================
//  Helpers
// ============================================================================
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
    // All tints are cool-blue per Design Bible v2 (no amber, purple, or green)
    switch (s)
    {
        case 0:  return juce::Colour (0xFF001830);  // RAINFOREST — midnight blue
        case 1:  return juce::Colour (0xFF001228);  // TEMPLE — deep navy
        case 2:  return juce::Colour (0xFF00142A);  // VALLEY — dark blue
        case 3:  return juce::Colour (0xFF001530);  // CITY — cool black-blue
        default: return juce::Colour (0xFF000F24);  // THE SILO — deepest blue
    }
}

// ============================================================================
//  drawModuleContainer
//  Design Bible v2: 20px corner radius, ultra-thin 20px metallic gradient
//  header, multi-stage bevel (white 50% / black 40%).
// ============================================================================
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

    // Ultra-thin header strip (kHdrH=20px) with metallic gradient
    if (title != nullptr)
    {
        const auto headerStrip = juce::Rectangle<float> (
            rf.getX(), rf.getY(), rf.getWidth(), (float) kHdrH);

        juce::Path headerPath;
        headerPath.addRoundedRectangle (headerStrip.getX(), headerStrip.getY(),
                                         headerStrip.getWidth(), headerStrip.getHeight(),
                                         rad, rad, true, true, false, false);

        // Metallic gradient: subtle white shimmer over accent tint
        juce::ColourGradient metalGrad (
            juce::Colours::white.withAlpha (0.13f), headerStrip.getX(), headerStrip.getY(),
            juce::Colours::black.withAlpha (0.18f), headerStrip.getX(), headerStrip.getBottom(), false);
        metalGrad.addColour (0.35, accentColour.withAlpha (0.07f));
        g.setGradientFill (metalGrad);
        g.fillPath (headerPath);

        // Title — 10pt bold, highly legible at 20px header height
        g.setColour (accentColour.withAlpha (0.85f));
        g.setFont (juce::FontOptions ("Courier New", 10.0f, juce::Font::bold));
        g.drawText (juce::String (title),
                    bounds.withHeight (kHdrH).withTrimmedLeft (12),
                    juce::Justification::centredLeft, false);

        // Hairline separator below header
        g.setColour (accentColour.withAlpha (0.20f));
        g.drawHorizontalLine (bounds.getY() + kHdrH,
                               rf.getX() + 10.0f, rf.getRight() - 10.0f);
    }

    // Design Bible v2 multi-stage bevel:
    // Layer 1 — Inner Highlight: sharp 1px #FFFFFF at 50% (top-left)
    // Layer 2 — Outer Recess: 2px soft #000000 at 40% (bottom-right)
    juce::ColourGradient bevel (
        juce::Colours::white.withAlpha (0.50f), rf.getX(),    rf.getY(),
        juce::Colours::black.withAlpha (0.40f), rf.getRight(), rf.getBottom(), false);
    g.setGradientFill (bevel);
    g.drawRoundedRectangle (rf.reduced (0.5f), rad - 0.5f, 2.0f);
}

// ============================================================================
//  paint — Phase 7 satin-chrome chassis, Pioneer-Blue module containers.
// ============================================================================
void OraclePadAudioProcessorEditor::paint (juce::Graphics& g)
{
    const juce::Colour pionCyan  = juce::Colour (kElecCyan);
    const juce::Colour pionBlue  = juce::Colour (kPioneerBlue);
    const juce::Colour pionMid   = juce::Colour (kPioneerMid);

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

    // ── 2. Module containers — Pioneer-Blue palette ───────────────────────────
    drawModuleContainer (g, { kOsc1X, kOsc1Y, kOsc1W, kOsc1H }, pionBlue,  "OSC 1");
    drawModuleContainer (g, { kSubX,  kSubY,  kSubW,  kSubH  }, pionCyan,  "SUB");
    drawModuleContainer (g, { kOsc2X, kOsc2Y, kOsc2W, kOsc2H }, pionMid,   "OSC 2");
    drawModuleContainer (g, { kRadarX, kRadarY, kRadarSz, kRadarSz }, pionCyan, "SPATIAL");
    drawModuleContainer (g, { kOutX,  kOutY,  kOutW,  kOutH  }, pionCyan,  "OUTPUT");
    drawModuleContainer (g, { kAdsrX, kAdsrY, kAdsrW, kAdsrH }, pionCyan,  "ADSR");

    // ── 3. Output knob labels (VINTAGE / MASTER) ─────────────────────────────
    {
        constexpr int cx       = kOutX + kOutW / 2;
        constexpr int contentH = kLarge + 3 + 14;
        constexpr int knoY     = kOutY + kHdrH + (kOutH - kHdrH - contentH) / 2;
        constexpr int lblY     = knoY + kLarge + 3;
        constexpr int vX       = cx - kLarge - 12;
        constexpr int mX       = cx + 12;

        g.setFont (juce::FontOptions ("Courier New", 10.0f, juce::Font::bold));
        g.setColour (pionCyan.withAlpha (0.75f));
        g.drawText ("VINTAGE", juce::Rectangle<int> (vX - 2, lblY, kLarge + 4, 14),
                    juce::Justification::centred, false);
        g.drawText ("MASTER",  juce::Rectangle<int> (mX - 2, lblY, kLarge + 4, 14),
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

    // ── 5. Top banner — OEL-90 display ───────────────────────────────────────
    const int atmoState = juce::jlimit (0, 4,
        (int) audioProcessor.apvts.getRawParameterValue ("atmosphere_state")->load());
    static const char* const kAtmoNames[] =
        { "RAINFOREST", "TEMPLE", "VALLEY", "CITY", "THE SILO" };

    const auto bannerBounds = juce::Rectangle<int> (kPad, kPad, kW - kPad * 2, kBannerH - kPad * 2);
    drawModuleContainer (g, bannerBounds, pionCyan, nullptr);

    // Left panel — Preset display
    auto memPanel = juce::Rectangle<int> (
        bannerBounds.getX() + 4, bannerBounds.getY() + 4,
        bannerBounds.getWidth() / 2 - 8,
        bannerBounds.getHeight() - 8);

    g.setColour (juce::Colour (0xFF000F1E));
    g.fillRoundedRectangle (memPanel.toFloat(), 4.0f);
    g.setColour (pionCyan.withAlpha (0.30f));
    g.drawRoundedRectangle (memPanel.reduced (1).toFloat(), 4.0f, 1.0f);

    g.setColour (pionCyan.withAlpha (0.50f));
    g.setFont (juce::FontOptions ("Courier New", 9.0f, juce::Font::bold));
    g.drawText ("ORACLE-PAD  OEL-90",
                juce::Rectangle<int> (memPanel.getX() + 8, memPanel.getY() + 5, 170, 15),
                juce::Justification::centredLeft, false);
    g.setColour (pionCyan.withAlpha (0.22f));
    g.setFont (juce::FontOptions ("Courier New", 7.0f, juce::Font::plain));
    g.drawText ("PHASE 7  CHROME-CORE",
                juce::Rectangle<int> (memPanel.getX() + 8, memPanel.getY() + 22, 170, 12),
                juce::Justification::centredLeft, false);
    {
        const juce::String txt = audioProcessor.presetManager.currentPresetName.toUpperCase();
        auto readArea = juce::Rectangle<int> (
            memPanel.getX() + 178, memPanel.getY(),
            memPanel.getRight() - (memPanel.getX() + 182), memPanel.getHeight());
        g.setColour (pionCyan.withAlpha (0.18f));
        g.setFont (juce::FontOptions ("Courier New", 18.0f, juce::Font::bold));
        g.drawText (txt, readArea.reduced (2, 0), juce::Justification::centredLeft, false);
        g.setColour (pionCyan);
        g.setFont (juce::FontOptions ("Courier New", 16.0f, juce::Font::bold));
        g.drawText (txt, readArea.reduced (2, 0), juce::Justification::centredLeft, false);
    }

    // Right panel — Atmosphere
    auto atmoPanel = juce::Rectangle<int> (
        bannerBounds.getX() + bannerBounds.getWidth() / 2 + 4,
        bannerBounds.getY() + 4,
        bannerBounds.getWidth() / 2 - 8,
        bannerBounds.getHeight() - 8);

    g.setColour (juce::Colour (0xFF000F1E));
    g.fillRoundedRectangle (atmoPanel.toFloat(), 4.0f);
    g.setColour (pionCyan.withAlpha (0.30f));
    g.drawRoundedRectangle (atmoPanel.reduced (1).toFloat(), 4.0f, 1.0f);

    g.setColour (pionCyan.withAlpha (0.50f));
    g.setFont (juce::FontOptions ("Courier New", 9.0f, juce::Font::bold));
    g.drawText ("ATMO ENGINE",
                juce::Rectangle<int> (atmoPanel.getX() + 8, atmoPanel.getY() + 5, 110, 15),
                juce::Justification::centredLeft, false);
    g.setColour (pionBlue.withAlpha (0.35f));
    g.setFont (juce::FontOptions ("Courier New", 7.0f, juce::Font::plain));
    g.drawText ("ENCLOSURE SIM",
                juce::Rectangle<int> (atmoPanel.getX() + 8, atmoPanel.getY() + 22, 110, 12),
                juce::Justification::centredLeft, false);
    g.setColour (pionCyan.withAlpha (0.15f));
    g.drawVerticalLine (atmoPanel.getX() + 122,
                        (float) (atmoPanel.getY() + 5), (float) (atmoPanel.getBottom() - 5));
    {
        const juce::String txt = kAtmoNames[atmoState];
        auto readArea = juce::Rectangle<int> (
            atmoPanel.getX() + 128, atmoPanel.getY(),
            atmoPanel.getWidth() - 132, atmoPanel.getHeight());
        g.setColour (pionCyan.withAlpha (0.18f));
        g.setFont (juce::FontOptions ("Courier New", 18.0f, juce::Font::bold));
        g.drawText (txt, readArea.reduced (2, 0), juce::Justification::centred, false);
        g.setColour (pionCyan);
        g.setFont (juce::FontOptions ("Courier New", 16.0f, juce::Font::bold));
        g.drawText (txt, readArea.reduced (2, 0), juce::Justification::centred, false);
        // Atmosphere indicator dots
        const float dotY  = (float) (atmoPanel.getBottom() - 9);
        const float dotX0 = (float) (atmoPanel.getCentreX() - 20);
        for (int d = 0; d < 5; ++d)
        {
            const float dotX = dotX0 + (float) d * 10.0f;
            const float dr   = (d == atmoState) ? 2.5f : 1.8f;
            g.setColour (d == atmoState ? pionCyan : pionCyan.withAlpha (0.22f));
            g.fillEllipse (dotX, dotY - dr, dr * 2.0f, dr * 2.0f);
        }
    }
}

// ============================================================================
//  resized — Phase 7 layout.
//  kMicro=36, kLarge=70, kHdrH=20, kOutH=116, kAdsrY=577, kAdsrH=95.
//  All inner Y offsets calculated from kHdrH to match the thin banner height.
// ============================================================================
void OraclePadAudioProcessorEditor::resized()
{
    // ── Banner buttons (y 0–65) ───────────────────────────────────────────────
    {
        const int btnY   = (kBannerH - 26) / 2;
        const int arrowY = (kBannerH - 22) / 2;
        prevPresetBtn.setBounds (22,           btnY, 28, 26);
        nextPresetBtn.setBounds (kW / 2 - 88,  btnY, 28, 26);
        savePresetBtn.setBounds (kW / 2 - 56,  btnY, 50, 26);
        prevAtmoBtn.setBounds (kW / 2 + 12,  arrowY, 22, 22);
        nextAtmoBtn.setBounds (kW - 30,       arrowY, 22, 22);
    }

    // ── OSC 1 — 3×2 grid (x 8, y 73, w 330, h 165) ──────────────────────────
    // Visible: VOL MORPH TILT / SPRD CUT RES   (MIX hidden)
    {
        constexpr int innerX = kOsc1X + 12;
        constexpr int innerY = kOsc1Y + kHdrH;            // 93
        constexpr int gW     = kOsc1W - 24;               // 306
        constexpr int gH     = kOsc1H - kHdrH - 12;       // 133
        constexpr int slotW  = gW / 3;                    // 102
        constexpr int slotH  = gH / 2;                    // 66

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
        osc1MixKnob.setBounds (0, 0, 0, 0);  // attached for preset compat, hidden
    }

    // ── SUB module (x 346, y 73, w 186, h 165) ───────────────────────────────
    {
        constexpr int innerX = kSubX + 12;
        constexpr int innerY = kSubY + kHdrH;              // 93
        constexpr int gW     = kSubW - 24;                 // 162
        constexpr int gH     = kSubH - kHdrH - 12;         // 133
        constexpr int slotW  = gW / 2;                     // 81

        const int ky = innerY + (gH - kMicro - kLabelH) / 2;

        subVolKnob.setBounds   (innerX + (slotW - kMicro) / 2,        ky, kMicro, kMicro);
        subLabels[0].setBounds (innerX + (slotW - kMicro) / 2 - 4,    ky + kMicro + 2, kMicro + 8, kLabelH);
        subShapeKnob.setBounds (innerX + slotW + (slotW - kMicro) / 2, ky, kMicro, kMicro);
        subLabels[1].setBounds (innerX + slotW + (slotW - kMicro) / 2 - 4, ky + kMicro + 2, kMicro + 8, kLabelH);
    }

    // ── OSC 2 — 3×2 grid (x 8, y 246, w 524, h 165) ─────────────────────────
    // Visible: VOL CUT RES / DENS SIZE SPD   (JITR hidden)
    {
        constexpr int innerX = kOsc2X + 12;
        constexpr int innerY = kOsc2Y + kHdrH;             // 266
        constexpr int gW     = kOsc2W - 24;                // 500
        constexpr int gH     = kOsc2H - kHdrH - 12;        // 133
        constexpr int slotW  = gW / 3;                     // 166
        constexpr int slotH  = gH / 2;                     // 66

        juce::Slider* grid[6] = {
            &osc2VolKnob, &osc2CutoffKnob, &osc2ResKnob,
            &granDensityKnob, &granSizeKnob, &granSpeedKnob
        };
        juce::Label* lbls[6] = {
            &osc2Labels[0], &osc2Labels[1], &osc2Labels[2],
            &osc2Labels[3], &osc2Labels[4], &osc2Labels[6]
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
        granJitterKnob.setBounds (0, 0, 0, 0);  // attached for preset compat, hidden
    }

    // ── RADAR (x 540, y 73, sz 372) ──────────────────────────────────────────
    // Inner field: 12px side inset, kHdrH top inset, 12px bottom inset.
    radarComponent.setBounds (kRadarX + 12, kRadarY + kHdrH,
                               kRadarSz - 24, kRadarSz - kHdrH - 12);

    // ── OUTPUT (x 540, y 453, w 372, h 116) ──────────────────────────────────
    // kLarge=70 dome knobs centred in space below the 20px header.
    {
        constexpr int cx       = kOutX + kOutW / 2;
        constexpr int contentH = kLarge + 3 + 14;    // knob + gap + label row
        constexpr int knoY     = kOutY + kHdrH + (kOutH - kHdrH - contentH) / 2;
        vintageModeKnob.setBounds (cx - kLarge - 12, knoY, kLarge, kLarge);
        masterGainKnob .setBounds (cx + 12,          knoY, kLarge, kLarge);
    }

    // ── ADSR (x 8, y 577, w 904, h 95) ──────────────────────────────────────
    // Phase 7 ADSR clipping fix: kAdsrY=577, kAdsrH=95.
    // Knobs + labels comfortably fit inside the module with clear bottom margin.
    {
        constexpr int innerX = kAdsrX + 12;
        constexpr int innerY = kAdsrY + kHdrH;            // 597
        constexpr int innerH = kAdsrH - kHdrH - 8;        // 67 — plenty for kMicro=36 + kLabelH=16
        constexpr int slotW  = 52;
        const int     knoY   = innerY + (innerH - kMicro - kLabelH) / 2;

        juce::Slider* row[] = { &attackKnob, &decayKnob, &sustainKnob, &releaseKnob };
        for (int i = 0; i < 4; ++i)
        {
            const int kx = innerX + i * slotW + (slotW - kMicro) / 2;
            row[i]->setBounds (kx, knoY, kMicro, kMicro);
            adsrLabels[i].setBounds (kx - 4, knoY + kMicro + 2, kMicro + 8, kLabelH);
        }

        // 4:3 envelope monitor — right of the 4 ADSR knobs, fills remainder
        constexpr int monitorX = innerX + 4 * slotW + 8;
        const int     monitorH = kAdsrH - 12;
        const int     monitorW = monitorH * 4 / 3;
        envelopeMonitor.setBounds (monitorX, kAdsrY + 6, monitorW, monitorH);
    }
}
