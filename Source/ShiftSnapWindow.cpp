#include "ShiftSnapWindow.h"

ShiftSnapWindow::ShiftSnapWindow()
{
    curveSelector.addItem("Shift", 1);
    curveSelector.addItem("Multiply", 2);
    curveSelector.setSelectedId(1, juce::dontSendNotification);
    curveSelector.onChange = [this] {
        setActiveCurve(curveSelector.getSelectedId() - 1);
    };
    addAndMakeVisible(curveSelector);

    settingsButton.setButtonText("Zoom");
    settingsButton.setClickingTogglesState(true);
    settingsButton.onClick = [this] { toggleSettings(); };
    addAndMakeVisible(settingsButton);

    // Settings pane: range editors
    const char* rowNames[2] = { "Shift", "Multiply" };
    const char* colNames[2] = { "Min", "Max" };

    for (int i = 0; i < 2; ++i)
    {
        rangeColLabels[i].setText(colNames[i], juce::dontSendNotification);
        rangeColLabels[i].setJustificationType(juce::Justification::centred);
        rangeColLabels[i].setFont(10.0f);
        rangeColLabels[i].setColour(juce::Label::textColourId, juce::Colours::grey);
        addChildComponent(rangeColLabels[i]);
    }

    for (int row = 0; row < 2; ++row)
    {
        rangeRowLabels[row].setText(rowNames[row], juce::dontSendNotification);
        rangeRowLabels[row].setJustificationType(juce::Justification::centredRight);
        rangeRowLabels[row].setFont(10.0f);
        rangeRowLabels[row].setColour(juce::Label::textColourId, juce::Colours::white);
        addChildComponent(rangeRowLabels[row]);

        for (int col = 0; col < 2; ++col)
        {
            auto& editor = rangeEditors[row][col];
            editor.setJustification(juce::Justification::centred);
            editor.setInputRestrictions(8, "-0123456789.");
            editor.setFont(10.0f);
            editor.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff2a2a2a));
            editor.setColour(juce::TextEditor::outlineColourId, juce::Colour(0xff4e4e4e));
            editor.setColour(juce::TextEditor::textColourId, juce::Colours::white);
            editor.onReturnKey = [this] { applyRangeFromEditors(); };
            editor.onFocusLost = [this] { applyRangeFromEditors(); };
            addChildComponent(editor);
        }
    }

    // Default display range values
    rangeEditors[0][0].setText("-500", false);   // Shift min Hz
    rangeEditors[0][1].setText("500", false);    // Shift max Hz
    rangeEditors[1][0].setText("0.5", false);    // Multiply min
    rangeEditors[1][1].setText("2.0", false);    // Multiply max

    // Order toggle button (small, next to Zoom)
    orderButton.setButtonText("Shift>Mult");
    orderButton.setClickingTogglesState(true);
    orderButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff2a2a2a));
    orderButton.onClick = [this] {
        bool toggledOn = orderButton.getToggleState();
        if (shiftBeforeMultiply != nullptr)
            *shiftBeforeMultiply = !toggledOn;
        orderButton.setButtonText(toggledOn ? "Mult>Shift" : "Shift>Mult");
        if (onSettingsChanged)
            onSettingsChanged();
    };
    addAndMakeVisible(orderButton);
}

void ShiftSnapWindow::setCurves(PiecewiseFunction* shift, PiecewiseFunction* multiply)
{
    curves[0] = shift;
    curves[1] = multiply;
    repaint();
}

void ShiftSnapWindow::setActiveCurve(int index)
{
    activeCurveIndex = std::clamp(index, 0, 1);
    curveSelector.setSelectedId(activeCurveIndex + 1, juce::dontSendNotification);
    if (onCurveSelectionChanged)
        onCurveSelectionChanged(activeCurveIndex);
    repaint();
}

PiecewiseFunction* ShiftSnapWindow::getActiveFunction() const
{
    return curves[activeCurveIndex];
}

