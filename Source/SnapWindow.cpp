#include "SnapWindow.h"

SnapWindow::SnapWindow()
{
}

void SnapWindow::setFunction(PiecewiseFunction* func)
{
    function = func;
    repaint();
}

juce::Point<float> SnapWindow::normalizedToScreen(float x, float y) const
{
    auto bounds = getLocalBounds().toFloat().reduced(30.0f, 18.0f);
    float screenX = bounds.getX() + x * bounds.getWidth();
    float screenY = bounds.getBottom() - y * bounds.getHeight(); // Flip Y
    return { screenX, screenY };
}

juce::Point<float> SnapWindow::screenToNormalized(const juce::Point<float>& screen) const
{
    auto bounds = getLocalBounds().toFloat().reduced(30.0f, 18.0f);
    float x = (screen.x - bounds.getX()) / bounds.getWidth();
    float y = 1.0f - (screen.y - bounds.getY()) / bounds.getHeight(); // Flip Y
    return { x, y };
}

float SnapWindow::normalizedToFrequency(float normalized) const
{
    const float minFreq = 20.0f;
    float maxFreq = sampleRate / 2.0f;

    // Convert from normalized log scale to frequency
    float logMin = std::log10(minFreq);
    float logMax = std::log10(maxFreq);
    float logFreq = logMin + normalized * (logMax - logMin);

    return std::pow(10.0f, logFreq);
}

juce::String SnapWindow::formatFrequency(float freq) const
{
    if (freq < 1000.0f)
        return juce::String(freq, 1) + " Hz";
    else
        return juce::String(freq / 1000.0f, 2) + " kHz";
}

juce::String SnapWindow::formatYValue(float normalizedY) const
{
    if (windowType == SnapWindowType::Delay)
    {
        float timeMs;
        if (delayLogScale)
        {
            timeMs = std::pow(delayMaxMs, normalizedY);
        }
        else
        {
            timeMs = normalizedY * delayMaxMs;
        }
        return juce::String(timeMs, 1) + " ms";
    }
    else if (windowType == SnapWindowType::Feedback)
    {
        if (normalizedY <= 0.001f)
            return "-inf dB";
        float dB = (normalizedY * 66.0f) - 60.0f;
        return juce::String(dB, 1) + " dB";
    }
    else // Pan
    {
        return juce::String(normalizedY, 2);
    }
}

