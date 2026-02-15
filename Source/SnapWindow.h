#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_graphics/juce_graphics.h>
#include "PiecewiseFunction.h"
#include "ClipboardMeta.h"

enum class SnapWindowType
{
    Delay,
    Pan,
    Feedback
};

class SnapWindow : public juce::Component
{
public:
    SnapWindow();
    ~SnapWindow() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override;

    void mouseDown(const juce::MouseEvent& event) override;
    void mouseDrag(const juce::MouseEvent& event) override;
    void mouseUp(const juce::MouseEvent& event) override;
    void mouseMove(const juce::MouseEvent& event) override;
    void mouseExit(const juce::MouseEvent& event) override;

    // Set the piecewise function to display/edit
    void setFunction(PiecewiseFunction* func);
    PiecewiseFunction* getFunction() { return function; }

    // Set the type of window (Delay or Pan)
    void setType(SnapWindowType type) { windowType = type; }

    // For delay windows: set max time and log scale
    void setDelayMax(float maxMs) { delayMaxMs = maxMs; }
    void setDelayLogScale(bool logScale) { delayLogScale = logScale; }

    // Set label
    void setLabel(const juce::String& label) { labelText = label; }

    // Set sample rate (for frequency display)
    void setSampleRate(float sr) { sampleRate = sr; }

    // Clipboard for right-click copy/paste
    PiecewiseFunction* clipboard = nullptr;
    bool* clipboardFilled = nullptr;
    ClipboardMeta* clipboardMeta = nullptr;
    void setClipboard(PiecewiseFunction* cb, bool* filled, ClipboardMeta* meta = nullptr)
    { clipboard = cb; clipboardFilled = filled; clipboardMeta = meta; }

private:
    PiecewiseFunction* function = nullptr;
    SnapWindowType windowType = SnapWindowType::Pan;

    float delayMaxMs = 1000.0f;
    bool delayLogScale = false;
    float sampleRate = 48000.0f;

    juce::String labelText;

    // Mouse interaction state
    int draggedPointIndex = -1;
    juce::Point<float> hoverPosition;
    juce::Point<float> mouseDownPosition;
    bool isHovering = false;
    bool hasDraggedSignificantly = false;

    // Coordinate conversion
    juce::Point<float> normalizedToScreen(float x, float y) const;
    juce::Point<float> screenToNormalized(const juce::Point<float>& screen) const;

    // Frequency conversion (for X-axis display)
    float normalizedToFrequency(float normalized) const;
    juce::String formatFrequency(float freq) const;
    juce::String formatYValue(float normalizedY) const;

    // Point hit testing
    int findPointAtPosition(const juce::Point<float>& pos, float tolerance = 10.0f);
};