void ShiftSnapWindow::toggleSettings()
{
    showSettings = settingsButton.getToggleState();

    for (int i = 0; i < 2; ++i)
        rangeColLabels[i].setVisible(showSettings);
    for (int row = 0; row < 2; ++row)
    {
        rangeRowLabels[row].setVisible(showSettings);
        for (int col = 0; col < 2; ++col)
            rangeEditors[row][col].setVisible(showSettings);
    }

    repaint();
}

void ShiftSnapWindow::applyRangeFromEditors()
{
    // Shift range (display-only zoom)
    float sMin = rangeEditors[0][0].getText().getFloatValue();
    float sMax = rangeEditors[0][1].getText().getFloatValue();
    if (sMax <= sMin) sMax = sMin + 1.0f;
    sMin = juce::jlimit(-10000.0f, 10000.0f, sMin);
    sMax = juce::jlimit(-9999.0f, 10000.0f, sMax);
    if (sMax <= sMin) sMax = sMin + 1.0f;
    shiftRange.minHz = sMin;
    shiftRange.maxHz = sMax;
    rangeEditors[0][0].setText(juce::String(sMin, 0), false);
    rangeEditors[0][1].setText(juce::String(sMax, 0), false);

    // Multiply range (display-only zoom, must be > 0)
    float mMin = rangeEditors[1][0].getText().getFloatValue();
    float mMax = rangeEditors[1][1].getText().getFloatValue();
    if (mMin <= 0.0f) mMin = 0.01f;
    if (mMax <= mMin) mMax = mMin * 2.0f;
    mMin = juce::jlimit(0.01f, 100.0f, mMin);
    mMax = juce::jlimit(0.02f, 100.0f, mMax);
    if (mMax <= mMin) mMax = mMin * 2.0f;
    multRange.minMult = mMin;
    multRange.maxMult = mMax;
    rangeEditors[1][0].setText(juce::String(mMin, 2), false);
    rangeEditors[1][1].setText(juce::String(mMax, 2), false);

    // Display ranges don't call onSettingsChanged (audio doesn't change)
    repaint();
}

// Convert absolute value (Hz or factor) to display fraction (0=bottom, 1=top)
float ShiftSnapWindow::valueToFraction(float absValue) const
{
    if (activeCurveIndex == 0)
    {
        // Shift: linear
        float span = shiftRange.maxHz - shiftRange.minHz;
        if (span <= 0.0f) return 0.5f;
        return (absValue - shiftRange.minHz) / span;
    }
    else
    {
        // Multiply: logarithmic
        if (multRange.minMult <= 0.0f || multRange.maxMult <= 0.0f) return 0.5f;
        float logMin = std::log10(multRange.minMult);
        float logMax = std::log10(multRange.maxMult);
        float logSpan = logMax - logMin;
        if (logSpan <= 0.0f) return 0.5f;
        return (std::log10(std::max(absValue, 0.001f)) - logMin) / logSpan;
    }
}

// Convert display fraction (0=bottom, 1=top) to absolute value
float ShiftSnapWindow::fractionToValue(float fraction) const
{
    if (activeCurveIndex == 0)
    {
        return shiftRange.minHz + fraction * (shiftRange.maxHz - shiftRange.minHz);
    }
    else
    {
        float logMin = std::log10(std::max(multRange.minMult, 0.001f));
        float logMax = std::log10(std::max(multRange.maxMult, 0.001f));
        return std::pow(10.0f, logMin + fraction * (logMax - logMin));
    }
}

juce::Point<float> ShiftSnapWindow::normalizedToScreen(float x, float y) const
{
    float screenX = plotBounds.getX() + x * plotBounds.getWidth();

    // Convert normalized Y to absolute value, then to display fraction
    float absValue;
    if (activeCurveIndex == 0)
        absValue = normalizedYToHz(y);
    else
        absValue = normalizedYToFactor(y);

    float fraction = valueToFraction(absValue);
    float screenY = plotBounds.getBottom() - fraction * plotBounds.getHeight();
    return { screenX, screenY };
}

