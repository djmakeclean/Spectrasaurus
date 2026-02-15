#include "DynamicsSnapWindow.h"

DynamicsSnapWindow::DynamicsSnapWindow()
{
    // Curve selector combo box
    curveSelector.addItem("PreGain", 1);
    curveSelector.addItem("Gate", 2);
    curveSelector.addItem("Clip", 3);
    curveSelector.setSelectedId(1, juce::dontSendNotification);
    curveSelector.onChange = [this] {
        setActiveCurve(curveSelector.getSelectedId() - 1);
    };
    addAndMakeVisible(curveSelector);

    // Zoom button
    settingsButton.setButtonText("Zoom");
    settingsButton.setClickingTogglesState(true);
    settingsButton.onClick = [this] { toggleSettings(); };
    addAndMakeVisible(settingsButton);

    // Precision slider (lives in zoom pane, hidden by default)
    precisionSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    precisionSlider.setRange(0.0, 1.0, 0.01);
    precisionSlider.setValue(0.15);
    precisionSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    precisionSlider.setColour(juce::Slider::trackColourId, juce::Colour(0xff2a4a6a));     // dark blue filled portion
    precisionSlider.setColour(juce::Slider::backgroundColourId, juce::Colour(0xff1a1a1a)); // dark unfilled track
    precisionSlider.setColour(juce::Slider::thumbColourId, juce::Colour(0xff4a9eff));      // bright blue thumb
    precisionSlider.onValueChange = [this] {
        precision = static_cast<float>(precisionSlider.getValue());
        if (onPrecisionChanged)
            onPrecisionChanged();
    };
    addChildComponent(precisionSlider); // hidden until settings shown

    // Settings pane: range editors and labels
    const char* rowNames[3] = { "PreGain", "Gate", "Clip" };
    const char* colNames[2] = { "Min dB", "Max dB" };

    for (int i = 0; i < 2; ++i)
    {
        rangeColLabels[i].setText(colNames[i], juce::dontSendNotification);
        rangeColLabels[i].setJustificationType(juce::Justification::centred);
        rangeColLabels[i].setFont(11.0f);
        rangeColLabels[i].setColour(juce::Label::textColourId, juce::Colours::grey);
        addChildComponent(rangeColLabels[i]);
    }

    for (int row = 0; row < 3; ++row)
    {
        rangeRowLabels[row].setText(rowNames[row], juce::dontSendNotification);
        rangeRowLabels[row].setJustificationType(juce::Justification::centredRight);
        rangeRowLabels[row].setFont(11.0f);
        rangeRowLabels[row].setColour(juce::Label::textColourId, juce::Colours::white);
        addChildComponent(rangeRowLabels[row]);

        for (int col = 0; col < 2; ++col)
        {
            auto& editor = rangeEditors[row][col];
            editor.setJustification(juce::Justification::centred);
            editor.setInputRestrictions(6, "-0123456789.");
            editor.setText(col == 0 ? "-60" : "0", false);
            editor.setFont(11.0f);
            editor.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff2a2a2a));
            editor.setColour(juce::TextEditor::outlineColourId, juce::Colour(0xff4e4e4e));
            editor.setColour(juce::TextEditor::textColourId, juce::Colours::white);
            editor.onReturnKey = [this] { applyRangeFromEditors(); };
            editor.onFocusLost = [this] { applyRangeFromEditors(); };
            addChildComponent(editor);
        }
    }
}

void DynamicsSnapWindow::setCurves(PiecewiseFunction* preGain, PiecewiseFunction* minGate, PiecewiseFunction* maxClip)
{
    curves[0] = preGain;
    curves[1] = minGate;
    curves[2] = maxClip;
    repaint();
}

void DynamicsSnapWindow::setActiveCurve(int index)
{
    activeCurveIndex = std::clamp(index, 0, 2);
    curveSelector.setSelectedId(activeCurveIndex + 1, juce::dontSendNotification);
    if (onCurveSelectionChanged)
        onCurveSelectionChanged(activeCurveIndex);
    repaint();
}

