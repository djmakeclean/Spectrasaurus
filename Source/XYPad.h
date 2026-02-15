#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_graphics/juce_graphics.h>

class XYPad : public juce::Component
{
public:
    XYPad();
    ~XYPad() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override;

    void mouseDown(const juce::MouseEvent& event) override;
    void mouseDrag(const juce::MouseEvent& event) override;

    // Get/Set position (0.0 to 1.0)
    float getX() const { return xPosition; }
    float getY() const { return yPosition; }

    void setX(float x) { xPosition = juce::jlimit(0.0f, 1.0f, x); repaint(); }
    void setY(float y) { yPosition = juce::jlimit(0.0f, 1.0f, y); repaint(); }

    // Callback for value changes
    std::function<void(float x, float y)> onValueChanged;

private:
    float xPosition = 0.0f; // 0 = A/C, 1 = B/D
    float yPosition = 0.0f; // 0 = A/B, 1 = C/D

    juce::Point<float> normalizedToScreen(float x, float y) const;
    juce::Point<float> screenToNormalized(const juce::Point<float>& screen) const;

    void updatePosition(const juce::Point<float>& pos);
};
