#include "PluginProcessor.h"
#include "PluginEditor.h"

// ============================================================================
// Layout constants — Phase 1 module grid (920×680)
// ============================================================================
static constexpr int kW       = 920;
static constexpr int kH       = 680;
static constexpr int kBannerH = 65;

// Left column modules
static constexpr int kOsc1X = 8,   kOsc1Y = 73,  kOsc1W = 330, kOsc1H = 165;
static constexpr int kSubX  = 346, kSubY  = 73,  kSubW  = 186, kSubH  = 165;
static constexpr int kOsc2X = 8,   kOsc2Y = 246, kOsc2W = 524, kOsc2H = 165;
static constexpr int kAtmoX = 8,   kAtmoY = 419, kAtmoW = 524, kAtmoH = 150;

// Master block — dedicated bottom-right zone, below the radar
static constexpr int kMstrX = 540, kMstrY = 452, kMstrW = 372, kMstrH = 117;

// Full-width bottom ADSR strip
static constexpr int kAdsrX = 8,   kAdsrY = 577, kAdsrW = 904, kAdsrH = 95;

// Right-column radar
static constexpr int kRadarX = 540, kRadarY = 73, kRadarSz = 372;

static constexpr int kMicro = 36;
static constexpr int kLblH  = 12;

// ============================================================================
// Helper: configure a rotary knob
// ============================================================================
void OraclePadAudioProcessorEditor::setupKnob (juce::Slider& s,
                                                const juce::String& tooltip)
{
    s.setSliderStyle (juce::Slider::Rotary);
    s.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    s.setTooltip (tooltip);
    s.setLookAndFeel (&microLAF);
    addAndMakeVisible (s);
}

void OraclePadAudioProcessorEditor::setupLabel (juce::Label& l,
                                                 const juce::String& text)
{
    l.setText (text, juce::dontSendNotification);
    l.setJustificationType (juce::Justification::centred);
    l.setFont (juce::FontOptions (10.0f));
    l.setColour (juce::Label::textColourId, juce::Colour (0xFF00D1FF).withAlpha (0.75f));
    addAndMakeVisible (l);
}

