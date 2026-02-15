#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_graphics/juce_graphics.h>
#include "PiecewiseFunction.h"
#include "ClipboardMeta.h"

class ShiftSnapWindow : public juce::Component
{
public:
    ShiftSnapWindow();
    ~ShiftSnapWindow() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override;

    void mouseDown(const juce::MouseEvent& event) override;
    void mouseDrag(const juce::MouseEvent& event) override;
    void mouseUp(const juce::MouseEvent& event) override;
    void mouseMove(const juce::MouseEvent& event) override;
    void mouseExit(const juce::MouseEvent& event) override;

    // Set the 2 curves (shift, multiply)
    void setCurves(PiecewiseFunction* shift, PiecewiseFunction* multiply);

    // Active curve index: 0=Shift, 1=Multiply
    void setActiveCurve(int index);
    int getActiveCurve() const { return activeCurveIndex; }

    void setLabel(const juce::String& label) { labelText = label; }
    void setSampleRate(float sr) { sampleRate = sr; }

    // Clipboard for right-click copy/paste
    PiecewiseFunction* clipboard = nullptr;
    bool* clipboardFilled = nullptr;
    ClipboardMeta* clipboardMeta = nullptr;
    void setClipboard(PiecewiseFunction* cb, bool* filled, ClipboardMeta* meta = nullptr)
    { clipboard = cb; clipboardFilled = filled; clipboardMeta = meta; }

    // Per-curve display range (zoom-only, does NOT affect audio)
    struct ShiftRange {
        float minHz = -500.0f;
        float maxHz = 500.0f;
    };
    struct MultRange {
        float minMult = 0.5f;
        float maxMult = 2.0f;
    };
    ShiftRange shiftRange;
    MultRange multRange;

    // Application order callback
    bool* shiftBeforeMultiply = nullptr; // pointer into Bank setting

    // Callback when order changes (only thing that affects audio)
    std::function<void()> onSettingsChanged;

    // Callback when curve selection changes (so editor can persist the selection)
    std::function<void(int)> onCurveSelectionChanged;

    // Sync UI state from current pointer values
    void syncSettings();

private:
    PiecewiseFunction* curves[2] = { nullptr, nullptr }; // shift, multiply
    int activeCurveIndex = 0;

    float sampleRate = 48000.0f;
    juce::String labelText;

    // Settings mode
    bool showSettings = false;
    juce::TextButton settingsButton;

    // Internal controls
    juce::ComboBox curveSelector;

    // Settings pane components
    juce::TextEditor rangeEditors[2][2]; // [curve][0=min, 1=max]
    juce::Label rangeRowLabels[2];
    juce::Label rangeColLabels[2];
    juce::TextButton orderButton; // toggles shift->mult vs mult->shift

    // Mouse interaction
    int draggedPointIndex = -1;
    juce::Point<float> hoverPosition;
    juce::Point<float> mouseDownPosition;
    bool isHovering = false;
    bool hasDraggedSignificantly = false;

    // Plot bounds
    juce::Rectangle<float> plotBounds;

    // Fixed absolute formulas (same as processor — defines what curve values mean)
    // Shift: Hz = (normalizedY - 0.5) * 20000  (Y=0→-10kHz, Y=0.5→0Hz, Y=1→+10kHz)
    static float normalizedYToHz(float y) { return (y - 0.5f) * 20000.0f; }
    static float hzToNormalizedY(float hz) { return hz / 20000.0f + 0.5f; }

    // Multiply: factor = 0.1 * pow(100, Y)  (Y=0→0.1x, Y=0.5→1.0x, Y=1→10x)
    static float normalizedYToFactor(float y) { return 0.1f * std::pow(100.0f, y); }
    static float factorToNormalizedY(float factor)
    {
        if (factor <= 0.0f) return 0.0f;
        return std::log10(factor * 10.0f) / 2.0f; // = (log10(f) + 1) / 2
    }

    // Display fraction within active zoom range (0=bottom, 1=top)
    float valueToFraction(float absValue) const;
    float fractionToValue(float fraction) const;

    // Screen <-> normalized coordinate conversion (goes through display zoom)
    juce::Point<float> normalizedToScreen(float x, float y) const;
    juce::Point<float> screenToNormalized(const juce::Point<float>& screen) const;

    float normalizedToFrequency(float normalized) const;
    juce::String formatFrequency(float freq) const;
    juce::String formatYValue(float normalizedY) const;

    int findPointAtPosition(const juce::Point<float>& pos, float tolerance = 10.0f);
    PiecewiseFunction* getActiveFunction() const;

    void paintCurveView(juce::Graphics& g);
    void paintSettingsView(juce::Graphics& g);
    void toggleSettings();
    void applyRangeFromEditors();

    static constexpr const char* curveNames[2] = { "Shift", "Multiply" };
};