void SnapWindow::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    auto plotBounds = bounds.reduced(30.0f, 18.0f);

    // Background
    g.fillAll(juce::Colour(0xff1e1e1e));

    // Border
    g.setColour(juce::Colour(0xff3e3e3e));
    g.drawRect(plotBounds, 1.0f);

    // Draw label
    g.setColour(juce::Colours::white);
    g.drawText(labelText, bounds.removeFromTop(15.0f), juce::Justification::centredLeft);

    if (function == nullptr)
        return;

    // Draw 0.5 line for Pan windows
    if (windowType == SnapWindowType::Pan)
    {
        auto halfY = normalizedToScreen(0.5f, 0.5f).y;
        g.setColour(juce::Colour(0xff505050));
        g.drawLine(plotBounds.getX(), halfY, plotBounds.getRight(), halfY, 1.0f);
    }

    // Draw reference lines for Feedback windows
    if (windowType == SnapWindowType::Feedback)
    {
        // dB = (y * 66) - 60, so y = (dB + 60) / 66

        // Prominent 0 dB reference line (unity gain boundary)
        float zeroDBNormY = 60.0f / 66.0f;
        auto zeroDBLineY = normalizedToScreen(0.0f, zeroDBNormY).y;
        g.setColour(juce::Colours::white.withAlpha(0.6f));
        g.drawLine(plotBounds.getX(), zeroDBLineY, plotBounds.getRight(), zeroDBLineY, 2.0f);

        // Faint reference lines at -6, -12, -24 dB
        g.setColour(juce::Colour(0xff404040));
        float refDBs[] = { -6.0f, -12.0f, -24.0f };
        for (float dB : refDBs)
        {
            float normY = (dB + 60.0f) / 66.0f;
            auto lineY = normalizedToScreen(0.0f, normY).y;
            g.drawLine(plotBounds.getX(), lineY, plotBounds.getRight(), lineY, 1.0f);
        }

        // Labels on the RIGHT side to avoid overlapping with "Feedback L" title
        g.setColour(juce::Colours::white.withAlpha(0.7f));
        g.setFont(9.0f);
        int labelX = static_cast<int>(plotBounds.getRight()) - 35;

        // "+6 dB" at top
        g.drawText("+6", labelX, static_cast<int>(plotBounds.getY()) + 1, 33, 10,
                   juce::Justification::centredRight);

        // "0 dB" next to the 0 dB line (only if far enough from top)
        if (zeroDBLineY > plotBounds.getY() + 14)
        {
            g.drawText("0 dB", labelX, static_cast<int>(zeroDBLineY) - 11, 33, 10,
                       juce::Justification::centredRight);
        }

        // "-60 dB" at bottom
        g.drawText("-60", labelX, static_cast<int>(plotBounds.getBottom()) - 12, 33, 10,
                   juce::Justification::centredRight);
    }

    // Draw the piecewise function
    const auto& points = function->getPoints();

    if (points.size() >= 2)
    {
        juce::Path path;
        bool first = true;

        // Draw line segments
        for (size_t i = 0; i < points.size() - 1; ++i)
        {
            auto p1 = normalizedToScreen(points[i].x, points[i].y);
            auto p2 = normalizedToScreen(points[i + 1].x, points[i + 1].y);

            if (first)
            {
                path.startNewSubPath(p1);
                first = false;
            }
            path.lineTo(p2);
        }

        g.setColour(juce::Colour(0xff4a9eff));
        g.strokePath(path, juce::PathStrokeType(2.0f));

        // Draw control points
        for (size_t i = 0; i < points.size(); ++i)
        {
            auto p = normalizedToScreen(points[i].x, points[i].y);

            // Endpoints are squares, interior points are circles
            if (i == 0 || i == points.size() - 1)
            {
                g.setColour(juce::Colour(0xff6ab0ff));
                g.fillRect(p.x - 4, p.y - 4, 8.0f, 8.0f);
            }
            else
            {
                g.setColour(juce::Colour(0xff6ab0ff));
                g.fillEllipse(p.x - 5, p.y - 5, 10.0f, 10.0f);
            }
        }
    }

    // Draw pan labels for Pan windows
    if (windowType == SnapWindowType::Pan)
    {
        g.setColour(juce::Colours::white.withAlpha(0.7f));
        g.setFont(10.0f);

        auto labelBounds = plotBounds;
        // "Same" at bottom left
        g.drawText("Same", labelBounds.removeFromBottom(12).removeFromLeft(50),
                   juce::Justification::centredLeft);
        // "Opposite" at top left
        g.drawText("Opposite", plotBounds.removeFromTop(12).removeFromLeft(65),
                   juce::Justification::centredLeft);
    }

    // Draw hover info - always show both X and Y values
    if (isHovering)
    {
        auto normalized = screenToNormalized(hoverPosition);
        if (normalized.x >= 0.0f && normalized.x <= 1.0f &&
            normalized.y >= 0.0f && normalized.y <= 1.0f)
        {
            float freq = normalizedToFrequency(normalized.x);
            auto freqStr = formatFrequency(freq);
            auto yStr = formatYValue(normalized.y);

            // Always display both X and Y values
            juce::String displayText = freqStr + " | " + yStr;

            g.setColour(juce::Colours::white.withAlpha(0.9f));
            g.setFont(12.0f);
            g.drawText(displayText, bounds.removeFromBottom(15.0f), juce::Justification::centredLeft);
        }
    }
}

int SnapWindow::findPointAtPosition(const juce::Point<float>& pos, float tolerance)
{
    if (function == nullptr)
        return -1;

    const auto& points = function->getPoints();
    auto normalized = screenToNormalized(pos);

    return function->findClosestPoint(normalized.x, normalized.y, tolerance / getWidth());
}