// ============================================================================
// Constructor
// ============================================================================
OraclePadAudioProcessorEditor::OraclePadAudioProcessorEditor (OraclePadAudioProcessor& p)
    : AudioProcessorEditor (&p),
      audioProcessor (p),
      waveformComponent (p.apvts, p.granEngine_),
      radarComponent    (p.apvts, p.currentOutputLevel)
{
    setLookAndFeel (&microLAF);

    // ── Header: plugin title (centre-banner branding) ─────────────────────────
    titleLabel.setText ("OEL-90", juce::dontSendNotification);
    titleLabel.setFont (juce::FontOptions ("Courier New", 14.0f, juce::Font::bold));
    titleLabel.setColour (juce::Label::textColourId, juce::Colour (0xFF00D1FF));
    titleLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (titleLabel);

    // ── Header: main patch preset browser (top-left) ──────────────────────────
    presetLabel.setJustificationType (juce::Justification::centred);
    presetLabel.setFont (juce::FontOptions (11.0f));
    presetLabel.setColour (juce::Label::textColourId, juce::Colour (0xFF00D1FF).withAlpha (0.8f));
    addAndMakeVisible (presetLabel);
    updatePresetLabel();

    prevPresetBtn.setColour (juce::TextButton::buttonColourId,  juce::Colour (0xFF1A1C22));
    prevPresetBtn.setColour (juce::TextButton::textColourOffId, juce::Colour (0xFF00D1FF));
    prevPresetBtn.onClick = [this]
    {
        audioProcessor.presetManager_.cyclePreset (-1);
        updatePresetLabel();
    };
    addAndMakeVisible (prevPresetBtn);

    nextPresetBtn.setColour (juce::TextButton::buttonColourId,  juce::Colour (0xFF1A1C22));
    nextPresetBtn.setColour (juce::TextButton::textColourOffId, juce::Colour (0xFF00D1FF));
    nextPresetBtn.onClick = [this]
    {
        audioProcessor.presetManager_.cyclePreset (+1);
        updatePresetLabel();
    };
    addAndMakeVisible (nextPresetBtn);

    // ── Header: atmosphere preset selector (top-right) ────────────────────────
    atmoPresetLabel.setJustificationType (juce::Justification::centred);
    atmoPresetLabel.setFont (juce::FontOptions (11.0f));
    atmoPresetLabel.setColour (juce::Label::textColourId, juce::Colour (0xFF00D1FF).withAlpha (0.8f));
    addAndMakeVisible (atmoPresetLabel);
    updateAtmoLabel();

    prevAtmoBtn.setColour (juce::TextButton::buttonColourId,  juce::Colour (0xFF1A1C22));
    prevAtmoBtn.setColour (juce::TextButton::textColourOffId, juce::Colour (0xFF00D1FF));
    prevAtmoBtn.onClick = [this]
    {
        if (auto* p = audioProcessor.apvts.getParameter ("atmosphere_state"))
        {
            int i = juce::roundToInt (audioProcessor.apvts.getRawParameterValue ("atmosphere_state")->load());
            i = juce::jmax (0, i - 1);
            p->setValueNotifyingHost ((float)i / 4.0f);
            updateAtmoLabel();
        }
    };
    addAndMakeVisible (prevAtmoBtn);

    nextAtmoBtn.setColour (juce::TextButton::buttonColourId,  juce::Colour (0xFF1A1C22));
    nextAtmoBtn.setColour (juce::TextButton::textColourOffId, juce::Colour (0xFF00D1FF));
    nextAtmoBtn.onClick = [this]
    {
        if (auto* p = audioProcessor.apvts.getParameter ("atmosphere_state"))
        {
            int i = juce::roundToInt (audioProcessor.apvts.getRawParameterValue ("atmosphere_state")->load());
            i = juce::jmin (4, i + 1);
            p->setValueNotifyingHost ((float)i / 4.0f);
            updateAtmoLabel();
        }
    };
    addAndMakeVisible (nextAtmoBtn);

    // ── WaveformComponent (OSC 2 sample display) ──────────────────────────────
    waveformComponent.onFileDropped = [this] (const juce::File& f)
    {
        audioProcessor.loadGranularSample (f);
    };
    addAndMakeVisible (waveformComponent);

    // ── RadarComponent ────────────────────────────────────────────────────────
    addAndMakeVisible (radarComponent);

    // ── OSC 1 knobs — 5 wired: vol/morph/mix/spread/cut ──────────────────────
    setupKnob (osc1VolKnob,    "Osc 1 volume");
    setupKnob (osc1MorphKnob,  "Juno pulse width morph (0=35% pw, 1=65% pw)");
    setupKnob (osc1MixKnob,    "VA blend: sine → saw+pulse with micro-detune");
    setupKnob (osc1SpreadKnob, "Per-voice stereo spread");
    setupKnob (osc1CutKnob,    "Osc 1 low-pass filter cutoff");
    setupLabel (osc1VolLbl,    "VOL");
    setupLabel (osc1MorphLbl,  "MRPH");
    setupLabel (osc1MixLbl,    "MIX");
    setupLabel (osc1SpreadLbl, "SPRD");
    setupLabel (osc1CutLbl,    "CUT");

    aOsc1Vol    = std::make_unique<SliderAttachment> (p.apvts, "osc1_vol",    osc1VolKnob);
    aOsc1Morph  = std::make_unique<SliderAttachment> (p.apvts, "osc1_morph",  osc1MorphKnob);
    aOsc1Mix    = std::make_unique<SliderAttachment> (p.apvts, "osc1_mix",    osc1MixKnob);
    aOsc1Cut    = std::make_unique<SliderAttachment> (p.apvts, "osc1_cut",    osc1CutKnob);
    aOsc1Spread = std::make_unique<SliderAttachment> (p.apvts, "osc1_spread", osc1SpreadKnob);

    // ── Sub oscillator knobs ──────────────────────────────────────────────────
    setupKnob (subLevelKnob,  "Sub oscillator level");
    setupKnob (subShapeKnob,  "Sub shape: sine → square");
    setupKnob (subOctaveKnob, "Sub octave offset (-2 / -1 / 0)");
    setupLabel (subLevelLbl,  "SUB");
    setupLabel (subShapeLbl,  "SHPE");
    setupLabel (subOctaveLbl, "OCT");

    aSubLevel  = std::make_unique<SliderAttachment> (p.apvts, "subVolume", subLevelKnob);
    aSubShape  = std::make_unique<SliderAttachment> (p.apvts, "subShape",  subShapeKnob);
    aSubOctave = std::make_unique<SliderAttachment> (p.apvts, "subOctave", subOctaveKnob);

    // ── OSC 2 granular control panel — 7 knobs below waveform display ─────────
    setupKnob (osc2VolKnob,     "Osc 2 / Granular output volume");
    setupKnob (osc2CutoffKnob,  "Granular filter cutoff");
    setupKnob (osc2ResKnob,     "Granular filter resonance");
    setupKnob (granDensityKnob, "Grain density (grains/sec)");
    setupKnob (granSizeKnob,    "Grain size (ms)");
    setupKnob (granJitterKnob,  "Grain position jitter");
    setupKnob (granSpeedKnob,   "Granular playback speed");
    setupLabel (osc2VolLbl,     "VOL");
    setupLabel (osc2CutoffLbl,  "COFF");
    setupLabel (osc2ResLbl,     "RES");
    setupLabel (granDensityLbl, "DENS");
    setupLabel (granSizeLbl,    "SIZE");
    setupLabel (granJitterLbl,  "JITR");
    setupLabel (granSpeedLbl,   "SPD");

    aGranDensity = std::make_unique<SliderAttachment> (p.apvts, "gran_density", granDensityKnob);
    aGranSize    = std::make_unique<SliderAttachment> (p.apvts, "gran_size",    granSizeKnob);
    // osc2Vol/Cutoff/Res/Jitter/Speed — reserved, APVTS params added in Phase 17+

    // ── ADSR knobs ────────────────────────────────────────────────────────────
    setupKnob (adsrAttackKnob,  "ADSR Attack");
    setupKnob (adsrDecayKnob,   "ADSR Decay");
    setupKnob (adsrSustainKnob, "ADSR Sustain");
    setupKnob (adsrReleaseKnob, "ADSR Release");
    setupLabel (adsrAttackLbl,  "ATK");
    setupLabel (adsrDecayLbl,   "DCY");
    setupLabel (adsrSustainLbl, "SUS");
    setupLabel (adsrReleaseLbl, "REL");

    aAdsrA = std::make_unique<SliderAttachment> (p.apvts, "adsr_attack",  adsrAttackKnob);
    aAdsrD = std::make_unique<SliderAttachment> (p.apvts, "adsr_decay",   adsrDecayKnob);
    aAdsrS = std::make_unique<SliderAttachment> (p.apvts, "adsr_sustain", adsrSustainKnob);
    aAdsrR = std::make_unique<SliderAttachment> (p.apvts, "adsr_release", adsrReleaseKnob);

    // ── ADSR envelope monitor (placeholder — Phase 17 will animate) ───────────
    envelopeMonitor.setText ("ENV", juce::dontSendNotification);
    envelopeMonitor.setJustificationType (juce::Justification::centred);
    envelopeMonitor.setFont (juce::FontOptions (9.0f));
    envelopeMonitor.setColour (juce::Label::textColourId,
                               juce::Colour (0xFF00D1FF).withAlpha (0.5f));
    envelopeMonitor.setColour (juce::Label::backgroundColourId,
                               juce::Colour (0xFF1A1C22));
    envelopeMonitor.setColour (juce::Label::outlineColourId,
                               juce::Colour (0xFF00D1FF).withAlpha (0.3f));
    addAndMakeVisible (envelopeMonitor);

    // ── Atmosphere engine knobs ───────────────────────────────────────────────
    setupKnob (atmosStateKnob, "Atmosphere preset (0=Rainforest … 4=Silo)");
    setupKnob (atmosMixKnob,   "Atmosphere dry/wet mix");
    setupLabel (atmosStateLbl, "PRST");
    setupLabel (atmosMixLbl,   "ATMO");

    aAtmosState = std::make_unique<SliderAttachment> (p.apvts, "atmosphere_state", atmosStateKnob);
    aAtmosMix   = std::make_unique<SliderAttachment> (p.apvts, "atmosphere_mix",   atmosMixKnob);

    // ── Master block knobs (dedicated bottom-right zone) ─────────────────────
    setupKnob (vintageKnob,    "Vintage character");
    setupKnob (masterGainKnob, "Master output gain");
    setupLabel (vintageLbl,    "VNTG");
    setupLabel (masterGainLbl, "MSTR");

    aVintage    = std::make_unique<SliderAttachment> (p.apvts, "vintage",     vintageKnob);
    aMasterGain = std::make_unique<SliderAttachment> (p.apvts, "master_gain", masterGainKnob);

    setSize (kW, kH);
    startTimerHz (30);
}