PiecewiseFunction* DynamicsSnapWindow::getActiveFunction() const
{
    return curves[activeCurveIndex];
}

void DynamicsSnapWindow::toggleSettings()
{
    showSettings = settingsButton.getToggleState();

    // Show/hide settings components
    for (int i = 0; i < 2; ++i)
        rangeColLabels[i].setVisible(showSettings);
    for (int row = 0; row < 3; ++row)
    {
        rangeRowLabels[row].setVisible(showSettings);
        for (int col = 0; col < 2; ++col)
            rangeEditors[row][col].setVisible(showSettings);
    }
    precisionSlider.setVisible(showSettings);

    repaint();
}

void DynamicsSnapWindow::applyRangeFromEditors()
{
    for (int row = 0; row < 3; ++row)
    {
        float minVal = rangeEditors[row][0].getText().getFloatValue();
        float maxVal = rangeEditors[row][1].getText().getFloatValue();

        // Enforce min < max with at least 1 dB span
        if (maxVal <= minVal)
            maxVal = minVal + 1.0f;

        // Clamp to reasonable range: -60 dB is the processing floor (normalizedY=0),
        // allow above 0 dB for PreGain boost
        minVal = juce::jlimit(-60.0f, 24.0f, minVal);
        maxVal = juce::jlimit(-59.0f, 48.0f, maxVal);
        if (maxVal <= minVal)
            maxVal = minVal + 1.0f;

        curveRanges[row].minDB = minVal;
        curveRanges[row].maxDB = maxVal;

        rangeEditors[row][0].setText(juce::String(minVal, 0), false);
        rangeEditors[row][1].setText(juce::String(maxVal, 0), false);
    }
    repaint();
}

void DynamicsSnapWindow::syncDisplayRanges()
{
    for (int row = 0; row < 3; ++row)
    {
        rangeEditors[row][0].setText(juce::String(curveRanges[row].minDB, 0), false);
        rangeEditors[row][1].setText(juce::String(curveRanges[row].maxDB, 0), false);
    }
    repaint();
}

float DynamicsSnapWindow::dBToFraction(float dB) const
{
    auto& range = getActiveRange();
    if (range.maxDB <= range.minDB) return 0.0f;
    return (dB - range.minDB) / (range.maxDB - range.minDB);
}

float DynamicsSnapWindow::fractionToDB(float fraction) const
{
    auto& range = getActiveRange();
    return range.minDB + fraction * (range.maxDB - range.minDB);
}

juce::Point<float> DynamicsSnapWindow::normalizedToScreen(float x, float y) const
{
    float screenX = plotBounds.getX() + x * plotBounds.getWidth();
    // Convert normalized Y to dB, then to fraction within display range
    float dB = normalizedYToDB(y);
    float fraction = dBToFraction(dB);
    float screenY = plotBounds.getBottom() - fraction * plotBounds.getHeight();
    return { screenX, screenY };
}

juce::Point<float> DynamicsSnapWindow::screenToNormalized(const juce::Point<float>& screen) const
{
    float x = (screen.x - plotBounds.getX()) / plotBounds.getWidth();
    float fraction = 1.0f - (screen.y - plotBounds.getY()) / plotBounds.getHeight();
    float dB = fractionToDB(fraction);
    float y = dBToNormalizedY(dB);
    return { x, y };
}

float DynamicsSnapWindow::normalizedToFrequency(float normalized) const
{
    const float minFreq = 20.0f;
    float maxFreq = sampleRate / 2.0f;
    float logMin = std::log10(minFreq);
    float logMax = std::log10(maxFreq);
    float logFreq = logMin + normalized * (logMax - logMin);
    return std::pow(10.0f, logFreq);
}

juce::String DynamicsSnapWindow::formatFrequency(float freq) const
{
    if (freq < 1000.0f)
        return juce::String(freq, 1) + " Hz";
    else
        return juce::String(freq / 1000.0f, 2) + " kHz";
}

