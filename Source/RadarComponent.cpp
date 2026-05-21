// RadarComponent.cpp — Full binaural spatial radar visual.
// Design: square module with 20px corner radius, source orb constrained
// within the rounded boundary, Electric Cyan (#00F0FF) pointer and grid.

#include "PluginProcessor.h"
#include "PluginEditor.h"

RadarComponent::RadarComponent (juce::AudioProcessorValueTreeState& a,
                                 std::atomic<float>&                 o)
    : apvts (a), outputLevel (o)
{
    setInterceptsMouseClicks (true, false);
}

void RadarComponent::paint (juce::Graphics& g)
{
    const float cx = (float) getWidth()  * 0.5f;
    const float cy = (float) getHeight() * 0.5f;

    const float r = std::min (cx, cy) - 4.0f;

    const juce::Colour cyberBlue = juce::Colour (0xFF00F0FF);
    const juce::Colour bgTint    = getAtmosphereTint();
    const float        level     = outputLevel.load (std::memory_order_relaxed);

    g.setColour (juce::Colour (0xFF010305));
    g.fillEllipse (cx - r, cy - r, r * 2.0f, r * 2.0f);
    g.setColour (bgTint.withAlpha (0.45f));
    g.fillEllipse (cx - r, cy - r, r * 2.0f, r * 2.0f);

    g.setColour (juce::Colour (0xFF010305).withAlpha (0.30f));
    for (float sy = cy - r + 1.0f; sy < cy + r; sy += 3.0f)
        g.drawHorizontalLine ((int) sy, cx - r, cx + r);

    for (int ring = 1; ring <= 4; ++ring)
    {
        const float ri    = r * ((float) ring / 4.0f);
        const bool  outer = (ring == 4);
        g.setColour (cyberBlue.withAlpha (outer ? 0.25f : 0.08f));
        g.drawEllipse (cx - ri, cy - ri, ri * 2.0f, ri * 2.0f,
                       outer ? 1.0f : 0.6f);
    }

    g.setColour (cyberBlue.withAlpha (0.12f));
    g.drawLine (cx - r, cy,     cx + r, cy,     0.75f);
    g.drawLine (cx,     cy - r, cx,     cy + r, 0.75f);

    g.setColour (cyberBlue.withAlpha (0.05f));
    const float d45 = r * 0.707f;
    g.drawLine (cx - d45, cy - d45, cx + d45, cy + d45, 0.5f);
    g.drawLine (cx - d45, cy + d45, cx + d45, cy - d45, 0.5f);

    // APVTS spatial params stored as [0,1]; remap to bipolar [-1,+1] for orb.
    auto* pSx = apvts.getRawParameterValue ("spatial_x");
    auto* pSy = apvts.getRawParameterValue ("spatial_y");
    if (pSx == nullptr || pSy == nullptr) return;
    const float sxRaw = pSx->load();
    const float syRaw = pSy->load();
    const float sx    = sxRaw * 2.0f - 1.0f;
    const float sy    = syRaw * 2.0f - 1.0f;

    const float orbR  = 5.0f;
    const float safeR = r - 20.0f - orbR;
    const float orbX  = cx + sx * safeR;
    const float orbY  = cy - sy * safeR;

    if (level > 0.02f)
    {
        const float glowAlpha = juce::jlimit (0.0f, 0.40f, level * 0.45f);
        g.setColour (cyberBlue.withAlpha (glowAlpha));
        g.fillEllipse (orbX - orbR * 3.8f, orbY - orbR * 3.8f,
                       orbR * 7.6f, orbR * 7.6f);
        g.setColour (cyberBlue.withAlpha (glowAlpha * 0.5f));
        g.drawEllipse (orbX - orbR * 2.8f, orbY - orbR * 2.8f,
                       orbR * 5.6f, orbR * 5.6f, 0.75f);
    }

    g.setColour (cyberBlue);
    g.fillEllipse (orbX - orbR, orbY - orbR, orbR * 2.0f, orbR * 2.0f);

    g.setColour (juce::Colours::white.withAlpha (0.45f));
    g.fillEllipse (orbX - orbR * 0.5f, orbY - orbR * 0.75f,
                   orbR * 0.7f, orbR * 0.55f);

    g.setColour (cyberBlue.withAlpha (0.18f));
    g.drawLine (cx, cy, orbX, orbY, 0.5f);

    const float headR = 7.5f;
    g.setColour (juce::Colour (0xFF010305));
    g.fillEllipse (cx - headR, cy - headR, headR * 2.0f, headR * 2.0f);
    g.setColour (cyberBlue.withAlpha (0.92f));
    g.drawEllipse (cx - headR, cy - headR, headR * 2.0f, headR * 2.0f, 1.5f);

    g.setColour (cyberBlue);
    g.fillEllipse (cx - 2.2f, cy - headR - 5.5f, 4.4f, 4.4f);

    g.setColour (cyberBlue.withAlpha (0.50f));
    g.fillEllipse (cx - headR - 3.0f, cy - 2.0f, 3.5f, 4.0f);
    g.fillEllipse (cx + headR - 0.5f, cy - 2.0f, 3.5f, 4.0f);

    g.setColour (cyberBlue.withAlpha (0.32f));
    g.drawLine (cx - 4.5f, cy, cx + 4.5f, cy, 0.75f);
    g.drawLine (cx, cy - 4.5f, cx, cy + 4.5f, 0.75f);

    g.setColour (cyberBlue.withAlpha (0.28f));
    g.setFont (juce::FontOptions ("Courier New", 7.0f, juce::Font::bold));
    g.drawText ("SPATIAL RADAR",
                getLocalBounds().removeFromBottom (12),
                juce::Justification::centred, false);
}

