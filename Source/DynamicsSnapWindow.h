#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_graphics/juce_graphics.h>
#include "PiecewiseFunction.h"
#include "ClipboardMeta.h"

class DynamicsSnapWindow : public juce::Component
{
public:
    DynamicsSnapWindow();
    ~DynamicsSnapWindow() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override;

    void mouseDown(const juce::MouseEvent& event) override;
    void mouseDrag(const juce::MouseEvent& event) override;
    void mouseUp(const juce::MouseEvent& event) override;
    void mouseMove(const juce::MouseEvent& event) override;
    void mouseExit(const juce::MouseEvent& event) override;

    // Set the 3 curves (preGain, minGate, maxClip)
    void setCurves(PiecewiseFunction* preGain, PiecewiseFunction* minGate, PiecewiseFunction* maxClip);

    // Active curve index: 0=PreGain, 1=MinGate, 2=MaxClip
    void setActiveCurve(int index);
    int getActiveCurve() const { return activeCurveIndex; }

    // Set label
    void setLabel(const juce::String& label) { labelText = label; }

    // Set sample rate
    void setSampleRate(float sr) { sampleRate = sr; }

    // Spectrograph: update with new magnitude data (in dB, -60 to 0)
    void updateSpectrograph(const float* magnitudes, int numBins);

    // Precision access
    float getPrecision() const { return precision; }

    // Clipboard for right-click copy/paste
    PiecewiseFunction* clipboard = nullptr;
    bool* clipboardFilled = nullptr;
    ClipboardMeta* clipboardMeta = nullptr;
    void setClipboard(PiecewiseFunction* cb, bool* filled, ClipboardMeta* meta = nullptr)
    { clipboard = cb; clipboardFilled = filled; clipboardMeta = meta; }

    // Callback when precision changes (so editor can update spectrographEnabled)
    std::function<void()> onPrecisionChanged;

    // Callback when curve selection changes (so editor can persist the selection)
    std::function<void(int)> onCurveSelectionChanged;

    // Sync range editor text from curveRanges values (e.g. after external copy)
    void syncDisplayRanges();

    // Per-curve display range
    struct DisplayRange {
        float minDB = -60.0f;
        float maxDB = 0.0f;
    };
    DisplayRange curveRanges[3]; // PreGain, MinGate, MaxClip

private:
    PiecewiseFunction* curves[3] = { nullptr, nullptr, nullptr }; // preGain, minGate, maxClip
    int activeCurveIndex = 0;

    float sampleRate = 48000.0f;
    juce::String labelText;

    // Spectrograph display data (smoothed, in dB)
    std::vector<float> spectrographDisplay;
    float precision = 0.15f; // smoothing factor, 0 = off

    // Settings mode
    bool showSettings = false;
    juce::TextButton settingsButton;

    // Internal controls
    juce::ComboBox curveSelector;
    juce::Slider precisionSlider;

    // Settings pane components
    juce::TextEditor rangeEditors[3][2]; // [curve][0=min, 1=max]
    juce::Label rangeRowLabels[3];
    juce::Label rangeColLabels[2];

    // Mouse interaction
    int draggedPointIndex = -1;
    juce::Point<float> hoverPosition;
    juce::Point<float> mouseDownPosition;
    bool isHovering = false;
    bool hasDraggedSignificantly = false;

    // Plot bounds (computed in resized, used in paint and mouse handlers)
    juce::Rectangle<float> plotBounds;

    // Coordinate conversion (uses active curve's display range)
    const DisplayRange& getActiveRange() const { return curveRanges[activeCurveIndex]; }

    // dB <-> normalized Y (processing formula, never changes)
    static float normalizedYToDB(float normY) { return normY * 60.0f - 60.0f; }
    static float dBToNormalizedY(float dB) { return (dB + 60.0f) / 60.0f; }

    // dB <-> fraction within active display range (0=bottom, 1=top)
    float dBToFraction(float dB) const;
    float fractionToDB(float fraction) const;

    // Full coordinate conversions (normalized curve coords <-> screen)
    juce::Point<float> normalizedToScreen(float x, float y) const;
    juce::Point<float> screenToNormalized(const juce::Point<float>& screen) const;

    float normalizedToFrequency(float normalized) const;
    juce::String formatFrequency(float freq) const;

    int findPointAtPosition(const juce::Point<float>& pos, float tolerance = 10.0f);

    PiecewiseFunction* getActiveFunction() const;

    void paintCurveView(juce::Graphics& g);
    void paintSettingsView(juce::Graphics& g);
    void toggleSettings();
    void applyRangeFromEditors();

    static constexpr const char* curveNames[3] = { "PreGain", "Gate", "Clip" };
    static constexpr juce::uint32 curveColorsInactive[3] = { 0xff3a6a3a, 0xff6a3a3a, 0xff6a6a3a };
};