// ============================================================================
// Destructor — detach LAF before members are destroyed
// ============================================================================
OraclePadAudioProcessorEditor::~OraclePadAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);

    for (auto* s : { &osc1VolKnob,    &osc1MorphKnob,   &osc1MixKnob,
                     &osc1SpreadKnob, &osc1CutKnob,
                     &subLevelKnob,   &subShapeKnob,    &subOctaveKnob,
                     &osc2VolKnob,    &osc2CutoffKnob,  &osc2ResKnob,
                     &granDensityKnob, &granSizeKnob,   &granJitterKnob, &granSpeedKnob,
                     &adsrAttackKnob, &adsrDecayKnob,   &adsrSustainKnob, &adsrReleaseKnob,
                     &atmosStateKnob, &atmosMixKnob,
                     &vintageKnob,    &masterGainKnob })
        s->setLookAndFeel (nullptr);
}

// ============================================================================
// Timer — drives radar glow repaint (WaveformComponent has its own 30fps timer)
// ============================================================================
void OraclePadAudioProcessorEditor::timerCallback()
{
    radarComponent.repaint();
}

// ============================================================================
// Module container — filled rounded rect with a hairline border and title tag.
// ============================================================================
void OraclePadAudioProcessorEditor::drawModuleContainer (juce::Graphics& g,
                                                          int x, int y, int w, int h,
                                                          const juce::String& name)
{
    const juce::Colour fill = juce::Colour (0xFF14161C);
    const juce::Colour cyan = juce::Colour (0xFF00D1FF);

    g.setColour (fill);
    g.fillRoundedRectangle ((float)x, (float)y, (float)w, (float)h, 5.0f);
    g.setColour (cyan.withAlpha (0.18f));
    g.drawRoundedRectangle ((float)x + 0.5f, (float)y + 0.5f,
                             (float)w - 1.0f, (float)h - 1.0f, 5.0f, 0.75f);

    g.setFont (juce::FontOptions ("Courier New", 7.5f, juce::Font::bold));
    g.setColour (cyan.withAlpha (0.40f));
    g.drawText (name, x + 6, y + 5, w - 12, 13, juce::Justification::left);
}

