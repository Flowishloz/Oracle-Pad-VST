// RadarComponent.cpp — Stage 1: Full binaural spatial radar visual.
// Design Bible: square module with 20px corner radius, source orb constrained
// within the rounded boundary, Electric Cyan (#00F0FF) pointer and grid.

#include "PluginProcessor.h"
#include "PluginEditor.h"

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------
RadarComponent::RadarComponent (juce::AudioProcessorValueTreeState& a,
                                 std::atomic<float>&                 o)
    : apvts (a), outputLevel (o)
{
    setInterceptsMouseClicks (true, false);
}

// ---------------------------------------------------------------------------
// Paint
// Design Bible radar field:
//   - Atmosphere-tinted ellipse background
//   - Concentric rings at 0.25 / 0.5 / 0.75 / 1.0 normalised radius
//   - Orthogonal + diagonal crosshair guides
//   - 15px listener head at centre (wireframe circle + nose dot pointing UP)
//   - 10px source orb physically bound within the 20px-radius module boundary
//   - Orb glow scales with outputLevel
// ---------------------------------------------------------------------------
void RadarComponent::paint (juce::Graphics& g)
{
    const float cx = (float) getWidth()  * 0.5f;
    const float cy = (float) getHeight() * 0.5f;

    // Field radius: inscribed in component bounds with a small margin.
    const float r = std::min (cx, cy) - 4.0f;

    const juce::Colour cyberBlue = juce::Colour (0xFF00F0FF);
    const juce::Colour bgTint    = getAtmosphereTint();
    const float        level     = outputLevel.load (std::memory_order_relaxed);

    // Background ellipse with atmosphere tint
    g.setColour (juce::Colour (0xFF010305));
    g.fillEllipse (cx - r, cy - r, r * 2.0f, r * 2.0f);
    g.setColour (bgTint.withAlpha (0.45f));
    g.fillEllipse (cx - r, cy - r, r * 2.0f, r * 2.0f);

    // Scanline texture (OEL feel)
    g.setColour (juce::Colour (0xFF010305).withAlpha (0.30f));
    for (float sy = cy - r + 1.0f; sy < cy + r; sy += 3.0f)
        g.drawHorizontalLine ((int) sy, cx - r, cx + r);

    // Concentric rings at 25 / 50 / 75 / 100 %
    for (int ring = 1; ring <= 4; ++ring)
    {
        const float ri    = r * ((float) ring / 4.0f);
        const bool  outer = (ring == 4);
        g.setColour (cyberBlue.withAlpha (outer ? 0.25f : 0.08f));
        g.drawEllipse (cx - ri, cy - ri, ri * 2.0f, ri * 2.0f,
                       outer ? 1.0f : 0.6f);
    }

    // Orthogonal crosshair
    g.setColour (cyberBlue.withAlpha (0.12f));
    g.drawLine (cx - r, cy,      cx + r, cy,      0.75f);
    g.drawLine (cx,     cy - r,  cx,     cy + r,  0.75f);

    // Diagonal guides (45°)
    g.setColour (cyberBlue.withAlpha (0.05f));
    const float d45 = r * 0.707f;
    g.drawLine (cx - d45, cy - d45, cx + d45, cy + d45, 0.5f);
    g.drawLine (cx - d45, cy + d45, cx + d45, cy - d45, 0.5f);

    // Read spatial parameters
    const float sx = apvts.getRawParameterValue ("spatial_x")->load();
    const float sy = apvts.getRawParameterValue ("spatial_y")->load();

    // Source orb — constrained within the rounded-rect boundary.
    // Safe radius: field radius minus corner radius (20px) minus orb radius (5px).
    // This ensures the orb circle never exits the rounded corners.
    const float orbR    = 5.0f;
    const float safeR   = r - 20.0f - orbR;  // inscribed circle for rounded-rect safety
    const float orbX    = cx + sx * safeR;
    const float orbY    = cy - sy * safeR;

    // Level-driven outer glow
    if (level > 0.02f)
    {
        const float glowAlpha = juce::jlimit (0.0f, 0.40f, level * 0.45f);
        g.setColour (cyberBlue.withAlpha (glowAlpha));
        g.fillEllipse (orbX - orbR * 3.8f, orbY - orbR * 3.8f,
                       orbR * 7.6f, orbR * 7.6f);
        // Secondary pulse ring
        g.setColour (cyberBlue.withAlpha (glowAlpha * 0.5f));
        g.drawEllipse (orbX - orbR * 2.8f, orbY - orbR * 2.8f,
                       orbR * 5.6f, orbR * 5.6f, 0.75f);
    }

    // Orb body
    g.setColour (cyberBlue);
    g.fillEllipse (orbX - orbR, orbY - orbR, orbR * 2.0f, orbR * 2.0f);

    // Orb specular highlight
    g.setColour (juce::Colours::white.withAlpha (0.45f));
    g.fillEllipse (orbX - orbR * 0.5f, orbY - orbR * 0.75f,
                   orbR * 0.7f, orbR * 0.55f);

    // Thin connector line from centre to orb
    g.setColour (cyberBlue.withAlpha (0.18f));
    g.drawLine (cx, cy, orbX, orbY, 0.5f);

    // Listener head — 15px diameter (wireframe circle at centre)
    const float headR = 7.5f;
    g.setColour (juce::Colour (0xFF010305));
    g.fillEllipse (cx - headR, cy - headR, headR * 2.0f, headR * 2.0f);
    g.setColour (cyberBlue.withAlpha (0.92f));
    g.drawEllipse (cx - headR, cy - headR, headR * 2.0f, headR * 2.0f, 1.5f);

    // Nose dot pointing UP (spatial Y+, screen Y-)
    g.setColour (cyberBlue);
    g.fillEllipse (cx - 2.2f, cy - headR - 5.5f, 4.4f, 4.4f);

    // Ear markers (left / right small dots for orientation)
    g.setColour (cyberBlue.withAlpha (0.50f));
    g.fillEllipse (cx - headR - 3.0f, cy - 2.0f, 3.5f, 4.0f);   // left ear
    g.fillEllipse (cx + headR - 0.5f, cy - 2.0f, 3.5f, 4.0f);   // right ear

    // Inner alignment cross on listener head
    g.setColour (cyberBlue.withAlpha (0.32f));
    g.drawLine (cx - 4.5f, cy, cx + 4.5f, cy, 0.75f);
    g.drawLine (cx, cy - 4.5f, cx, cy + 4.5f, 0.75f);

    // Footer label
    g.setColour (cyberBlue.withAlpha (0.28f));
    g.setFont (juce::FontOptions ("Courier New", 7.0f, juce::Font::bold));
    g.drawText ("SPATIAL RADAR",
                getLocalBounds().removeFromBottom (12),
                juce::Justification::centred, false);
}

