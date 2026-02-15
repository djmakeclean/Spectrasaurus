#include "XYPad.h"

XYPad::XYPad()
{
}

juce::Point<float> XYPad::normalizedToScreen(float x, float y) const
{
    auto bounds = getLocalBounds().toFloat().reduced(10.0f);
    float screenX = bounds.getX() + x * bounds.getWidth();
    float screenY = bounds.getY() + y * bounds.getHeight();
    return { screenX, screenY };
}

juce::Point<float> XYPad::screenToNormalized(const juce::Point<float>& screen) const
{
    auto bounds = getLocalBounds().toFloat().reduced(10.0f);
    float x = (screen.x - bounds.getX()) / bounds.getWidth();
    float y = (screen.y - bounds.getY()) / bounds.getHeight();
    return { x, y };
}

void XYPad::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    auto padBounds = bounds.reduced(10.0f);

    // Background
    g.fillAll(juce::Colour(0xff1e1e1e));

    // Pad area
    g.setColour(juce::Colour(0xff2a2a2a));
    g.fillRect(padBounds);

    // Border
    g.setColour(juce::Colour(0xff3e3e3e));
    g.drawRect(padBounds, 1.0f);

    // Corner labels - use fresh bounds for each to avoid accumulating modifications
    g.setColour(juce::Colours::white);
    g.setFont(14.0f);

    auto labelBounds = padBounds.reduced(5.0f);

    // A - top left
    auto boundsA = labelBounds;
    g.drawText("A", boundsA.removeFromTop(15.0f).removeFromLeft(15.0f),
               juce::Justification::topLeft);

    // B - top right
    auto boundsB = labelBounds;
    g.drawText("B", boundsB.removeFromTop(15.0f).removeFromRight(15.0f),
               juce::Justification::topRight);

    // C - bottom left
    auto boundsC = labelBounds;
    g.drawText("C", boundsC.removeFromBottom(15.0f).removeFromLeft(15.0f),
               juce::Justification::bottomLeft);

    // D - bottom right
    auto boundsD = labelBounds;
    g.drawText("D", boundsD.removeFromBottom(15.0f).removeFromRight(15.0f),
               juce::Justification::bottomRight);

    // Draw position indicator
    auto pos = normalizedToScreen(xPosition, yPosition);
    g.setColour(juce::Colour(0xff4a9eff));
    g.fillEllipse(pos.x - 8, pos.y - 8, 16.0f, 16.0f);

    g.setColour(juce::Colours::white);
    g.drawEllipse(pos.x - 8, pos.y - 8, 16.0f, 16.0f, 2.0f);
}

void XYPad::updatePosition(const juce::Point<float>& pos)
{
    auto normalized = screenToNormalized(pos);
    xPosition = juce::jlimit(0.0f, 1.0f, normalized.x);
    yPosition = juce::jlimit(0.0f, 1.0f, normalized.y);

    repaint();

    if (onValueChanged)
        onValueChanged(xPosition, yPosition);
}

void XYPad::mouseDown(const juce::MouseEvent& event)
{
    updatePosition(event.position);
}

void XYPad::mouseDrag(const juce::MouseEvent& event)
{
    updatePosition(event.position);
}

void XYPad::resized()
{
}
