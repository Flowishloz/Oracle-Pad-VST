#include "PluginProcessor.h"
#include "PluginEditor.h"

OraclePadAudioProcessorEditor::OraclePadAudioProcessorEditor (OraclePadAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    // Setup Labels with modern FontOptions
    auto setupLabel = [this](juce::Label& l, juce::String text, float size) {
        l.setText(text, juce::dontSendNotification);
        l.setFont (juce::FontOptions (size).withStyle ("Bold"));
        l.setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.7f));
        addAndMakeVisible(l);
    };

    setupLabel(bannerLabel, "ORACLE DISPLAY // SYSTEM ACTIVE", 18.0f);
    setupLabel(osc1Label, "OSC 1: WAVE", 13.0f);
    setupLabel(osc2Label, "OSC 2: SAMPLER", 13.0f);
    setupLabel(arpLabel, "ORACLE ARPEGGIATOR", 13.0f);
    setupLabel(radarLabel, "SPATIAL RADAR", 13.0f);
    setupLabel(globalSettingsLabel, "GLOBAL", 13.0f);

    // Setup Slider - Correct way to hide the text box
    gainSlider.setSliderStyle(juce::Slider::LinearVertical);
    gainSlider.setRange(0.0, 1.0, 0.01);
    gainSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    addAndMakeVisible(gainSlider);

    setSize (900, 500);
}

OraclePadAudioProcessorEditor::~OraclePadAudioProcessorEditor() {}

void OraclePadAudioProcessorEditor::paint (juce::Graphics& g)
{
    // 1. Chrome Core Background Gradient
    // We define the start (top) and end (bottom) colors
    juce::ColourGradient gradient(juce::Colour(0xff222222), 0, 0,
                                  juce::Colour(0xff1A1A1A), 0, (float)getHeight(), false);
    
    // CORRECTED: addColour is the proper JUCE method for adding a stop in the middle (0.5)
    gradient.addColour(0.5, juce::Colour(0xff121212));
    
    g.setGradientFill(gradient);
    g.fillAll();

    // 2. Define Rects for drawing (Matches the layout in resized)
    auto bannerRect = getLocalBounds().removeFromTop(75).reduced(10);
    auto area = getLocalBounds().withTrimmedTop(75);
    auto rightArea = area.withTrimmedLeft(250);
    auto spatialRadarRect = rightArea.removeFromTop((int)(rightArea.getHeight() * 0.8f)).reduced(10);

    // 3. Cyber Blue Glow Borders
    auto drawGlow = [&](juce::Rectangle<int> r) {
        g.setColour(cyberBlue.withAlpha(0.1f));
        g.drawRoundedRectangle(r.toFloat().expanded(2.0f), 4.0f, 3.0f);
        g.setColour(cyberBlue.withAlpha(0.4f));
        g.drawRoundedRectangle(r.toFloat(), 4.0f, 1.5f);
        g.setColour(cyberBlue);
        g.drawRoundedRectangle(r.reduced(1).toFloat(), 4.0f, 0.5f);
    };

    drawGlow(bannerRect);
    drawGlow(spatialRadarRect);
    
    // Subtle section divider to separate the Left Wing
    g.setColour(juce::Colours::white.withAlpha(0.05f));
    g.drawRect(250, 75, 1, getHeight() - 75);
}

void OraclePadAudioProcessorEditor::resized()
{
    auto area = getLocalBounds();
    
    // Top Banner
    auto bannerArea = area.removeFromTop(75);
    bannerLabel.setBounds(bannerArea.reduced(20));

    // Left Wing
    auto leftArea = area.removeFromLeft(250);
    auto sectionHeight = leftArea.getHeight() / 3;
    
    osc1Label.setBounds(leftArea.removeFromTop(sectionHeight).reduced(10));
    osc2Label.setBounds(leftArea.removeFromTop(sectionHeight).reduced(10));
    arpLabel.setBounds(leftArea.reduced(10));

    // Right Wing / Radar
    auto rightArea = area;
    auto radarArea = rightArea.removeFromTop((int)(rightArea.getHeight() * 0.8f));
    radarLabel.setBounds(radarArea.reduced(20));
    
    // Global Settings Area
    auto globalArea = rightArea;
    globalSettingsLabel.setBounds(globalArea.removeFromLeft(100).reduced(10));
    gainSlider.setBounds(globalArea.reduced(10));
}