void DynamicsSnapWindow::updateSpectrograph(const float* magnitudes, int numBins)
{
    if (precision <= 0.0f)
    {
        spectrographDisplay.clear();
        return;
    }

    if (static_cast<int>(spectrographDisplay.size()) != numBins)
        spectrographDisplay.assign(numBins, -60.0f);

    float alpha = precision;
    for (int i = 0; i < numBins; ++i)
        spectrographDisplay[i] = spectrographDisplay[i] * (1.0f - alpha) + magnitudes[i] * alpha;
}

void DynamicsSnapWindow::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff1e1e1e));

    if (showSettings)
        paintSettingsView(g);
    else
        paintCurveView(g);
}

void DynamicsSnapWindow::paintSettingsView(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    // Title in control strip area
    g.setColour(juce::Colours::white);
    g.setFont(11.0f);
    g.drawText(labelText + " - Zoom", bounds.removeFromTop(20.0f).reduced(4, 0),
               juce::Justification::centredLeft);

    // Border around the settings area
    g.setColour(juce::Colour(0xff3e3e3e));
    g.drawRect(plotBounds, 1.0f);

    // "Display Precision" label above the slider (to the right of the table)
    if (precisionSlider.isVisible())
    {
        auto sliderBounds = precisionSlider.getBounds();
        g.setColour(juce::Colours::white.withAlpha(0.7f));
        g.setFont(9.0f);
        g.drawText("Display", sliderBounds.getX(), sliderBounds.getY() - 24,
                   sliderBounds.getWidth(), 11, juce::Justification::centred);
        g.drawText("Precision", sliderBounds.getX(), sliderBounds.getY() - 14,
                   sliderBounds.getWidth(), 11, juce::Justification::centred);
    }
}