// ============================================================================
// Paint
// ============================================================================
void OraclePadAudioProcessorEditor::paint (juce::Graphics& g)
{
    const juce::Colour bg   = juce::Colour (0xFF0D0F14);
    const juce::Colour cyan = juce::Colour (0xFF00D1FF);

    g.fillAll (bg);

    // Header banner
    g.setColour (juce::Colour (0xFF14161C));
    g.fillRect (0, 0, kW, kBannerH);
    g.setColour (cyan.withAlpha (0.15f));
    g.drawHorizontalLine (kBannerH, 0.0f, (float)kW);

    // Module containers
    drawModuleContainer (g, kOsc1X, kOsc1Y, kOsc1W, kOsc1H, "OSC 1");
    drawModuleContainer (g, kSubX,  kSubY,  kSubW,  kSubH,  "SUB");
    drawModuleContainer (g, kOsc2X, kOsc2Y, kOsc2W, kOsc2H, "OSC 2");
    drawModuleContainer (g, kAtmoX, kAtmoY, kAtmoW, kAtmoH, "ATMOSPHERE");
    drawModuleContainer (g, kMstrX, kMstrY, kMstrW, kMstrH, "MASTER");
    drawModuleContainer (g, kAdsrX, kAdsrY, kAdsrW, kAdsrH, "ADSR");
}