juce::Point<float> ShiftSnapWindow::screenToNormalized(const juce::Point<float>& screen) const
{
    float x = (screen.x - plotBounds.getX()) / plotBounds.getWidth();
    float fraction = 1.0f - (screen.y - plotBounds.getY()) / plotBounds.getHeight();

    // Convert display fraction to absolute value, then to normalized Y
    float absValue = fractionToValue(fraction);
    float y;
    if (activeCurveIndex == 0)
        y = hzToNormalizedY(absValue);
    else
        y = factorToNormalizedY(absValue);

    return { x, y };
}

float ShiftSnapWindow::normalizedToFrequency(float normalized) const
{
    const float minFreq = 20.0f;
    float maxFreq = sampleRate / 2.0f;
    float logMin = std::log10(minFreq);
    float logMax = std::log10(maxFreq);
    float logFreq = logMin + normalized * (logMax - logMin);
    return std::pow(10.0f, logFreq);
}

juce::String ShiftSnapWindow::formatFrequency(float freq) const
{
    if (freq < 1000.0f)
        return juce::String(freq, 1) + " Hz";
    else
        return juce::String(freq / 1000.0f, 2) + " kHz";
}

juce::String ShiftSnapWindow::formatYValue(float normalizedY) const
{
    if (activeCurveIndex == 0)
    {
        float hz = normalizedYToHz(normalizedY);
        if (std::abs(hz) < 0.05f) return "0 Hz";
        return juce::String(hz, 1) + " Hz";
    }
    else
    {
        float factor = normalizedYToFactor(normalizedY);
        return juce::String(factor, 3) + "x";
    }
}

void ShiftSnapWindow::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff1e1e1e));

    if (showSettings)
        paintSettingsView(g);
    else
        paintCurveView(g);
}

void ShiftSnapWindow::paintSettingsView(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    g.setColour(juce::Colours::white);
    g.setFont(11.0f);
    g.drawText(labelText + " - Zoom", bounds.removeFromTop(20.0f).reduced(4, 0),
               juce::Justification::centredLeft);

    g.setColour(juce::Colour(0xff3e3e3e));
    g.drawRect(plotBounds, 1.0f);

    // Draw unit labels to the right of each row
    g.setColour(juce::Colours::grey);
    g.setFont(9.0f);
    if (rangeEditors[0][1].getBounds().getWidth() > 0)
    {
        auto shiftMaxB = rangeEditors[0][1].getBounds();
        g.drawText("Hz", shiftMaxB.getRight() + 1, shiftMaxB.getY(),
                   18, shiftMaxB.getHeight(), juce::Justification::centredLeft);

        auto multMaxB = rangeEditors[1][1].getBounds();
        g.drawText("x", multMaxB.getRight() + 1, multMaxB.getY(),
                   18, multMaxB.getHeight(), juce::Justification::centredLeft);
    }
}