void SnapWindow::mouseDown(const juce::MouseEvent& event)
{
    if (function == nullptr)
        return;

    if (event.mods.isPopupMenu())
    {
        juce::PopupMenu menu;
        menu.addItem(1, "Copy Curve");
        menu.addItem(2, "Paste Curve", clipboardFilled != nullptr && *clipboardFilled);
        menu.addItem(3, "Reset Curve");
        menu.addItem(4, "Add X,Y Point");
        auto safeThis = juce::Component::SafePointer<SnapWindow>(this);
        menu.showMenuAsync({}, [safeThis](int result)
        {
            if (safeThis == nullptr) return;
            auto* self = safeThis.getComponent();
            if (result == 1 && self->function && self->clipboard)
            {
                self->clipboard->copyFrom(*self->function);
                if (self->clipboardFilled) *self->clipboardFilled = true;
                if (self->clipboardMeta)
                {
                    self->clipboardMeta->source = ClipboardMeta::Plain;
                    self->clipboardMeta->curveIndex = 0;
                }
            }
            else if (result == 2 && self->function && self->clipboard)
            {
                self->function->copyFrom(*self->clipboard);
                self->repaint();
            }
            else if (result == 3 && self->function)
            {
                self->function->reset();
                self->repaint();
            }
            else if (result == 4 && self->function)
            {
                // Determine hint text based on window type
                juce::String hintText;
                if (self->windowType == SnapWindowType::Delay)
                    hintText = "Enter point as: freq_hz, delay_ms\nExample: 440, 200";
                else if (self->windowType == SnapWindowType::Pan)
                    hintText = "Enter point as: freq_hz, pan_0to1\nExample: 440, 0.5";
                else // Feedback
                    hintText = "Enter point as: freq_hz, feedback_dB\nExample: 440, -12";

                auto* aw = new juce::AlertWindow("Add X,Y Point", hintText, juce::AlertWindow::NoIcon);
                aw->addTextEditor("point", "", "X, Y:");
                aw->addButton("OK", 1);
                aw->addButton("Cancel", 0);

                auto safeInner = juce::Component::SafePointer<SnapWindow>(self);
                aw->enterModalState(true, juce::ModalCallbackFunction::create([safeInner, aw](int r)
                {
                    if (r == 1 && safeInner != nullptr)
                    {
                        auto* s = safeInner.getComponent();
                        auto text = aw->getTextEditorContents("point");

                        // Parse "x, y"
                        int commaIdx = text.indexOfChar(',');
                        if (commaIdx >= 0 && s->function)
                        {
                            float xVal = text.substring(0, commaIdx).trim().getFloatValue();
                            float yVal = text.substring(commaIdx + 1).trim().getFloatValue();

                            // Validate X: frequency in Hz (20 to Nyquist)
                            float nyquist = s->sampleRate / 2.0f;
                            if (xVal >= 20.0f && xVal <= nyquist)
                            {
                                // Convert frequency to normalized X (log scale)
                                float logMin = std::log10(20.0f);
                                float logMax = std::log10(nyquist);
                                float normX = (std::log10(xVal) - logMin) / (logMax - logMin);

                                // Convert Y based on window type
                                float normY = 0.0f;
                                bool valid = false;

                                if (s->windowType == SnapWindowType::Delay)
                                {
                                    // Y in ms (0 to delayMaxMs)
                                    if (yVal >= 0.0f && yVal <= s->delayMaxMs)
                                    {
                                        if (s->delayLogScale && s->delayMaxMs > 1.0f)
                                            normY = std::log(std::max(yVal, 1.0f)) / std::log(s->delayMaxMs);
                                        else
                                            normY = yVal / s->delayMaxMs;
                                        valid = true;
                                    }
                                }
                                else if (s->windowType == SnapWindowType::Pan)
                                {
                                    // Y as 0-1 (0=Same, 1=Opposite)
                                    if (yVal >= 0.0f && yVal <= 1.0f)
                                    {
                                        normY = yVal;
                                        valid = true;
                                    }
                                }
                                else // Feedback
                                {
                                    // Y in dB (-60 to +6)
                                    if (yVal >= -60.0f && yVal <= 6.0f)
                                    {
                                        normY = (yVal + 60.0f) / 66.0f;
                                        valid = true;
                                    }
                                }

                                if (valid)
                                {
                                    normX = juce::jlimit(0.0f, 1.0f, normX);
                                    normY = juce::jlimit(0.0f, 1.0f, normY);
                                    s->function->addPoint(normX, normY);
                                    s->repaint();
                                }
                            }
                        }
                    }
                    delete aw;
                }));
            }
        });
        return;
    }

    mouseDownPosition = event.position;
    hasDraggedSignificantly = false;

    auto pos = event.position;
    int pointIndex = findPointAtPosition(pos, 15.0f); // Increased tolerance

    if (pointIndex >= 0)
    {
        // Start potentially dragging this point
        draggedPointIndex = pointIndex;
    }
    else
    {
        // Will add point on mouseUp if we didn't drag
        draggedPointIndex = -1;
    }
}