// ============================================================================
// Resized — Phase 1 absolute spatial positioning
// ============================================================================
void OraclePadAudioProcessorEditor::resized()
{
    // ── HEADER: main patch preset browser (top-left) ───────────────────────────
    prevPresetBtn.setBounds (8,   20, 22, 25);
    presetLabel  .setBounds (34,  12, 160, 40);
    nextPresetBtn.setBounds (198, 20, 22,  25);

    // ── HEADER: plugin title (centred branding) ────────────────────────────────
    titleLabel   .setBounds (380, 12, 160, 40);

    // ── HEADER: atmosphere preset selector (top-right) ────────────────────────
    atmoPresetLabel.setBounds (630, 12,  160, 40);
    prevAtmoBtn    .setBounds (794, 20,   22, 25);
    nextAtmoBtn    .setBounds (820, 20,   22, 25);

    // ── RADAR (right column) ───────────────────────────────────────────────────
    radarComponent.setBounds (kRadarX, kRadarY, kRadarSz, kRadarSz);

    // ── OSC 2: waveform display (top 90 px) + 7-knob granular panel (bottom) ──
    waveformComponent.setBounds (kOsc2X, kOsc2Y, kOsc2W, 90);
    {
        const int stripY = kOsc2Y + 90;
        const int stripH = kOsc2H - 90;          // 75 px
        const int ix     = kOsc2X + 8;
        const int iw     = kOsc2W - 16;
        const int colW   = iw / 7;               // ≈72 px per knob cell
        const int ky     = stripY + 4 + (stripH - 4 - kMicro - kLblH - 2) / 2;
        const int ox     = (colW - kMicro) / 2;

        auto osc2Knob = [&](juce::Slider& s, juce::Label& l, int col)
        {
            const int kx = ix + col * colW + ox;
            s.setBounds (kx, ky,              kMicro, kMicro);
            l.setBounds (kx, ky + kMicro + 1, kMicro, kLblH);
        };

        osc2Knob (osc2VolKnob,     osc2VolLbl,     0);
        osc2Knob (osc2CutoffKnob,  osc2CutoffLbl,  1);
        osc2Knob (osc2ResKnob,     osc2ResLbl,      2);
        osc2Knob (granDensityKnob, granDensityLbl,  3);
        osc2Knob (granSizeKnob,    granSizeLbl,     4);
        osc2Knob (granJitterKnob,  granJitterLbl,   5);
        osc2Knob (granSpeedKnob,   granSpeedLbl,    6);
    }

    // ── OSC 1: 3-over-2 staggered grid ───────────────────────────────────────
    // Top row  (3 knobs): VOL / MORPH / MIX  — columns 0,1,2
    // Bottom row (2 knobs): SPREAD / CUT — centered under top 3 at columns 0.5,1.5
    {
        const int ix = kOsc1X + 8;
        const int iy = kOsc1Y + 22;
        const int cw = (kOsc1W - 16) / 3;
        const int ch = (kOsc1H - 30) / 2;
        const int ox = (cw - kMicro) / 2;
        const int oy = (ch - kMicro - kLblH - 2) / 2;

        auto topKnob = [&](juce::Slider& s, juce::Label& l, int col)
        {
            const int kx = ix + col * cw + ox;
            const int ky = iy + oy;
            s.setBounds (kx, ky,              kMicro, kMicro);
            l.setBounds (kx, ky + kMicro + 1, kMicro, kLblH);
        };

        auto botKnob = [&](juce::Slider& s, juce::Label& l, int pos)
        {
            // pos 0 → column 0.5, pos 1 → column 1.5 (staggered midpoints)
            const int kx = ix + cw / 2 + pos * cw + ox;
            const int ky = iy + ch + oy;
            s.setBounds (kx, ky,              kMicro, kMicro);
            l.setBounds (kx, ky + kMicro + 1, kMicro, kLblH);
        };

        topKnob (osc1VolKnob,   osc1VolLbl,   0);
        topKnob (osc1MorphKnob, osc1MorphLbl, 1);
        topKnob (osc1MixKnob,   osc1MixLbl,   2);

        botKnob (osc1SpreadKnob, osc1SpreadLbl, 0);
        botKnob (osc1CutKnob,    osc1CutLbl,    1);
    }

    // ── SUB: 3 knobs in a centred horizontal strip ────────────────────────────
    {
        const int ix = kSubX + 8;
        const int iy = kSubY + 22;
        const int cw = (kSubW - 16) / 3;
        const int ky = iy + ((kSubH - 30) - kMicro - kLblH - 2) / 2;
        const int ox = (cw - kMicro) / 2;

        subLevelKnob .setBounds (ix + 0 * cw + ox, ky, kMicro, kMicro);
        subLevelLbl  .setBounds (ix + 0 * cw + ox, ky + kMicro + 1, kMicro, kLblH);
        subShapeKnob .setBounds (ix + 1 * cw + ox, ky, kMicro, kMicro);
        subShapeLbl  .setBounds (ix + 1 * cw + ox, ky + kMicro + 1, kMicro, kLblH);
        subOctaveKnob.setBounds (ix + 2 * cw + ox, ky, kMicro, kMicro);
        subOctaveLbl .setBounds (ix + 2 * cw + ox, ky + kMicro + 1, kMicro, kLblH);
    }

    // ── ATMOSPHERE: 2 knobs (preset state + dry/wet mix) ─────────────────────
    {
        const int ix = kAtmoX + 8;
        const int iy = kAtmoY + 22;
        const int cw = (kAtmoW - 16) / 2;
        const int ky = iy + ((kAtmoH - 30) - kMicro - kLblH - 2) / 2;
        const int ox = (cw - kMicro) / 2;

        atmosStateKnob.setBounds (ix + 0 * cw + ox, ky, kMicro, kMicro);
        atmosStateLbl .setBounds (ix + 0 * cw + ox, ky + kMicro + 1, kMicro, kLblH);
        atmosMixKnob  .setBounds (ix + 1 * cw + ox, ky, kMicro, kMicro);
        atmosMixLbl   .setBounds (ix + 1 * cw + ox, ky + kMicro + 1, kMicro, kLblH);
    }

    // ── MASTER: vintage + gain in dedicated bottom-right block ────────────────
    {
        const int ix = kMstrX + 8;
        const int iy = kMstrY + 22;
        const int cw = (kMstrW - 16) / 2;
        const int ky = iy + ((kMstrH - 30) - kMicro - kLblH - 2) / 2;
        const int ox = (cw - kMicro) / 2;

        vintageKnob   .setBounds (ix + 0 * cw + ox, ky, kMicro, kMicro);
        vintageLbl    .setBounds (ix + 0 * cw + ox, ky + kMicro + 1, kMicro, kLblH);
        masterGainKnob.setBounds (ix + 1 * cw + ox, ky, kMicro, kMicro);
        masterGainLbl .setBounds (ix + 1 * cw + ox, ky + kMicro + 1, kMicro, kLblH);
    }

    // ── ADSR: envelope monitor centred, A/D flanking left, S/R flanking right ──
    {
        static constexpr int envW = 160;
        const int envX = kAdsrX + (kAdsrW - envW) / 2;
        const int envY = kAdsrY + 18;
        const int envH = kAdsrH - 26;

        envelopeMonitor.setBounds (envX, envY, envW, envH);

        // Left zone: attack + decay
        const int ixL    = kAdsrX + 8;
        const int leftW  = envX - ixL;
        const int colL   = leftW / 2;
        const int oxL    = (colL - kMicro) / 2;
        const int ky     = envY + (envH - kMicro - kLblH - 2) / 2;

        adsrAttackKnob.setBounds (ixL + 0 * colL + oxL, ky, kMicro, kMicro);
        adsrAttackLbl .setBounds (ixL + 0 * colL + oxL, ky + kMicro + 1, kMicro, kLblH);
        adsrDecayKnob .setBounds (ixL + 1 * colL + oxL, ky, kMicro, kMicro);
        adsrDecayLbl  .setBounds (ixL + 1 * colL + oxL, ky + kMicro + 1, kMicro, kLblH);

        // Right zone: sustain + release
        const int ixR    = envX + envW;
        const int rightW = (kAdsrX + kAdsrW - 8) - ixR;
        const int colR   = rightW / 2;
        const int oxR    = (colR - kMicro) / 2;

        adsrSustainKnob.setBounds (ixR + 0 * colR + oxR, ky, kMicro, kMicro);
        adsrSustainLbl .setBounds (ixR + 0 * colR + oxR, ky + kMicro + 1, kMicro, kLblH);
        adsrReleaseKnob.setBounds (ixR + 1 * colR + oxR, ky, kMicro, kMicro);
        adsrReleaseLbl .setBounds (ixR + 1 * colR + oxR, ky + kMicro + 1, kMicro, kLblH);
    }
}

// ============================================================================
// Preset display helpers
// ============================================================================
void OraclePadAudioProcessorEditor::updatePresetLabel()
{
    presetLabel.setText (audioProcessor.presetManager_.currentPresetName,
                         juce::dontSendNotification);
}

void OraclePadAudioProcessorEditor::updateAtmoLabel()
{
    static const char* names[] = { "Rainforest", "Temple", "Valley", "City", "Silo" };
    const int i = juce::roundToInt (
        audioProcessor.apvts.getRawParameterValue ("atmosphere_state")->load());
    atmoPresetLabel.setText (names[juce::jlimit (0, 4, i)],
                             juce::dontSendNotification);
}