void RadarComponent::mouseDown (const juce::MouseEvent& e)
{
    isDragging = true;
    updateSpatialFromPoint (e.position);
}

void RadarComponent::mouseDrag (const juce::MouseEvent& e)
{
    if (isDragging)
        updateSpatialFromPoint (e.position);
}

void RadarComponent::mouseUp (const juce::MouseEvent&)
{
    isDragging = false;
}

juce::Colour RadarComponent::getAtmosphereTint() const
{
    auto* p = apvts.getRawParameterValue ("atmosphere_state");
    if (p == nullptr) return juce::Colour (0xFF002233);
    const int s = juce::jlimit (0, 4, (int) p->load());
    switch (s)
    {
        case 0:  return juce::Colour (0xFF003311);
        case 1:  return juce::Colour (0xFF112233);
        case 2:  return juce::Colour (0xFF332200);
        case 3:  return juce::Colour (0xFF220033);
        default: return juce::Colour (0xFF002233);
    }
}

void RadarComponent::updateSpatialFromPoint (juce::Point<float> pos)
{
    const float cx    = (float) getWidth()  * 0.5f;
    const float cy    = (float) getHeight() * 0.5f;
    const float r     = std::min (cx, cy) - 4.0f;
    const float orbR  = 5.0f;
    const float safeR = r - 20.0f - orbR;

    float dx = (pos.x - cx) / safeR;   // raw bipolar [-1, +1]
    float dy = (cy - pos.y) / safeR;   // raw bipolar [-1, +1], +y = up

    const float dist = std::sqrt (dx * dx + dy * dy);
    if (dist > 1.0f) { dx /= dist; dy /= dist; }

    // APVTS params are [0,1]; convert bipolar [-1,+1] → [0,1] for storage.
    if (auto* px = apvts.getParameter ("spatial_x"))
        px->setValueNotifyingHost ((dx + 1.0f) * 0.5f);
    if (auto* py = apvts.getParameter ("spatial_y"))
        py->setValueNotifyingHost ((dy + 1.0f) * 0.5f);

    repaint();
}