void SnapWindow::mouseDrag(const juce::MouseEvent& event)
{
    if (function == nullptr)
        return;

    // Check if we've moved significantly
    float distance = mouseDownPosition.getDistanceFrom(event.position);
    if (distance > 3.0f)
        hasDraggedSignificantly = true;

    // Only drag if we clicked on a point
    if (draggedPointIndex >= 0 && hasDraggedSignificantly)
    {
        auto normalized = screenToNormalized(event.position);
        normalized.x = juce::jlimit(0.0f, 1.0f, normalized.x);
        normalized.y = juce::jlimit(0.0f, 1.0f, normalized.y);

        // Store the old X position to detect crossing other points
        const auto& points = function->getPoints();
        float oldX = points[draggedPointIndex].x;

        function->updatePoint(draggedPointIndex, normalized.x, normalized.y);

        // After updatePoint, the points may have been re-sorted
        // Find the point we're dragging (it might have moved in the array)
        int newIndex = -1;
        for (size_t i = 0; i < points.size(); ++i)
        {
            if (std::abs(points[i].x - normalized.x) < 0.001f &&
                std::abs(points[i].y - normalized.y) < 0.001f)
            {
                newIndex = static_cast<int>(i);
                break;
            }
        }

        if (newIndex >= 0)
            draggedPointIndex = newIndex;
    }

    hoverPosition = event.position;
    isHovering = true;
    repaint();
}

void SnapWindow::mouseUp(const juce::MouseEvent& event)
{
    if (function == nullptr)
        return;

    if (event.mods.isPopupMenu())
        return;

    // If we didn't drag significantly
    if (!hasDraggedSignificantly)
    {
        auto pos = event.position;
        int pointIndex = findPointAtPosition(pos, 15.0f);

        if (pointIndex >= 0)
        {
            // Click on point without dragging = remove it
            function->removePoint(pointIndex);
            repaint();
        }
        else
        {
            // Click on empty space without dragging = add point
            auto normalized = screenToNormalized(pos);
            if (normalized.x >= 0.0f && normalized.x <= 1.0f &&
                normalized.y >= 0.0f && normalized.y <= 1.0f)
            {
                function->addPoint(normalized.x, normalized.y);
                repaint();
            }
        }
    }

    // Reset drag state
    draggedPointIndex = -1;
    hasDraggedSignificantly = false;
}

void SnapWindow::mouseMove(const juce::MouseEvent& event)
{
    hoverPosition = event.position;
    isHovering = true;
    repaint();
}

void SnapWindow::mouseExit(const juce::MouseEvent& event)
{
    isHovering = false;
    draggedPointIndex = -1;
    hasDraggedSignificantly = false;
    repaint();
}

void SnapWindow::resized()
{
}