void ShiftSnapWindow::paintCurveView(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    // Label in control strip
    g.setColour(juce::Colours::white);
    g.setFont(11.0f);
    g.drawText(labelText, bounds.removeFromTop(20.0f).reduced(4, 0),
               juce::Justification::centredLeft);

    // Border
    g.setColour(juce::Colour(0xff3e3e3e));
    g.drawRect(plotBounds, 1.0f);

    // Y-axis labels showing display range
    g.setColour(juce::Colours::white.withAlpha(0.7f));
    g.setFont(10.0f);

    if (activeCurveIndex == 0)
    {
        juce::String topLabel = juce::String(shiftRange.maxHz, 0) + " Hz";
        juce::String botLabel = juce::String(shiftRange.minHz, 0) + " Hz";
        g.drawText(topLabel, static_cast<int>(plotBounds.getX()) + 2,
                   static_cast<int>(plotBounds.getY()) + 1, 60, 12,
                   juce::Justification::centredLeft);
        g.drawText(botLabel, static_cast<int>(plotBounds.getX()) + 2,
                   static_cast<int>(plotBounds.getBottom()) - 13, 60, 12,
                   juce::Justification::centredLeft);

        // Reference line at 0 Hz
        float fraction0 = valueToFraction(0.0f);
        if (fraction0 > 0.02f && fraction0 < 0.98f)
        {
            float lineY = plotBounds.getBottom() - fraction0 * plotBounds.getHeight();
            g.setColour(juce::Colours::white.withAlpha(0.5f));
            g.drawLine(plotBounds.getX(), lineY, plotBounds.getRight(), lineY, 1.5f);
            g.setFont(9.0f);
            g.drawText("0 Hz", static_cast<int>(plotBounds.getRight()) - 30,
                       static_cast<int>(lineY) - 10, 28, 10,
                       juce::Justification::centredRight);
        }
    }
    else
    {
        juce::String topLabel = juce::String(multRange.maxMult, 2) + "x";
        juce::String botLabel = juce::String(multRange.minMult, 2) + "x";
        g.drawText(topLabel, static_cast<int>(plotBounds.getX()) + 2,
                   static_cast<int>(plotBounds.getY()) + 1, 50, 12,
                   juce::Justification::centredLeft);
        g.drawText(botLabel, static_cast<int>(plotBounds.getX()) + 2,
                   static_cast<int>(plotBounds.getBottom()) - 13, 50, 12,
                   juce::Justification::centredLeft);

        // Reference line at 1.0x
        float fraction1 = valueToFraction(1.0f);
        if (fraction1 > 0.02f && fraction1 < 0.98f)
        {
            float lineY = plotBounds.getBottom() - fraction1 * plotBounds.getHeight();
            g.setColour(juce::Colours::white.withAlpha(0.5f));
            g.drawLine(plotBounds.getX(), lineY, plotBounds.getRight(), lineY, 1.5f);
            g.setFont(9.0f);
            g.drawText("1.0x", static_cast<int>(plotBounds.getRight()) - 30,
                       static_cast<int>(lineY) - 10, 28, 10,
                       juce::Justification::centredRight);
        }
    }

    // Clip region for curves
    g.saveState();
    g.reduceClipRegion(plotBounds.toNearestInt());

    // Draw active curve
    auto* func = getActiveFunction();
    if (func != nullptr)
    {
        const auto& points = func->getPoints();
        if (points.size() >= 2)
        {
            juce::Path path;
            bool first = true;
            for (size_t i = 0; i < points.size() - 1; ++i)
            {
                auto p1 = normalizedToScreen(points[i].x, points[i].y);
                auto p2 = normalizedToScreen(points[i + 1].x, points[i + 1].y);
                if (first) { path.startNewSubPath(p1); first = false; }
                path.lineTo(p2);
            }

            g.setColour(juce::Colour(0xff4a9eff));
            g.strokePath(path, juce::PathStrokeType(2.0f));

            for (size_t i = 0; i < points.size(); ++i)
            {
                auto p = normalizedToScreen(points[i].x, points[i].y);
                if (p.y >= plotBounds.getY() - 5 && p.y <= plotBounds.getBottom() + 5)
                {
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
        }
    }

    g.restoreState();

    // Hover info
    if (isHovering && !showSettings)
    {
        auto normalized = screenToNormalized(hoverPosition);
        if (normalized.x >= 0.0f && normalized.x <= 1.0f)
        {
            float freq = normalizedToFrequency(normalized.x);
            auto freqStr = formatFrequency(freq);

            // Show absolute value from screen position
            float fraction = 1.0f - (hoverPosition.y - plotBounds.getY()) / plotBounds.getHeight();
            juce::String yStr;
            if (activeCurveIndex == 0)
            {
                float hz = fractionToValue(fraction);
                yStr = juce::String(hz, 1) + " Hz";
            }
            else
            {
                float factor = fractionToValue(fraction);
                yStr = juce::String(factor, 3) + "x";
            }

            juce::String displayText = freqStr + " | " + yStr;

            g.setColour(juce::Colours::white.withAlpha(0.9f));
            g.setFont(11.0f);
            auto hoverBounds = getLocalBounds().toFloat();
            g.drawText(displayText, hoverBounds.removeFromBottom(14.0f).reduced(4, 0),
                       juce::Justification::centredLeft);
        }
    }
}

int ShiftSnapWindow::findPointAtPosition(const juce::Point<float>& pos, float tolerance)
{
    auto* func = getActiveFunction();
    if (func == nullptr) return -1;
    auto normalized = screenToNormalized(pos);
    return func->findClosestPoint(normalized.x, normalized.y, tolerance / getWidth());
}

void ShiftSnapWindow::mouseDown(const juce::MouseEvent& event)
{
    if (showSettings) return;

    auto* func = getActiveFunction();
    if (func == nullptr) return;

    if (event.mods.isPopupMenu())
    {
        juce::PopupMenu menu;
        menu.addItem(1, "Copy Curve");
        menu.addItem(2, "Paste Curve", clipboardFilled != nullptr && *clipboardFilled);
        menu.addItem(3, "Reset Curve");
        menu.addItem(4, "Add X,Y Point");
        auto safeThis = juce::Component::SafePointer<ShiftSnapWindow>(this);
        int curveIdx = activeCurveIndex;
        menu.showMenuAsync({}, [safeThis, curveIdx](int result)
        {
            if (safeThis == nullptr) return;
            auto* self = safeThis.getComponent();
            auto* activeFunc = self->getActiveFunction();
            if (result == 1 && activeFunc && self->clipboard)
            {
                self->clipboard->copyFrom(*activeFunc);
                if (self->clipboardFilled) *self->clipboardFilled = true;
                if (self->clipboardMeta)
                {
                    self->clipboardMeta->source = ClipboardMeta::Shift;
                    self->clipboardMeta->curveIndex = curveIdx;
                    self->clipboardMeta->shiftMinHz = self->shiftRange.minHz;
                    self->clipboardMeta->shiftMaxHz = self->shiftRange.maxHz;
                    self->clipboardMeta->multMin = self->multRange.minMult;
                    self->clipboardMeta->multMax = self->multRange.maxMult;
                }
            }
            else if (result == 2 && activeFunc && self->clipboard)
            {
                activeFunc->copyFrom(*self->clipboard);
                // Apply zoom if source was also a Shift window
                if (self->clipboardMeta && self->clipboardMeta->source == ClipboardMeta::Shift)
                {
                    if (self->activeCurveIndex == 0)
                    {
                        self->shiftRange.minHz = self->clipboardMeta->shiftMinHz;
                        self->shiftRange.maxHz = self->clipboardMeta->shiftMaxHz;
                    }
                    else
                    {
                        self->multRange.minMult = self->clipboardMeta->multMin;
                        self->multRange.maxMult = self->clipboardMeta->multMax;
                    }
                    self->syncSettings();
                }
                self->repaint();
            }
            else if (result == 3 && activeFunc)
            {
                activeFunc->reset(0.5f);
                self->repaint();
            }
            else if (result == 4 && activeFunc)
            {
                juce::String hintText;
                if (curveIdx == 0)
                    hintText = "Enter point as: freq_hz, shift_hz\nExample: 440, -200";
                else
                    hintText = "Enter point as: freq_hz, multiply_factor\nExample: 440, 2.0";

                auto* aw = new juce::AlertWindow("Add X,Y Point", hintText, juce::AlertWindow::NoIcon);
                aw->addTextEditor("point", "", "X, Y:");
                aw->addButton("OK", 1);
                aw->addButton("Cancel", 0);

                auto safeInner = juce::Component::SafePointer<ShiftSnapWindow>(self);
                aw->enterModalState(true, juce::ModalCallbackFunction::create([safeInner, aw, curveIdx](int r)
                {
                    if (r == 1 && safeInner != nullptr)
                    {
                        auto* s = safeInner.getComponent();
                        auto* pointFunc = s->getActiveFunction();
                        auto text = aw->getTextEditorContents("point");

                        int commaIdx = text.indexOfChar(',');
                        if (commaIdx >= 0 && pointFunc)
                        {
                            float xVal = text.substring(0, commaIdx).trim().getFloatValue();
                            float yVal = text.substring(commaIdx + 1).trim().getFloatValue();

                            float nyquist = s->sampleRate / 2.0f;
                            if (xVal >= 20.0f && xVal <= nyquist)
                            {
                                float logMin = std::log10(20.0f);
                                float logMax = std::log10(nyquist);
                                float normX = (std::log10(xVal) - logMin) / (logMax - logMin);

                                float normY = 0.0f;
                                bool valid = false;

                                if (curveIdx == 0)
                                {
                                    // Shift: Y in Hz (-10000 to +10000)
                                    if (yVal >= -10000.0f && yVal <= 10000.0f)
                                    {
                                        normY = hzToNormalizedY(yVal);
                                        valid = true;
                                    }
                                }
                                else
                                {
                                    // Multiply: Y as factor (0.1 to 10.0)
                                    if (yVal >= 0.1f && yVal <= 10.0f)
                                    {
                                        normY = factorToNormalizedY(yVal);
                                        valid = true;
                                    }
                                }

                                if (valid)
                                {
                                    normX = juce::jlimit(0.0f, 1.0f, normX);
                                    normY = juce::jlimit(0.0f, 1.0f, normY);
                                    pointFunc->addPoint(normX, normY);
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
    int pointIndex = findPointAtPosition(event.position, 15.0f);
    draggedPointIndex = (pointIndex >= 0) ? pointIndex : -1;
}

void ShiftSnapWindow::mouseDrag(const juce::MouseEvent& event)
{
    if (showSettings) return;
    auto* func = getActiveFunction();
    if (func == nullptr) return;

    float distance = mouseDownPosition.getDistanceFrom(event.position);
    if (distance > 3.0f) hasDraggedSignificantly = true;

    if (draggedPointIndex >= 0 && hasDraggedSignificantly)
    {
        auto normalized = screenToNormalized(event.position);
        normalized.x = juce::jlimit(0.0f, 1.0f, normalized.x);

        // Clamp Y to visible display range (zoom acts as range limiter)
        float minNormY, maxNormY;
        if (activeCurveIndex == 0)
        {
            minNormY = hzToNormalizedY(shiftRange.minHz);
            maxNormY = hzToNormalizedY(shiftRange.maxHz);
        }
        else
        {
            minNormY = factorToNormalizedY(multRange.minMult);
            maxNormY = factorToNormalizedY(multRange.maxMult);
        }
        normalized.y = juce::jlimit(minNormY, maxNormY, normalized.y);

        func->updatePoint(draggedPointIndex, normalized.x, normalized.y);

        const auto& points = func->getPoints();
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
        if (newIndex >= 0) draggedPointIndex = newIndex;
    }

    hoverPosition = event.position;
    isHovering = true;
    repaint();
}

void ShiftSnapWindow::mouseUp(const juce::MouseEvent& event)
{
    if (showSettings) return;
    auto* func = getActiveFunction();
    if (func == nullptr) return;
    if (event.mods.isPopupMenu()) return;

    if (!hasDraggedSignificantly)
    {
        int pointIndex = findPointAtPosition(event.position, 15.0f);
        if (pointIndex >= 0)
        {
            func->removePoint(pointIndex);
            repaint();
        }
        else
        {
            auto normalized = screenToNormalized(event.position);
            if (normalized.x >= 0.0f && normalized.x <= 1.0f)
            {
                // Clamp Y to visible display range
                float minNormY, maxNormY;
                if (activeCurveIndex == 0)
                {
                    minNormY = hzToNormalizedY(shiftRange.minHz);
                    maxNormY = hzToNormalizedY(shiftRange.maxHz);
                }
                else
                {
                    minNormY = factorToNormalizedY(multRange.minMult);
                    maxNormY = factorToNormalizedY(multRange.maxMult);
                }
                normalized.y = juce::jlimit(minNormY, maxNormY, normalized.y);
                func->addPoint(normalized.x, normalized.y);
                repaint();
            }
        }
    }

    draggedPointIndex = -1;
    hasDraggedSignificantly = false;
}

void ShiftSnapWindow::mouseMove(const juce::MouseEvent& event)
{
    if (showSettings) return;
    hoverPosition = event.position;
    isHovering = true;
    repaint();
}

void ShiftSnapWindow::mouseExit(const juce::MouseEvent&)
{
    isHovering = false;
    draggedPointIndex = -1;
    hasDraggedSignificantly = false;
    repaint();
}

void ShiftSnapWindow::resized()
{
    auto bounds = getLocalBounds();

    // Reserve top strip for controls
    auto controlStrip = bounds.removeFromTop(20);
    int controlHeight = 16;
    int stripY = controlStrip.getY() + 2;

    // Right to left: curveSelector, Zoom, Order button
    int selectorWidth = 75;
    int zoomWidth = 44;
    int orderWidth = 68;
    int gap = 3;
    int rx = controlStrip.getRight() - 2;

    curveSelector.setBounds(rx - selectorWidth, stripY, selectorWidth, controlHeight);
    rx -= selectorWidth + gap;
    settingsButton.setBounds(rx - zoomWidth, stripY, zoomWidth, controlHeight);
    rx -= zoomWidth + gap;
    orderButton.setBounds(rx - orderWidth, stripY, orderWidth, controlHeight);

    // Plot bounds
    plotBounds = bounds.toFloat().reduced(24.0f, 10.0f);

    // Zoom pane layout (compact, no order button here)
    {
        auto settingsArea = plotBounds.toNearestInt().reduced(6, 4);

        int labelColWidth = 42;
        int editorColWidth = 42;
        int rowHeight = 16;
        int headerHeight = 14;
        int zoomGap = 2;
        int startX = settingsArea.getX() + labelColWidth;
        int startY = settingsArea.getY() + 2;

        rangeColLabels[0].setBounds(startX, startY, editorColWidth, headerHeight);
        rangeColLabels[1].setBounds(startX + editorColWidth + zoomGap, startY, editorColWidth, headerHeight);

        startY += headerHeight + zoomGap;

        for (int row = 0; row < 2; ++row)
        {
            int y = startY + row * (rowHeight + zoomGap);
            rangeRowLabels[row].setBounds(settingsArea.getX(), y, labelColWidth - 2, rowHeight);
            rangeEditors[row][0].setBounds(startX, y, editorColWidth, rowHeight);
            rangeEditors[row][1].setBounds(startX + editorColWidth + zoomGap, y, editorColWidth, rowHeight);
        }
    }
}

void ShiftSnapWindow::syncSettings()
{
    if (shiftBeforeMultiply != nullptr)
    {
        bool shiftFirst = *shiftBeforeMultiply;
        orderButton.setToggleState(!shiftFirst, juce::dontSendNotification);
        orderButton.setButtonText(shiftFirst ? "Shift>Mult" : "Mult>Shift");
    }

    rangeEditors[0][0].setText(juce::String(shiftRange.minHz, 0), false);
    rangeEditors[0][1].setText(juce::String(shiftRange.maxHz, 0), false);
    rangeEditors[1][0].setText(juce::String(multRange.minMult, 2), false);
    rangeEditors[1][1].setText(juce::String(multRange.maxMult, 2), false);

    repaint();
}