// ---------------------------------------------------------------------------
// Mouse handlers
// ---------------------------------------------------------------------------
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

// ---------------------------------------------------------------------------
// Atmosphere tint (matches main editor state)
// ---------------------------------------------------------------------------
juce::Colour RadarComponent::getAtmosphereTint() const
{
    const int s = juce::jlimit (0, 4,
        (int) apvts.getRawParameterValue ("atmosphere_state")->load());
    switch (s)
    {
        case 0:  return juce::Colour (0xFF003311);
        case 1:  return juce::Colour (0xFF112233);
        case 2:  return juce::Colour (0xFF332200);
        case 3:  return juce::Colour (0xFF220033);
        default: return juce::Colour (0xFF002233);
    }
}

// ---------------------------------------------------------------------------
// Map a local click position to spatial_x / spatial_y APVTS parameters.
// The usable field radius matches the safeR constraint in paint().
// ---------------------------------------------------------------------------
void RadarComponent::updateSpatialFromPoint (juce::Point<float> pos)
{
    const float cx    = (float) getWidth()  * 0.5f;
    const float cy    = (float) getHeight() * 0.5f;
    const float r     = std::min (cx, cy) - 4.0f;
    const float orbR  = 5.0f;
    const float safeR = r - 20.0f - orbR;

    // Raw delta from centre, normalised to [-1, 1] in safeR units
    float dx = (pos.x - cx) / safeR;
    float dy = (cy - pos.y) / safeR;   // flip Y: screen-up = spatial +Y

    // Clamp to unit circle so dragging outside the field stays on the edge
    const float dist = std::sqrt (dx * dx + dy * dy);
    if (dist > 1.0f) { dx /= dist; dy /= dist; }

    if (auto* px = apvts.getParameter ("spatial_x"))
        px->setValueNotifyingHost (px->convertTo0to1 (dx));
    if (auto* py = apvts.getParameter ("spatial_y"))
        py->setValueNotifyingHost (py->convertTo0to1 (dy));

    repaint();
}
