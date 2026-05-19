#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "GranularEngine.h"

// ============================================================================
// WaveformComponent — Phase 15
//
// Responsibilities:
//   · Draws the loaded sample as a min/max waveform thumbnail.
//   · Hosts four draggable markers: Start, End, FadeIn, FadeOut.
//   · Supports horizontal zoom via the mouse wheel.
//   · Runs a 30 fps Timer to refresh 24 grain-playhead visualiser lines.
//   · Implements FileDragAndDropTarget for .wav / .mp3 drop-loading.
//   · Owns the LOOP toggle button, wired to the "gran_loop" APVTS parameter.
// ============================================================================
class WaveformComponent : public juce::Component,
                          public juce::Timer,
                          public juce::FileDragAndDropTarget
{
public:
    std::function<void (const juce::File&)> onFileDropped;

    WaveformComponent (juce::AudioProcessorValueTreeState& apvts,
                       GranularEngine& engine);
    ~WaveformComponent() override;

    void setWaveformData (GranularSampleBuffer::Ptr buffer);

    void paint          (juce::Graphics& g)                         override;
    void resized        ()                                          override;
    void mouseDown      (const juce::MouseEvent& e)                 override;
    void mouseDrag      (const juce::MouseEvent& e)                 override;
    void mouseUp        (const juce::MouseEvent& e)                 override;
    void mouseWheelMove (const juce::MouseEvent& e,
                         const juce::MouseWheelDetails& wheel)      override;

    void timerCallback() override;

    bool isInterestedInFileDrag (const juce::StringArray& files)    override;
    void fileDragEnter          (const juce::StringArray& files,
                                 int x, int y)                      override;
    void fileDragExit           (const juce::StringArray& files)    override;
    void filesDropped           (const juce::StringArray& files,
                                 int x, int y)                      override;

private:
    juce::AudioProcessorValueTreeState& apvts_;
    GranularEngine&                     engine_;

    GranularSampleBuffer::Ptr waveformBuffer_;

    juce::TextButton                                loopButton_ { "LOOP" };
    std::unique_ptr<juce::ButtonParameterAttachment> loopAttachment_;

    double viewStart_ = 0.0;
    double viewEnd_   = 1.0;

    static constexpr double kMinViewSpan = 0.01;

    enum class DragTarget { None, Start, End, FadeIn, FadeOut };
    DragTarget activeDrag_ = DragTarget::None;

    static constexpr const char* kIdStart   = "gran_start";
    static constexpr const char* kIdEnd     = "gran_end";
    static constexpr const char* kIdFadeIn  = "gran_fade_in";
    static constexpr const char* kIdFadeOut = "gran_fade_out";

    static constexpr float kMarkerHitZone = 8.0f;

    static constexpr int kGrainDisplaySlots = 24;
    std::array<float, kGrainDisplaySlots> grainDisplayPos_ {};
    int numActiveGrains_ = 0;

    bool isDragOver_ = false;

    static constexpr int kLoopBarH = 22;

    juce::Rectangle<int> waveArea()   const;
    juce::Rectangle<int> buttonArea() const;

    float normToPixel (float norm)  const noexcept;
    float pixelToNorm (float pixel) const noexcept;

    DragTarget hitTestMarker (int x) const;

    float markerNorm  (const char* paramId) const noexcept;
    void  setMarkerNorm (const char* paramId, float norm) noexcept;
    float markerPixel (DragTarget t) const noexcept;

    void paintBackground  (juce::Graphics& g);
    void paintWaveform    (juce::Graphics& g);
    void paintMarkers     (juce::Graphics& g);
    void paintGrainLines  (juce::Graphics& g);
    void paintDropOverlay (juce::Graphics& g);

    static constexpr juce::uint32 kCyanARGB      = 0xFF00F0FFu;
    static constexpr juce::uint32 kPanelARGB     = 0xFF1A1C22u;
    static constexpr juce::uint32 kStartARGB     = 0xFF00FF88u;
    static constexpr juce::uint32 kEndARGB       = 0xFFFF4444u;
    static constexpr juce::uint32 kFadeInARGB    = 0xFFFFCC00u;
    static constexpr juce::uint32 kFadeOutARGB   = 0xFFFF8800u;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WaveformComponent)
};