void DynamicsSnapWindow::paintCurveView(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    // Label in control strip area
    g.setColour(juce::Colours::white);
    g.setFont(11.0f);
    g.drawText(labelText, bounds.removeFromTop(20.0f).reduced(4, 0),
               juce::Justification::centredLeft);

    // Border
    g.setColour(juce::Colour(0xff3e3e3e));
    g.drawRect(plotBounds, 1.0f);

    auto& range = getActiveRange();

    // Y-axis labels showing active range
    g.setColour(juce::Colours::white.withAlpha(0.7f));
    g.setFont(10.0f);

    // Top label (max dB)
    g.drawText(juce::String(range.maxDB, 0) + " dB",
               static_cast<int>(plotBounds.getX()) + 2,
               static_cast<int>(plotBounds.getY()) + 1, 50, 12,
               juce::Justification::centredLeft);

    // Bottom label (min dB)
    g.drawText(juce::String(range.minDB, 0) + " dB",
               static_cast<int>(plotBounds.getX()) + 2,
               static_cast<int>(plotBounds.getBottom()) - 13, 50, 12,
               juce::Justification::centredLeft);

    // Reference lines at adaptive intervals within visible range
    float rangeSpan = range.maxDB - range.minDB;
    float step;
    if (rangeSpan > 30.0f) step = 12.0f;
    else if (rangeSpan > 15.0f) step = 6.0f;
    else if (rangeSpan > 6.0f) step = 3.0f;
    else step = 1.0f;

    g.setColour(juce::Colour(0xff404040));
    for (float dB = std::ceil(range.minDB / step) * step; dB < range.maxDB; dB += step)
    {
        float fraction = dBToFraction(dB);
        if (fraction <= 0.01f || fraction >= 0.99f) continue;
        float lineY = plotBounds.getBottom() - fraction * plotBounds.getHeight();
        g.drawLine(plotBounds.getX(), lineY, plotBounds.getRight(), lineY, 1.0f);
    }

    // Prominent 0 dB line if within range
    {
        float fraction0dB = dBToFraction(0.0f);
        if (fraction0dB > 0.01f && fraction0dB < 0.99f)
        {
            float lineY = plotBounds.getBottom() - fraction0dB * plotBounds.getHeight();
            g.setColour(juce::Colours::white.withAlpha(0.5f));
            g.drawLine(plotBounds.getX(), lineY, plotBounds.getRight(), lineY, 1.5f);
            g.setFont(9.0f);
            g.drawText("0 dB",
                       static_cast<int>(plotBounds.getRight()) - 30,
                       static_cast<int>(lineY) - 10, 28, 10,
                       juce::Justification::centredRight);
        }
    }

    // Set clip region for plot area
    g.saveState();
    g.reduceClipRegion(plotBounds.toNearestInt());

    // Draw spectrograph (behind curves)
    if (!spectrographDisplay.empty() && precision > 0.0f)
    {
        int numBins = static_cast<int>(spectrographDisplay.size());
        float minFreq = 20.0f;
        float maxFreq = sampleRate / 2.0f;
        float logMin = std::log10(minFreq);
        float logMax = std::log10(maxFreq);

        g.setColour(juce::Colour(0x30608090));

        for (int bin = 1; bin < numBins; ++bin)
        {
            float freq = (bin * sampleRate) / (numBins * 2.0f);
            if (freq < minFreq || freq > maxFreq)
                continue;

            float logFreq = std::log10(freq);
            float normX = (logFreq - logMin) / (logMax - logMin);

            float dB = spectrographDisplay[bin];
            float fraction = dBToFraction(dB);
            fraction = std::clamp(fraction, 0.0f, 1.0f);

            float topY = plotBounds.getBottom() - fraction * plotBounds.getHeight();
            float bottomY = plotBounds.getBottom();

            float screenX = plotBounds.getX() + normX * plotBounds.getWidth();
            float barWidth = std::max(1.0f, plotBounds.getWidth() / static_cast<float>(numBins) * 0.8f);
            g.fillRect(screenX - barWidth * 0.5f, topY, barWidth, bottomY - topY);
        }
    }

    // Draw all 3 curves (clipped to plot area by reduceClipRegion)
    for (int c = 0; c < 3; ++c)
    {
        if (curves[c] == nullptr)
            continue;

        bool isActive = (c == activeCurveIndex);
        const auto& points = curves[c]->getPoints();

        if (points.size() < 2)
            continue;

        // Draw curve path
        juce::Path path;
        bool first = true;
        for (size_t i = 0; i < points.size() - 1; ++i)
        {
            auto p1 = normalizedToScreen(points[i].x, points[i].y);
            auto p2 = normalizedToScreen(points[i + 1].x, points[i + 1].y);
            if (first) { path.startNewSubPath(p1); first = false; }
            path.lineTo(p2);
        }

        if (isActive)
            g.setColour(juce::Colour(0xff4a9eff));
        else
            g.setColour(juce::Colour(curveColorsInactive[c]).withAlpha(0.6f));

        g.strokePath(path, juce::PathStrokeType(isActive ? 2.0f : 1.5f));

        // Draw control points only for active curve
        if (isActive)
        {
            for (size_t i = 0; i < points.size(); ++i)
            {
                auto p = normalizedToScreen(points[i].x, points[i].y);
                // Only draw if within visible area
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

    g.restoreState(); // restore clip region

    // Draw hover info
    if (isHovering && !showSettings)
    {
        auto normalized = screenToNormalized(hoverPosition);
        if (normalized.x >= 0.0f && normalized.x <= 1.0f)
        {
            float freq = normalizedToFrequency(normalized.x);
            auto freqStr = formatFrequency(freq);

            // Show dB value directly from screen position
            float fraction = 1.0f - (hoverPosition.y - plotBounds.getY()) / plotBounds.getHeight();
            float dB = fractionToDB(fraction);
            juce::String yStr;
            if (dB <= -119.0f)
                yStr = "-inf dB";
            else
                yStr = juce::String(dB, 1) + " dB";

            juce::String displayText = freqStr + " | " + yStr;

            g.setColour(juce::Colours::white.withAlpha(0.9f));
            g.setFont(11.0f);
            auto hoverBounds = getLocalBounds().toFloat();
            g.drawText(displayText, hoverBounds.removeFromBottom(14.0f).reduced(4, 0),
                       juce::Justification::centredLeft);
        }
    }
}

int DynamicsSnapWindow::findPointAtPosition(const juce::Point<float>& pos, float tolerance)
{
    auto* func = getActiveFunction();
    if (func == nullptr)
        return -1;

    auto normalized = screenToNormalized(pos);
    return func->findClosestPoint(normalized.x, normalized.y, tolerance / getWidth());
}

void DynamicsSnapWindow::mouseDown(const juce::MouseEvent& event)
{
    if (showSettings) return;

    auto* func = getActiveFunction();
    if (func == nullptr)
        return;

    if (event.mods.isPopupMenu())
    {
        juce::PopupMenu menu;
        menu.addItem(1, "Copy Curve");
        menu.addItem(2, "Paste Curve", clipboardFilled != nullptr && *clipboardFilled);
        menu.addItem(3, "Reset Curve");
        menu.addItem(4, "Add X,Y Point");
        auto safeThis = juce::Component::SafePointer<DynamicsSnapWindow>(this);
        menu.showMenuAsync({}, [safeThis](int result)
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
                    self->clipboardMeta->source = ClipboardMeta::Dynamics;
                    self->clipboardMeta->curveIndex = self->activeCurveIndex;
                    self->clipboardMeta->dynMinDB = self->curveRanges[self->activeCurveIndex].minDB;
                    self->clipboardMeta->dynMaxDB = self->curveRanges[self->activeCurveIndex].maxDB;
                }
            }
            else if (result == 2 && activeFunc && self->clipboard)
            {
                activeFunc->copyFrom(*self->clipboard);
                // Apply zoom if source was also a Dynamics window
                if (self->clipboardMeta && self->clipboardMeta->source == ClipboardMeta::Dynamics)
                {
                    self->curveRanges[self->activeCurveIndex].minDB = self->clipboardMeta->dynMinDB;
                    self->curveRanges[self->activeCurveIndex].maxDB = self->clipboardMeta->dynMaxDB;
                    self->syncDisplayRanges();
                }
                self->repaint();
            }
            else if (result == 3 && activeFunc)
            {
                float defaultY = (self->activeCurveIndex == 1) ? 0.0f : 1.0f;
                activeFunc->reset(defaultY);
                self->repaint();
            }
            else if (result == 4 && activeFunc)
            {
                juce::String hintText = "Enter point as: freq_hz, value_dB\nExample: 440, -12";

                auto* aw = new juce::AlertWindow("Add X,Y Point", hintText, juce::AlertWindow::NoIcon);
                aw->addTextEditor("point", "", "X, Y:");
                aw->addButton("OK", 1);
                aw->addButton("Cancel", 0);

                auto safeInner = juce::Component::SafePointer<DynamicsSnapWindow>(self);
                aw->enterModalState(true, juce::ModalCallbackFunction::create([safeInner, aw](int r)
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

                                // Y in dB (-60 to +48)
                                if (yVal >= -60.0f && yVal <= 48.0f)
                                {
                                    float normY = dBToNormalizedY(yVal);
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

void DynamicsSnapWindow::mouseDrag(const juce::MouseEvent& event)
{
    if (showSettings) return;

    auto* func = getActiveFunction();
    if (func == nullptr)
        return;

    float distance = mouseDownPosition.getDistanceFrom(event.position);
    if (distance > 3.0f)
        hasDraggedSignificantly = true;

    if (draggedPointIndex >= 0 && hasDraggedSignificantly)
    {
        auto normalized = screenToNormalized(event.position);
        normalized.x = juce::jlimit(0.0f, 1.0f, normalized.x);

        // Clamp Y to visible display range (allows zoom to act as range limiter)
        auto& range = getActiveRange();
        float minNormY = dBToNormalizedY(range.minDB);
        float maxNormY = dBToNormalizedY(range.maxDB);
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
        if (newIndex >= 0)
            draggedPointIndex = newIndex;
    }

    hoverPosition = event.position;
    isHovering = true;
    repaint();
}

void DynamicsSnapWindow::mouseUp(const juce::MouseEvent& event)
{
    if (showSettings) return;

    auto* func = getActiveFunction();
    if (func == nullptr)
        return;

    if (event.mods.isPopupMenu())
        return;

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
                auto& range = getActiveRange();
                float minNormY = dBToNormalizedY(range.minDB);
                float maxNormY = dBToNormalizedY(range.maxDB);
                normalized.y = juce::jlimit(minNormY, maxNormY, normalized.y);
                func->addPoint(normalized.x, normalized.y);
                repaint();
            }
        }
    }

    draggedPointIndex = -1;
    hasDraggedSignificantly = false;
}

void DynamicsSnapWindow::mouseMove(const juce::MouseEvent& event)
{
    if (showSettings) return;
    hoverPosition = event.position;
    isHovering = true;
    repaint();
}

void DynamicsSnapWindow::mouseExit(const juce::MouseEvent&)
{
    isHovering = false;
    draggedPointIndex = -1;
    hasDraggedSignificantly = false;
    repaint();
}

void DynamicsSnapWindow::resized()
{
    auto bounds = getLocalBounds();

    // Reserve top strip for controls (above the plot area)
    auto controlStrip = bounds.removeFromTop(20);
    int controlWidth = 70;
    int controlHeight = 16;

    // Curve selector on the right of the control strip
    curveSelector.setBounds(controlStrip.getRight() - controlWidth - 2,
                            controlStrip.getY() + 2,
                            controlWidth, controlHeight);

    // Settings button next to it
    settingsButton.setBounds(controlStrip.getRight() - controlWidth * 2 - 6,
                             controlStrip.getY() + 2,
                             controlWidth, controlHeight);

    // Plot bounds from remaining area (with margins for axis labels/hover)
    plotBounds = bounds.toFloat().reduced(24.0f, 10.0f);

    // Settings pane layout (compact, precision slider to the right)
    {
        auto settingsArea = plotBounds.toNearestInt().reduced(6, 4);

        // Table on the left side (compact to fit within plot area)
        int labelColWidth = 45;
        int editorColWidth = 40;
        int rowHeight = 15;
        int headerHeight = 13;
        int gap = 1;
        int startX = settingsArea.getX() + labelColWidth;
        int startY = settingsArea.getY() + 2;

        rangeColLabels[0].setBounds(startX, startY, editorColWidth, headerHeight);
        rangeColLabels[1].setBounds(startX + editorColWidth + gap, startY, editorColWidth, headerHeight);

        startY += headerHeight + gap;

        for (int row = 0; row < 3; ++row)
        {
            int y = startY + row * (rowHeight + gap);
            rangeRowLabels[row].setBounds(settingsArea.getX(), y, labelColWidth - 2, rowHeight);
            rangeEditors[row][0].setBounds(startX, y, editorColWidth, rowHeight);
            rangeEditors[row][1].setBounds(startX + editorColWidth + gap, y, editorColWidth, rowHeight);
        }

        // Precision slider to the right of the table (horizontal)
        int tableRightX = startX + editorColWidth * 2 + gap + 10;
        int sliderW = settingsArea.getRight() - tableRightX - 2;
        if (sliderW > 20)
        {
            int sliderH = 16;
            // Center vertically in settings area, offset down for label
            int sliderY = settingsArea.getY() + settingsArea.getHeight() / 2;
            precisionSlider.setBounds(tableRightX, sliderY,
                                      sliderW, sliderH);
        }
    }
}
