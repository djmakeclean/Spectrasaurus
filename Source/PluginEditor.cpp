#include "PluginProcessor.h"
#include "PluginEditor.h"

SpectrasaurusAudioProcessorEditor::SpectrasaurusAudioProcessorEditor (SpectrasaurusAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    setSize (1100, 960);

    // Setup bank buttons (used for hit detection; painting done in paint())
    for (int i = 0; i < 4; ++i)
    {
        bankButtons[i].setButtonText(juce::String::charToString('A' + i));
        bankButtons[i].setClickingTogglesState(true);
        bankButtons[i].setRadioGroupId(1001);
        bankButtons[i].onClick = [this, i] { selectBank(i); };
        bankButtons[i].setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
        bankButtons[i].setColour(juce::TextButton::buttonOnColourId, juce::Colours::transparentBlack);
        bankButtons[i].setColour(juce::TextButton::textColourOffId, juce::Colours::grey);
        bankButtons[i].setColour(juce::TextButton::textColourOnId, juce::Colours::white);
        addAndMakeVisible(bankButtons[i]);
    }
    bankButtons[0].setToggleState(true, juce::dontSendNotification);

    // Register mouse listener on bank buttons for right-click context menu
    for (int i = 0; i < 4; ++i)
        bankButtons[i].addMouseListener(this, false);

    // Setup snap windows
    snapDelayL.setLabel("Delay L");
    snapDelayL.setType(SnapWindowType::Delay);
    snapDelayL.setClipboard(&curveClipboard, &clipboardFilled, &clipboardMeta);
    addAndMakeVisible(snapDelayL);

    snapDelayR.setLabel("Delay R");
    snapDelayR.setType(SnapWindowType::Delay);
    snapDelayR.setClipboard(&curveClipboard, &clipboardFilled, &clipboardMeta);
    addAndMakeVisible(snapDelayR);

    snapPanL.setLabel("L -> R");
    snapPanL.setType(SnapWindowType::Pan);
    snapPanL.setClipboard(&curveClipboard, &clipboardFilled, &clipboardMeta);
    addAndMakeVisible(snapPanL);

    snapPanR.setLabel("R -> L");
    snapPanR.setType(SnapWindowType::Pan);
    snapPanR.setClipboard(&curveClipboard, &clipboardFilled, &clipboardMeta);
    addAndMakeVisible(snapPanR);

    snapFeedbackL.setLabel("Feedback L");
    snapFeedbackL.setType(SnapWindowType::Feedback);
    snapFeedbackL.setClipboard(&curveClipboard, &clipboardFilled, &clipboardMeta);
    addAndMakeVisible(snapFeedbackL);

    snapFeedbackR.setLabel("Feedback R");
    snapFeedbackR.setType(SnapWindowType::Feedback);
    snapFeedbackR.setClipboard(&curveClipboard, &clipboardFilled, &clipboardMeta);
    addAndMakeVisible(snapFeedbackR);

    // Setup dynamics snap windows (controls are internal to DynamicsSnapWindow)
    dynamicsL.setLabel("Dynamics L");
    dynamicsL.setClipboard(&curveClipboard, &clipboardFilled, &clipboardMeta);
    dynamicsL.onPrecisionChanged = [this] {
        bool enable = (dynamicsL.getPrecision() > 0.0f || dynamicsR.getPrecision() > 0.0f);
        audioProcessor.spectrographEnabled.store(enable);
    };
    addAndMakeVisible(dynamicsL);

    dynamicsR.setLabel("Dynamics R");
    dynamicsR.setClipboard(&curveClipboard, &clipboardFilled, &clipboardMeta);
    dynamicsR.onPrecisionChanged = [this] {
        bool enable = (dynamicsL.getPrecision() > 0.0f || dynamicsR.getPrecision() > 0.0f);
        audioProcessor.spectrographEnabled.store(enable);
    };
    addAndMakeVisible(dynamicsR);

    // Setup shift snap windows
    shiftL.setLabel("Pitch L");
    shiftL.setClipboard(&curveClipboard, &clipboardFilled, &clipboardMeta);
    shiftL.onSettingsChanged = [this] {
        // Only order button affects audio — ranges are display-only
    };
    addAndMakeVisible(shiftL);

    shiftR.setLabel("Pitch R");
    shiftR.setClipboard(&curveClipboard, &clipboardFilled, &clipboardMeta);
    shiftR.onSettingsChanged = [this] {
        // Only order button affects audio — ranges are display-only
    };
    addAndMakeVisible(shiftR);

    // Wire up dropdown selection persistence
    dynamicsL.onCurveSelectionChanged = [this](int idx) { audioProcessor.dynamicsLCurveIndex = idx; };
    dynamicsR.onCurveSelectionChanged = [this](int idx) { audioProcessor.dynamicsRCurveIndex = idx; };
    shiftL.onCurveSelectionChanged = [this](int idx) { audioProcessor.shiftLCurveIndex = idx; };
    shiftR.onCurveSelectionChanged = [this](int idx) { audioProcessor.shiftRCurveIndex = idx; };

    // Setup XY pad
    addAndMakeVisible(xyPad);
    xyPad.onValueChanged = [this](float x, float y)
    {
        morphXSlider.setValue(x, juce::sendNotificationAsync);
        morphYSlider.setValue(y, juce::sendNotificationAsync);
    };

    // Setup morph sliders for automation
    morphXSlider.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    morphXSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    morphXSlider.onValueChange = [this]
    {
        xyPad.setX(static_cast<float>(morphXSlider.getValue()));
    };
    addAndMakeVisible(morphXSlider);

    morphYSlider.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    morphYSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    morphYSlider.onValueChange = [this]
    {
        xyPad.setY(static_cast<float>(morphYSlider.getValue()));
    };
    addAndMakeVisible(morphYSlider);

    morphXLabel.setText("X", juce::dontSendNotification);
    morphXLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(morphXLabel);

    morphYLabel.setText("Y", juce::dontSendNotification);
    morphYLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(morphYLabel);

    // Create parameter attachments
    morphXAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.parameters, "morphX", morphXSlider);
    morphYAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.parameters, "morphY", morphYSlider);

    // Setup single gain slider (current bank)
    gainSlider.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    gainSlider.setRange(-40.0, 12.0, 0.1);
    gainSlider.setValue(0.0);
    gainSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 15);
    gainSlider.setTextValueSuffix(" dB");
    gainSlider.onValueChange = [this]()
    {
        audioProcessor.banks[selectedBank].gainDB = static_cast<float>(gainSlider.getValue());
    };
    addAndMakeVisible(gainSlider);

    gainLabel.setText("Gain", juce::dontSendNotification);
    gainLabel.setJustificationType(juce::Justification::centred);
    gainLabel.setFont(11.0f);
    addAndMakeVisible(gainLabel);

    // Setup single soft clip slider (current bank)
    softClipSlider.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    softClipSlider.setRange(-20.0, 0.0, 0.1);
    softClipSlider.setValue(0.0);
    softClipSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 15);
    softClipSlider.setTextValueSuffix(" dB");
    softClipSlider.onValueChange = [this]()
    {
        audioProcessor.banks[selectedBank].softClipThresholdDB = static_cast<float>(softClipSlider.getValue());
    };
    addAndMakeVisible(softClipSlider);

    softClipLabel.setText("Clip", juce::dontSendNotification);
    softClipLabel.setJustificationType(juce::Justification::centred);
    softClipLabel.setFont(11.0f);
    addAndMakeVisible(softClipLabel);

    // Setup per-bank pan slider
    panSlider.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    panSlider.setRange(-1.0, 1.0, 0.01);
    panSlider.setValue(0.0);
    panSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 15);
    panSlider.setDoubleClickReturnValue(true, 0.0);
    panSlider.onValueChange = [this]()
    {
        audioProcessor.banks[selectedBank].panValue = static_cast<float>(panSlider.getValue());
    };
    addAndMakeVisible(panSlider);

    panLabel.setText("Pan", juce::dontSendNotification);
    panLabel.setJustificationType(juce::Justification::centred);
    panLabel.setFont(11.0f);
    addAndMakeVisible(panLabel);

    // Setup master gain slider (global)
    masterGainSlider.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    masterGainSlider.setRange(-40.0, 12.0, 0.1);
    masterGainSlider.setValue(0.0);
    masterGainSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 15);
    masterGainSlider.setTextValueSuffix(" dB");
    masterGainSlider.onValueChange = [this]()
    {
        audioProcessor.masterGainDB.store(static_cast<float>(masterGainSlider.getValue()));
    };
    addAndMakeVisible(masterGainSlider);

    masterGainLabel.setText("Gain", juce::dontSendNotification);
    masterGainLabel.setJustificationType(juce::Justification::centred);
    masterGainLabel.setFont(11.0f);
    addAndMakeVisible(masterGainLabel);

    // Setup master clip slider (global)
    masterClipSlider.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    masterClipSlider.setRange(-20.0, 0.0, 0.1);
    masterClipSlider.setValue(0.0);
    masterClipSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 15);
    masterClipSlider.setTextValueSuffix(" dB");
    masterClipSlider.onValueChange = [this]()
    {
        audioProcessor.masterClipDB.store(static_cast<float>(masterClipSlider.getValue()));
    };
    addAndMakeVisible(masterClipSlider);

    masterClipLabel.setText("Clip", juce::dontSendNotification);
    masterClipLabel.setJustificationType(juce::Justification::centred);
    masterClipLabel.setFont(11.0f);
    addAndMakeVisible(masterClipLabel);

    // Setup master dry/wet slider (global)
    masterDryWetSlider.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    masterDryWetSlider.setRange(0.0, 100.0, 1.0);
    masterDryWetSlider.setValue(100.0);
    masterDryWetSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 15);
    masterDryWetSlider.setTextValueSuffix(" %");
    masterDryWetSlider.setDoubleClickReturnValue(true, 100.0);
    masterDryWetSlider.onValueChange = [this]()
    {
        audioProcessor.masterDryWet.store(static_cast<float>(masterDryWetSlider.getValue() / 100.0));
    };
    addAndMakeVisible(masterDryWetSlider);

    masterDryWetLabel.setText("Dry/Wet", juce::dontSendNotification);
    masterDryWetLabel.setJustificationType(juce::Justification::centred);
    masterDryWetLabel.setFont(11.0f);
    addAndMakeVisible(masterDryWetLabel);

    // Setup preset save/load buttons
    savePresetButton.setButtonText("Save");
    savePresetButton.onClick = [this] { savePreset(); };
    addAndMakeVisible(savePresetButton);

    loadPresetButton.setButtonText("Load");
    loadPresetButton.onClick = [this] { loadPreset(); };
    addAndMakeVisible(loadPresetButton);

    // Setup preset path label (read-only display of loaded file path)
    presetPathLabel.setText("", juce::dontSendNotification);
    presetPathLabel.setFont(juce::Font(10.0f));
    presetPathLabel.setColour(juce::Label::textColourId, juce::Colours::grey);
    presetPathLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(presetPathLabel);

    // Setup notes text editor
    notesEditor.setMultiLine(true);
    notesEditor.setReturnKeyStartsNewLine(true);
    notesEditor.setScrollbarsShown(true);
    notesEditor.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff1e1e1e));
    notesEditor.setColour(juce::TextEditor::outlineColourId, juce::Colour(0xff3e3e3e));
    notesEditor.setColour(juce::TextEditor::textColourId, juce::Colours::white);
    notesEditor.setFont(12.0f);
    notesEditor.setTextToShowWhenEmpty("Describe this patch...", juce::Colours::grey);
    notesEditor.onTextChange = [this]
    {
        audioProcessor.notesText = notesEditor.getText();
    };
    addAndMakeVisible(notesEditor);

    // Setup per-channel delay max time editors
    auto setupDelayMaxEditor = [this](juce::Label& caption, juce::TextEditor& editor,
                                       const char* captionText, auto setter)
    {
        caption.setText(captionText, juce::dontSendNotification);
        caption.setJustificationType(juce::Justification::centredRight);
        caption.setFont(10.0f);
        caption.setColour(juce::Label::textColourId, juce::Colours::grey);
        addAndMakeVisible(caption);

        editor.setJustification(juce::Justification::centred);
        editor.setInputRestrictions(8, "0123456789.");
        editor.setText("1000", false);
        editor.setFont(10.0f);
        editor.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff1e1e1e));
        editor.setColour(juce::TextEditor::outlineColourId, juce::Colour(0xff3e3e3e));
        editor.setColour(juce::TextEditor::textColourId, juce::Colours::white);
        editor.onReturnKey = [this, &editor, setter]
        {
            float val = editor.getText().getFloatValue();
            val = juce::jlimit(1.0f, 99000.0f, val);
            editor.setText(juce::String(val, 0), false);
            setter(val);
            audioProcessor.reallocateDelayBuffersIfNeeded();
            updateSnapWindows();
        };
        editor.onFocusLost = [this, &editor, setter]
        {
            float val = editor.getText().getFloatValue();
            val = juce::jlimit(1.0f, 99000.0f, val);
            editor.setText(juce::String(val, 0), false);
            setter(val);
            audioProcessor.reallocateDelayBuffersIfNeeded();
            updateSnapWindows();
        };
        addAndMakeVisible(editor);
    };

    setupDelayMaxEditor(delayMaxCaptionL, delayMaxEditorL, "max ms:",
        [this](float val) { audioProcessor.banks[selectedBank].delayMaxTimeMsL = val; });
    setupDelayMaxEditor(delayMaxCaptionR, delayMaxEditorR, "max ms:",
        [this](float val) { audioProcessor.banks[selectedBank].delayMaxTimeMsR = val; });

    // Setup per-channel delay log scale toggles
    auto setupLogScaleButton = [this](juce::TextButton& btn, auto setter)
    {
        btn.setButtonText("Linear");
        btn.setClickingTogglesState(true);
        btn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff2a2a2a));
        btn.onClick = [this, &btn, setter]
        {
            bool logScale = btn.getToggleState();
            setter(logScale);
            btn.setButtonText(logScale ? "Log" : "Linear");
            updateSnapWindows();
        };
        addAndMakeVisible(btn);
    };
    setupLogScaleButton(delayLogScaleButtonL,
        [this](bool v) { audioProcessor.banks[selectedBank].delayLogScaleL = v; });
    setupLogScaleButton(delayLogScaleButtonR,
        [this](bool v) { audioProcessor.banks[selectedBank].delayLogScaleR = v; });

    // Enable spectrograph by default (precision defaults to 0.15)
    audioProcessor.spectrographEnabled.store(true);

    // Restore master controls from processor state
    masterGainSlider.setValue(audioProcessor.masterGainDB.load(), juce::dontSendNotification);
    masterClipSlider.setValue(audioProcessor.masterClipDB.load(), juce::dontSendNotification);
    masterDryWetSlider.setValue(audioProcessor.masterDryWet.load() * 100.0, juce::dontSendNotification);

    // Restore notes text
    notesEditor.setText(audioProcessor.notesText, false);

    // Start timer for level metering (30 Hz)
    startTimerHz(30);

    // Restore dropdown selections from processor state
    dynamicsL.setActiveCurve(juce::jlimit(0, 2, audioProcessor.dynamicsLCurveIndex));
    dynamicsR.setActiveCurve(juce::jlimit(0, 2, audioProcessor.dynamicsRCurveIndex));
    shiftL.setActiveCurve(juce::jlimit(0, 1, audioProcessor.shiftLCurveIndex));
    shiftR.setActiveCurve(juce::jlimit(0, 1, audioProcessor.shiftRCurveIndex));

    // Restore zoom ranges from processor state
    for (int c = 0; c < 3; ++c)
    {
        dynamicsL.curveRanges[c] = { audioProcessor.dynamicsLZoom[c].minDB, audioProcessor.dynamicsLZoom[c].maxDB };
        dynamicsR.curveRanges[c] = { audioProcessor.dynamicsRZoom[c].minDB, audioProcessor.dynamicsRZoom[c].maxDB };
    }
    dynamicsL.syncDisplayRanges();
    dynamicsR.syncDisplayRanges();
    shiftL.shiftRange = { audioProcessor.shiftLZoom.minHz, audioProcessor.shiftLZoom.maxHz };
    shiftR.shiftRange = { audioProcessor.shiftRZoom.minHz, audioProcessor.shiftRZoom.maxHz };
    shiftL.multRange = { audioProcessor.multLZoom.minMult, audioProcessor.multLZoom.maxMult };
    shiftR.multRange = { audioProcessor.multRZoom.minMult, audioProcessor.multRZoom.maxMult };
    shiftL.syncSettings();
    shiftR.syncSettings();

    // Restore the last active bank (persisted in processor state)
    int restoredBank = juce::jlimit(0, 3, audioProcessor.activeBankIndex.load());
    bankButtons[restoredBank].setToggleState(true, juce::dontSendNotification);
    selectBank(restoredBank);
}

SpectrasaurusAudioProcessorEditor::~SpectrasaurusAudioProcessorEditor()
{
    stopTimer();

    // Persist dropdown selections back to processor
    audioProcessor.dynamicsLCurveIndex = dynamicsL.getActiveCurve();
    audioProcessor.dynamicsRCurveIndex = dynamicsR.getActiveCurve();
    audioProcessor.shiftLCurveIndex = shiftL.getActiveCurve();
    audioProcessor.shiftRCurveIndex = shiftR.getActiveCurve();

    // Persist zoom ranges back to processor
    for (int c = 0; c < 3; ++c)
    {
        audioProcessor.dynamicsLZoom[c] = { dynamicsL.curveRanges[c].minDB, dynamicsL.curveRanges[c].maxDB };
        audioProcessor.dynamicsRZoom[c] = { dynamicsR.curveRanges[c].minDB, dynamicsR.curveRanges[c].maxDB };
    }
    audioProcessor.shiftLZoom = { shiftL.shiftRange.minHz, shiftL.shiftRange.maxHz };
    audioProcessor.shiftRZoom = { shiftR.shiftRange.minHz, shiftR.shiftRange.maxHz };
    audioProcessor.multLZoom = { shiftL.multRange.minMult, shiftL.multRange.maxMult };
    audioProcessor.multRZoom = { shiftR.multRange.minMult, shiftR.multRange.maxMult };
}

void SpectrasaurusAudioProcessorEditor::timerCallback()
{
    // Update meter levels from processor
    meterLevelL = audioProcessor.outputLevelL.load();
    meterLevelR = audioProcessor.outputLevelR.load();
    repaint(meterAreaL);
    repaint(meterAreaR);

    // Update spectrograph data for dynamics windows
    if (audioProcessor.spectrographEnabled.load())
    {
        juce::SpinLock::ScopedLockType lock(audioProcessor.spectrographLock);
        int numBins = audioProcessor.spectrographNumBins;
        if (numBins > 0)
        {
            dynamicsL.updateSpectrograph(audioProcessor.spectrographDataL, numBins);
            dynamicsR.updateSpectrograph(audioProcessor.spectrographDataR, numBins);
        }
    }
    dynamicsL.repaint();
    dynamicsR.repaint();
}

void SpectrasaurusAudioProcessorEditor::selectBank(int bankIndex)
{
    selectedBank = bankIndex;
    audioProcessor.activeBankIndex = bankIndex;
    updateSnapWindows();
}

void SpectrasaurusAudioProcessorEditor::updateSnapWindows()
{
    auto& bank = audioProcessor.banks[selectedBank];

    snapDelayL.setFunction(&bank.delayL);
    snapDelayL.setDelayMax(bank.delayMaxTimeMsL);
    snapDelayL.setDelayLogScale(bank.delayLogScaleL);

    snapDelayR.setFunction(&bank.delayR);
    snapDelayR.setDelayMax(bank.delayMaxTimeMsR);
    snapDelayR.setDelayLogScale(bank.delayLogScaleR);

    snapPanL.setFunction(&bank.panL);
    snapPanR.setFunction(&bank.panR);

    snapFeedbackL.setFunction(&bank.feedbackL);
    snapFeedbackR.setFunction(&bank.feedbackR);

    // Set dynamics curves
    dynamicsL.setCurves(&bank.preGainL, &bank.minGateL, &bank.maxClipL);
    dynamicsR.setCurves(&bank.preGainR, &bank.minGateR, &bank.maxClipR);
    dynamicsL.setSampleRate(static_cast<float>(audioProcessor.getSampleRate()));
    dynamicsR.setSampleRate(static_cast<float>(audioProcessor.getSampleRate()));

    // Set shift/multiply curves (ranges are display-only, not stored in bank)
    shiftL.setCurves(&bank.shiftL, &bank.multiplyL);
    shiftR.setCurves(&bank.shiftR, &bank.multiplyR);
    shiftL.setSampleRate(static_cast<float>(audioProcessor.getSampleRate()));
    shiftR.setSampleRate(static_cast<float>(audioProcessor.getSampleRate()));
    shiftL.shiftBeforeMultiply = &bank.shiftBeforeMultiply;
    shiftR.shiftBeforeMultiply = &bank.shiftBeforeMultiply;
    shiftL.syncSettings();
    shiftR.syncSettings();

    // Sync gain/clip/pan to current bank
    gainSlider.setValue(bank.gainDB, juce::dontSendNotification);
    softClipSlider.setValue(bank.softClipThresholdDB, juce::dontSendNotification);
    panSlider.setValue(bank.panValue, juce::dontSendNotification);

    // Sync per-channel delay max time and log scale toggle
    delayMaxEditorL.setText(juce::String(bank.delayMaxTimeMsL, 0), false);
    delayMaxEditorR.setText(juce::String(bank.delayMaxTimeMsR, 0), false);
    delayLogScaleButtonL.setToggleState(bank.delayLogScaleL, juce::dontSendNotification);
    delayLogScaleButtonL.setButtonText(bank.delayLogScaleL ? "Log" : "Linear");
    delayLogScaleButtonR.setToggleState(bank.delayLogScaleR, juce::dontSendNotification);
    delayLogScaleButtonR.setButtonText(bank.delayLogScaleR ? "Log" : "Linear");

    repaint();
}

void SpectrasaurusAudioProcessorEditor::mouseDown(const juce::MouseEvent& event)
{
    for (int i = 0; i < 4; ++i)
    {
        if (event.eventComponent == &bankButtons[i] && event.mods.isPopupMenu())
        {
            showBankContextMenu(i);
            return;
        }
    }
}

void SpectrasaurusAudioProcessorEditor::showBankContextMenu(int bankIndex)
{
    juce::PopupMenu menu;
    menu.addItem(1, "Copy Bank");
    menu.addItem(2, "Paste Bank", bankClipboardFilled);
    menu.addItem(5, "Reset Bank");
    menu.addSeparator();
    menu.addItem(3, "Copy L -> R");
    menu.addItem(4, "Copy R -> L");

    auto safeThis = juce::Component::SafePointer<SpectrasaurusAudioProcessorEditor>(this);
    menu.showMenuAsync({}, [safeThis, bankIndex](int result)
    {
        if (safeThis == nullptr) return;
        auto* self = safeThis.getComponent();
        auto& bank = self->audioProcessor.banks[bankIndex];

        if (result == 1)
        {
            // Copy Bank (curves + view state)
            {
                juce::SpinLock::ScopedLockType lock(self->audioProcessor.bankLock);
                self->bankClipboard = bank;
            }
            // Capture view state (which curve is shown + zoom ranges)
            auto& vs = self->bankViewClipboard;
            vs.dynamicsLCurveIndex = self->dynamicsL.getActiveCurve();
            vs.dynamicsRCurveIndex = self->dynamicsR.getActiveCurve();
            for (int c = 0; c < 3; ++c)
            {
                vs.dynamicsLRanges[c] = self->dynamicsL.curveRanges[c];
                vs.dynamicsRRanges[c] = self->dynamicsR.curveRanges[c];
            }
            vs.shiftLCurveIndex = self->shiftL.getActiveCurve();
            vs.shiftRCurveIndex = self->shiftR.getActiveCurve();
            vs.shiftLRange = self->shiftL.shiftRange;
            vs.shiftRRange = self->shiftR.shiftRange;
            vs.multLRange = self->shiftL.multRange;
            vs.multRRange = self->shiftR.multRange;
            self->bankClipboardFilled = true;
        }
        else if (result == 2 && self->bankClipboardFilled)
        {
            // Paste Bank (curves + view state)
            {
                juce::SpinLock::ScopedLockType lock(self->audioProcessor.bankLock);
                bank = self->bankClipboard;
            }
            // Restore view state
            auto& vs = self->bankViewClipboard;
            self->dynamicsL.setActiveCurve(vs.dynamicsLCurveIndex);
            self->dynamicsR.setActiveCurve(vs.dynamicsRCurveIndex);
            for (int c = 0; c < 3; ++c)
            {
                self->dynamicsL.curveRanges[c] = vs.dynamicsLRanges[c];
                self->dynamicsR.curveRanges[c] = vs.dynamicsRRanges[c];
            }
            self->dynamicsL.syncDisplayRanges();
            self->dynamicsR.syncDisplayRanges();
            self->shiftL.setActiveCurve(vs.shiftLCurveIndex);
            self->shiftR.setActiveCurve(vs.shiftRCurveIndex);
            self->shiftL.shiftRange = vs.shiftLRange;
            self->shiftR.shiftRange = vs.shiftRRange;
            self->shiftL.multRange = vs.multLRange;
            self->shiftR.multRange = vs.multRRange;
            self->shiftL.syncSettings();
            self->shiftR.syncSettings();
            if (bankIndex == self->selectedBank)
                self->updateSnapWindows();
        }
        else if (result == 3)
        {
            // Copy L -> R (curves + settings + view state)
            {
                juce::SpinLock::ScopedLockType lock(self->audioProcessor.bankLock);
                bank.delayR.copyFrom(bank.delayL);
                bank.panR.copyFrom(bank.panL);
                bank.feedbackR.copyFrom(bank.feedbackL);
                bank.preGainR.copyFrom(bank.preGainL);
                bank.minGateR.copyFrom(bank.minGateL);
                bank.maxClipR.copyFrom(bank.maxClipL);
                bank.shiftR.copyFrom(bank.shiftL);
                bank.multiplyR.copyFrom(bank.multiplyL);
                bank.delayMaxTimeMsR = bank.delayMaxTimeMsL;
                bank.delayLogScaleR = bank.delayLogScaleL;
            }

            // Copy view state L -> R
            self->dynamicsR.setActiveCurve(self->dynamicsL.getActiveCurve());
            for (int c = 0; c < 3; ++c)
                self->dynamicsR.curveRanges[c] = self->dynamicsL.curveRanges[c];
            self->dynamicsR.syncDisplayRanges();
            self->shiftR.setActiveCurve(self->shiftL.getActiveCurve());
            self->shiftR.shiftRange = self->shiftL.shiftRange;
            self->shiftR.multRange = self->shiftL.multRange;
            self->shiftR.syncSettings();

            if (bankIndex == self->selectedBank)
                self->updateSnapWindows();
        }
        else if (result == 4)
        {
            // Copy R -> L (curves + settings + view state)
            {
                juce::SpinLock::ScopedLockType lock(self->audioProcessor.bankLock);
                bank.delayL.copyFrom(bank.delayR);
                bank.panL.copyFrom(bank.panR);
                bank.feedbackL.copyFrom(bank.feedbackR);
                bank.preGainL.copyFrom(bank.preGainR);
                bank.minGateL.copyFrom(bank.minGateR);
                bank.maxClipL.copyFrom(bank.maxClipR);
                bank.shiftL.copyFrom(bank.shiftR);
                bank.multiplyL.copyFrom(bank.multiplyR);
                bank.delayMaxTimeMsL = bank.delayMaxTimeMsR;
                bank.delayLogScaleL = bank.delayLogScaleR;
            }

            // Copy view state R -> L
            self->dynamicsL.setActiveCurve(self->dynamicsR.getActiveCurve());
            for (int c = 0; c < 3; ++c)
                self->dynamicsL.curveRanges[c] = self->dynamicsR.curveRanges[c];
            self->dynamicsL.syncDisplayRanges();
            self->shiftL.setActiveCurve(self->shiftR.getActiveCurve());
            self->shiftL.shiftRange = self->shiftR.shiftRange;
            self->shiftL.multRange = self->shiftR.multRange;
            self->shiftL.syncSettings();

            if (bankIndex == self->selectedBank)
                self->updateSnapWindows();
        }
        else if (result == 5)
        {
            // Reset Bank (under lock)
            {
                juce::SpinLock::ScopedLockType lock(self->audioProcessor.bankLock);
                bank.reset();
            }
            if (bankIndex == self->selectedBank)
                self->updateSnapWindows();
        }
    });
}

void SpectrasaurusAudioProcessorEditor::savePreset()
{
    fileChooser = std::make_unique<juce::FileChooser>(
        "Save Preset", juce::File(), "*.spectral");

    fileChooser->launchAsync(juce::FileBrowserComponent::saveMode |
                             juce::FileBrowserComponent::canSelectFiles,
        [this](const juce::FileChooser& fc)
        {
            auto file = fc.getResult();
            if (file == juce::File())
                return;

            // Ensure .spectral extension
            if (!file.hasFileExtension("spectral"))
                file = file.withFileExtension("spectral");

            auto* root = new juce::DynamicObject();
            root->setProperty("spectrasaurus_version", "1.0");
            root->setProperty("selectedBank", selectedBank);
            root->setProperty("morphX", static_cast<double>(audioProcessor.getMorphX()));
            root->setProperty("morphY", static_cast<double>(audioProcessor.getMorphY()));
            root->setProperty("masterGainDB", static_cast<double>(audioProcessor.masterGainDB.load()));
            root->setProperty("masterClipDB", static_cast<double>(audioProcessor.masterClipDB.load()));
            root->setProperty("masterDryWet", static_cast<double>(audioProcessor.masterDryWet.load()));

            // Dropdown selections
            root->setProperty("dynamicsLCurveIndex", dynamicsL.getActiveCurve());
            root->setProperty("dynamicsRCurveIndex", dynamicsR.getActiveCurve());
            root->setProperty("shiftLCurveIndex", shiftL.getActiveCurve());
            root->setProperty("shiftRCurveIndex", shiftR.getActiveCurve());

            // Zoom ranges
            auto saveZoom = [&](const char* prefix, const DynamicsSnapWindow& dyn,
                                const ShiftSnapWindow& sh)
            {
                for (int c = 0; c < 3; ++c)
                {
                    root->setProperty(juce::String(prefix) + "DynZoomMin" + juce::String(c),
                                      static_cast<double>(dyn.curveRanges[c].minDB));
                    root->setProperty(juce::String(prefix) + "DynZoomMax" + juce::String(c),
                                      static_cast<double>(dyn.curveRanges[c].maxDB));
                }
                root->setProperty(juce::String(prefix) + "ShiftZoomMin", static_cast<double>(sh.shiftRange.minHz));
                root->setProperty(juce::String(prefix) + "ShiftZoomMax", static_cast<double>(sh.shiftRange.maxHz));
                root->setProperty(juce::String(prefix) + "MultZoomMin", static_cast<double>(sh.multRange.minMult));
                root->setProperty(juce::String(prefix) + "MultZoomMax", static_cast<double>(sh.multRange.maxMult));
            };
            saveZoom("L", dynamicsL, shiftL);
            saveZoom("R", dynamicsR, shiftR);

            juce::Array<juce::var> banksArray;
            for (int i = 0; i < 4; ++i)
                banksArray.add(audioProcessor.banks[i].toVar());
            root->setProperty("banks", juce::var(banksArray));
            root->setProperty("notesText", audioProcessor.notesText);

            auto json = juce::JSON::toString(juce::var(root));
            file.replaceWithText(json);

            // Track saved preset path
            currentPresetPath = file.getFullPathName();
            presetPathLabel.setText(".../" + juce::File(currentPresetPath).getParentDirectory().getParentDirectory().getFileName()
                + "/" + juce::File(currentPresetPath).getParentDirectory().getFileName()
                + "/" + juce::File(currentPresetPath).getFileName(), juce::dontSendNotification);
        });
}

void SpectrasaurusAudioProcessorEditor::loadPreset()
{
    fileChooser = std::make_unique<juce::FileChooser>(
        "Load Preset", juce::File(), "*.spectral");

    fileChooser->launchAsync(juce::FileBrowserComponent::openMode |
                             juce::FileBrowserComponent::canSelectFiles,
        [this](const juce::FileChooser& fc)
        {
            auto file = fc.getResult();
            if (file == juce::File() || !file.existsAsFile())
                return;

            auto json = file.loadFileAsString();
            auto parsed = juce::JSON::parse(json);

            if (auto* root = parsed.getDynamicObject())
            {
                auto banksVar = root->getProperty("banks");
                if (auto* banksArray = banksVar.getArray())
                {
                    juce::SpinLock::ScopedLockType lock(audioProcessor.bankLock);
                    int count = std::min(static_cast<int>(banksArray->size()), 4);
                    for (int i = 0; i < count; ++i)
                        audioProcessor.banks[i].fromVar((*banksArray)[i]);
                }

                // Restore master controls
                if (root->hasProperty("masterGainDB"))
                {
                    float g = static_cast<float>(static_cast<double>(root->getProperty("masterGainDB")));
                    audioProcessor.masterGainDB.store(g);
                    masterGainSlider.setValue(g, juce::dontSendNotification);
                }
                if (root->hasProperty("masterClipDB"))
                {
                    float c = static_cast<float>(static_cast<double>(root->getProperty("masterClipDB")));
                    audioProcessor.masterClipDB.store(c);
                    masterClipSlider.setValue(c, juce::dontSendNotification);
                }
                if (root->hasProperty("masterDryWet"))
                {
                    float dw = static_cast<float>(static_cast<double>(root->getProperty("masterDryWet")));
                    audioProcessor.masterDryWet.store(dw);
                    masterDryWetSlider.setValue(dw * 100.0, juce::dontSendNotification);
                }

                // Restore notes
                if (root->hasProperty("notesText"))
                {
                    audioProcessor.notesText = root->getProperty("notesText").toString();
                    notesEditor.setText(audioProcessor.notesText, false);
                }

                // Restore morph XY position
                if (root->hasProperty("morphX"))
                {
                    float mx = static_cast<float>(static_cast<double>(root->getProperty("morphX")));
                    if (auto* param = audioProcessor.parameters.getParameter("morphX"))
                        param->setValueNotifyingHost(mx);
                }
                if (root->hasProperty("morphY"))
                {
                    float my = static_cast<float>(static_cast<double>(root->getProperty("morphY")));
                    if (auto* param = audioProcessor.parameters.getParameter("morphY"))
                        param->setValueNotifyingHost(my);
                }

                // Restore dropdown selections
                if (root->hasProperty("dynamicsLCurveIndex"))
                    dynamicsL.setActiveCurve(juce::jlimit(0, 2, static_cast<int>(root->getProperty("dynamicsLCurveIndex"))));
                if (root->hasProperty("dynamicsRCurveIndex"))
                    dynamicsR.setActiveCurve(juce::jlimit(0, 2, static_cast<int>(root->getProperty("dynamicsRCurveIndex"))));
                if (root->hasProperty("shiftLCurveIndex"))
                    shiftL.setActiveCurve(juce::jlimit(0, 1, static_cast<int>(root->getProperty("shiftLCurveIndex"))));
                if (root->hasProperty("shiftRCurveIndex"))
                    shiftR.setActiveCurve(juce::jlimit(0, 1, static_cast<int>(root->getProperty("shiftRCurveIndex"))));

                // Restore zoom ranges (backward compatible)
                auto loadZoom = [&](const char* prefix, DynamicsSnapWindow& dyn,
                                    ShiftSnapWindow& sh)
                {
                    for (int c = 0; c < 3; ++c)
                    {
                        auto minKey = juce::String(prefix) + "DynZoomMin" + juce::String(c);
                        auto maxKey = juce::String(prefix) + "DynZoomMax" + juce::String(c);
                        if (root->hasProperty(minKey))
                            dyn.curveRanges[c].minDB = static_cast<float>(static_cast<double>(root->getProperty(minKey)));
                        if (root->hasProperty(maxKey))
                            dyn.curveRanges[c].maxDB = static_cast<float>(static_cast<double>(root->getProperty(maxKey)));
                    }
                    dyn.syncDisplayRanges();
                    auto shMinKey = juce::String(prefix) + "ShiftZoomMin";
                    auto shMaxKey = juce::String(prefix) + "ShiftZoomMax";
                    if (root->hasProperty(shMinKey))
                        sh.shiftRange.minHz = static_cast<float>(static_cast<double>(root->getProperty(shMinKey)));
                    if (root->hasProperty(shMaxKey))
                        sh.shiftRange.maxHz = static_cast<float>(static_cast<double>(root->getProperty(shMaxKey)));
                    auto mMinKey = juce::String(prefix) + "MultZoomMin";
                    auto mMaxKey = juce::String(prefix) + "MultZoomMax";
                    if (root->hasProperty(mMinKey))
                        sh.multRange.minMult = static_cast<float>(static_cast<double>(root->getProperty(mMinKey)));
                    if (root->hasProperty(mMaxKey))
                        sh.multRange.maxMult = static_cast<float>(static_cast<double>(root->getProperty(mMaxKey)));
                    sh.syncSettings();
                };
                loadZoom("L", dynamicsL, shiftL);
                loadZoom("R", dynamicsR, shiftR);

                // Restore selected bank
                if (root->hasProperty("selectedBank"))
                {
                    int bank = juce::jlimit(0, 3, static_cast<int>(root->getProperty("selectedBank")));
                    bankButtons[bank].setToggleState(true, juce::dontSendNotification);
                    selectBank(bank);
                }
            }

            // Track loaded preset path
            currentPresetPath = file.getFullPathName();
            presetPathLabel.setText(".../" + juce::File(currentPresetPath).getParentDirectory().getParentDirectory().getFileName()
                + "/" + juce::File(currentPresetPath).getParentDirectory().getFileName()
                + "/" + juce::File(currentPresetPath).getFileName(), juce::dontSendNotification);

            updateSnapWindows();
        });
}

void SpectrasaurusAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff1a1a1a));

    // --- Right-side panels ---
    auto drawPanel = [&](juce::Rectangle<int> bounds, const juce::String& title)
    {
        g.setColour(juce::Colour(0xff242424));
        g.fillRoundedRectangle(bounds.toFloat(), 6.0f);
        g.setColour(juce::Colour(0xff3a3a3a));
        g.drawRoundedRectangle(bounds.toFloat(), 6.0f, 1.0f);

        g.setColour(juce::Colours::white.withAlpha(0.8f));
        g.setFont(juce::Font(13.0f, juce::Font::bold));
        g.drawText(title, bounds.getX() + 10, bounds.getY() + 4, bounds.getWidth() - 20, 18,
                   juce::Justification::centred);
    };

    // Website text above right panels
    g.setColour(juce::Colours::white.withAlpha(0.4f));
    g.setFont(juce::Font(10.0f));
    g.drawText("www.djmakeclean.com",
               bankMorphPanel.getX(), bankMorphPanel.getY() - 16,
               bankMorphPanel.getWidth(), 14,
               juce::Justification::centredRight);

    drawPanel(bankMorphPanel, "Bank Morph");
    drawPanel(masterPanel, "Master");
    drawPanel(notesPanel, "Notes");
    drawPanel(presetPanel, "Preset");

    // --- Chrome-tab bank selector ---
    const int tabHeight = 30;
    const int tabWidth = 60;
    const int tabStartX = 15;
    const int tabY = panelArea.getY() - tabHeight;
    const float cornerRadius = 6.0f;

    // Draw panel background
    g.setColour(juce::Colour(0xff2a2a2a));
    g.fillRoundedRectangle(panelArea.toFloat(), cornerRadius);

    // Draw tabs
    for (int i = 0; i < 4; ++i)
    {
        int tx = tabStartX + i * (tabWidth + 4);
        auto tabRect = juce::Rectangle<float>((float)tx, (float)tabY, (float)tabWidth, (float)tabHeight + cornerRadius);

        if (i == selectedBank)
        {
            g.setColour(juce::Colour(0xff2a2a2a));
            juce::Path tabPath;
            tabPath.addRoundedRectangle(tabRect.getX(), tabRect.getY(),
                                         tabRect.getWidth(), tabRect.getHeight(),
                                         cornerRadius, cornerRadius, true, true, false, false);
            g.fillPath(tabPath);
        }
        else
        {
            g.setColour(juce::Colour(0xff1e1e1e));
            juce::Path tabPath;
            tabPath.addRoundedRectangle(tabRect.getX(), tabRect.getY(),
                                         tabRect.getWidth(), tabRect.getHeight(),
                                         cornerRadius, cornerRadius, true, true, false, false);
            g.fillPath(tabPath);

            g.setColour(juce::Colour(0xff3a3a3a));
            g.strokePath(tabPath, juce::PathStrokeType(1.0f));
        }
    }

    // --- Level meters ---
    auto drawMeter = [&](juce::Rectangle<int> bounds, float level)
    {
        g.setColour(juce::Colour(0xff2a2a2a));
        g.fillRect(bounds);

        float levelDB = juce::Decibels::gainToDecibels(level);
        levelDB = std::clamp(levelDB, -60.0f, 0.0f);
        float normalizedLevel = (levelDB + 60.0f) / 60.0f;

        int barHeight = static_cast<int>(bounds.getHeight() * normalizedLevel);
        auto barBounds = bounds.removeFromBottom(barHeight);

        juce::Colour meterColor;
        if (levelDB > -6.0f)
            meterColor = juce::Colours::red;
        else if (levelDB > -12.0f)
            meterColor = juce::Colours::yellow;
        else
            meterColor = juce::Colour(0xff00ff00);

        g.setColour(meterColor);
        g.fillRect(barBounds);

        g.setColour(juce::Colours::grey);
        g.drawRect(bounds, 1);
    };

    drawMeter(meterAreaL, meterLevelL);
    drawMeter(meterAreaR, meterLevelR);

    // === Signal Flow Diagram ===

    // Arrow drawing helper (larger heads, thicker lines)
    auto drawArrow = [&g](float x1, float y1, float x2, float y2, float headSize = 7.0f)
    {
        g.drawLine(x1, y1, x2, y2, 1.5f);
        float angle = std::atan2(y2 - y1, x2 - x1);
        float ax = x2 - headSize * std::cos(angle - 0.4f);
        float ay = y2 - headSize * std::sin(angle - 0.4f);
        float bx = x2 - headSize * std::cos(angle + 0.4f);
        float by = y2 - headSize * std::sin(angle + 0.4f);
        juce::Path head;
        head.startNewSubPath(x2, y2);
        head.lineTo(ax, ay);
        head.lineTo(bx, by);
        head.closeSubPath();
        g.fillPath(head);
    };

    // Boxed label drawing helper
    auto drawBoxedLabel = [&g](float cx, float cy, const juce::String& text, float w = 50, float h = 18)
    {
        auto rect = juce::Rectangle<float>(cx - w / 2, cy - h / 2, w, h);
        g.setColour(juce::Colour(0xff333333));
        g.fillRoundedRectangle(rect, 3.0f);
        g.setColour(juce::Colour(0xff555555));
        g.drawRoundedRectangle(rect, 3.0f, 1.0f);
        g.setColour(juce::Colours::white.withAlpha(0.9f));
        g.setFont(juce::Font(13.0f, juce::Font::bold));
        g.drawText(text, rect.toNearestInt(), juce::Justification::centred);
    };

    // Get snap window bounds
    auto dLb = dynamicsL.getBounds().toFloat();
    auto dRb = dynamicsR.getBounds().toFloat();
    auto sLb = shiftL.getBounds().toFloat();
    auto sRb = shiftR.getBounds().toFloat();
    auto dlLb = snapDelayL.getBounds().toFloat();
    auto dlRb = snapDelayR.getBounds().toFloat();
    auto pLb = snapPanL.getBounds().toFloat();
    auto pRb = snapPanR.getBounds().toFloat();
    auto fLb = snapFeedbackL.getBounds().toFloat();
    auto fRb = snapFeedbackR.getBounds().toFloat();

    float leftCX = dLb.getCentreX();
    float rightCX = dRb.getCentreX();

    // "IN L" and "IN R" boxed labels with arrows
    drawBoxedLabel(leftCX, dLb.getY() - 14, "IN L");
    drawBoxedLabel(rightCX, dRb.getY() - 14, "IN R");
    g.setColour(juce::Colour(0xff707070));
    drawArrow(leftCX, dLb.getY() - 5, leftCX, dLb.getY());
    drawArrow(rightCX, dRb.getY() - 5, rightCX, dRb.getY());

    // Dynamics -> Shift
    g.setColour(juce::Colour(0xff606060));
    drawArrow(leftCX, dLb.getBottom(), leftCX, sLb.getY());
    drawArrow(rightCX, dRb.getBottom(), rightCX, sRb.getY());

    // Shift -> Delay
    drawArrow(leftCX, sLb.getBottom(), leftCX, dlLb.getY());
    drawArrow(rightCX, sRb.getBottom(), rightCX, dlRb.getY());

    // Delay -> Pan
    drawArrow(leftCX, dlLb.getBottom(), leftCX, pLb.getY());
    drawArrow(rightCX, dlRb.getBottom(), rightCX, pRb.getY());

    // Pan -> OUT area: cross-arrows showing stereo mixing
    float outMidY = (pLb.getBottom() + fLb.getY()) / 2.0f;

    // L->R contributes to OUT L (straight down) and OUT R (cross)
    g.setColour(juce::Colour(0xff707070));
    drawArrow(leftCX, pLb.getBottom() + 2, leftCX, outMidY - 10);
    g.setColour(juce::Colour(0xff505050));
    drawArrow(leftCX + 30, pLb.getBottom() + 2, rightCX - 30, outMidY - 10, 6.0f);

    // R->L contributes to OUT R (straight down) and OUT L (cross)
    g.setColour(juce::Colour(0xff707070));
    drawArrow(rightCX, pRb.getBottom() + 2, rightCX, outMidY - 10);
    g.setColour(juce::Colour(0xff505050));
    drawArrow(rightCX - 30, pRb.getBottom() + 2, leftCX + 30, outMidY - 10, 6.0f);

    // "OUT L" and "OUT R" boxed labels
    drawBoxedLabel(leftCX, outMidY, "OUT L");
    drawBoxedLabel(rightCX, outMidY, "OUT R");

    // OUT -> Feedback
    g.setColour(juce::Colour(0xff606060));
    drawArrow(leftCX, outMidY + 10, leftCX, fLb.getY());
    drawArrow(rightCX, outMidY + 10, rightCX, fRb.getY());

    // Feedback loop arrows (left side: goes left and back up to Dynamics)
    g.setColour(juce::Colour(0xff886644)); // warm color for feedback path
    float loopMarginL = panelArea.getX() + 8.0f;
    float fbLY = fLb.getCentreY();
    float dynEntryLY = dLb.getY() + dLb.getHeight() * 0.3f;

    g.drawLine(fLb.getX(), fbLY, loopMarginL, fbLY, 1.5f);
    g.drawLine(loopMarginL, fbLY, loopMarginL, dynEntryLY, 1.5f);
    drawArrow(loopMarginL, dynEntryLY, dLb.getX(), dynEntryLY, 6.0f);

    // Feedback loop (right side: goes right and back up to Dynamics)
    float loopMarginR = panelArea.getRight() - 8.0f;
    float fbRY = fRb.getCentreY();
    float dynEntryRY = dRb.getY() + dRb.getHeight() * 0.3f;

    g.drawLine(fRb.getRight(), fbRY, loopMarginR, fbRY, 1.5f);
    g.drawLine(loopMarginR, fbRY, loopMarginR, dynEntryRY, 1.5f);
    drawArrow(loopMarginR, dynEntryRY, dRb.getRight(), dynEntryRY, 6.0f);

    // "FB" labels on the feedback loop lines
    g.setFont(10.0f);
    g.drawText("FB", (int)(loopMarginL - 8), (int)((fbLY + dynEntryLY) / 2 - 6), 18, 12,
               juce::Justification::centred);
    g.drawText("FB", (int)(loopMarginR - 8), (int)((fbRY + dynEntryRY) / 2 - 6), 18, 12,
               juce::Justification::centred);

    // Final stereo OUT: centered box above gain/clip/pan knobs
    float gainLabelTopY = gainLabel.getBounds().toFloat().getY();
    float finalOutCX = panelArea.getCentreX();
    float finalOutY = gainLabelTopY - 18;

    drawBoxedLabel(finalOutCX, finalOutY, "OUT L  OUT R", 90, 18);

    // Single arrow from OUT box to gain/clip/pan area
    g.setColour(juce::Colour(0xff606060));
    drawArrow(finalOutCX, finalOutY + 10, finalOutCX, gainLabelTopY - 2, 5.0f);
}

void SpectrasaurusAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();

    const int tabHeight = 30;
    const int tabWidth = 60;
    const int tabStartX = 15;

    // Reserve space for tabs at top
    bounds.removeFromTop(tabHeight + 5);

    // Right side: 4 labeled panels (Bank Morph, Master, Notes, Preset)
    auto rightSide = bounds.removeFromRight(280);
    rightSide.removeFromTop(5);
    rightSide.removeFromBottom(5);

    const int panelGap = 8;
    const int panelTitleH = 20;
    const int panelPad = 8;
    int totalRightH = rightSide.getHeight();
    // Bank Morph gets a smaller share (just enough for XY pad + knobs), Master same as before
    int bankMorphHeight = (totalRightH - panelGap * 3) * 3 / 10;  // smaller than before
    int masterHeight = (totalRightH - panelGap * 3) * 2 / 5;

    // --- Panel 1: Bank Morph ---
    bankMorphPanel = rightSide.removeFromTop(bankMorphHeight);
    rightSide.removeFromTop(panelGap);

    {
        auto inner = bankMorphPanel.reduced(panelPad);
        inner.removeFromTop(panelTitleH); // space for title

        // XY pad — as large as space allows
        int xyPadSize = std::min(inner.getWidth() - 10, inner.getHeight() - 80);
        xyPadSize = std::min(xyPadSize, 200);
        auto xyPadBounds = inner.removeFromTop(xyPadSize).withSizeKeepingCentre(xyPadSize, xyPadSize);
        xyPad.setBounds(xyPadBounds);

        // Morph knobs below
        inner.removeFromTop(5);
        auto knobArea = inner;
        int knobSize = 50;
        int knobSpacing = (knobArea.getWidth() - knobSize * 2) / 3;

        auto xKnobArea = knobArea.removeFromLeft(knobSpacing + knobSize);
        morphXSlider.setBounds(xKnobArea.removeFromTop(knobSize).withSizeKeepingCentre(knobSize, knobSize));
        morphXLabel.setBounds(xKnobArea.removeFromTop(14));

        auto yKnobArea = knobArea.removeFromLeft(knobSize);
        morphYSlider.setBounds(yKnobArea.removeFromTop(knobSize).withSizeKeepingCentre(knobSize, knobSize));
        morphYLabel.setBounds(yKnobArea.removeFromTop(14));
    }

    // --- Panel 2: Master ---
    masterPanel = rightSide.removeFromTop(masterHeight);
    rightSide.removeFromTop(panelGap);

    {
        auto inner = masterPanel.reduced(panelPad);
        inner.removeFromTop(panelTitleH); // space for title

        // Level meters — fill most of the vertical space, leave room for knobs
        int knobH = 75;
        auto meterArea = inner.removeFromTop(inner.getHeight() - knobH);
        int meterWidth = 30;
        auto meterCenter = meterArea.withSizeKeepingCentre(meterWidth * 2 + 10, meterArea.getHeight());
        meterAreaL = meterCenter.removeFromLeft(meterWidth).reduced(2, 4);
        meterCenter.removeFromLeft(10);
        meterAreaR = meterCenter.removeFromLeft(meterWidth).reduced(2, 4);

        // Gain/Clip/DryWet knobs below meters
        inner.removeFromTop(5);
        auto masterKnobArea = inner;
        int masterKnobSize = 50;
        int masterKnobGap = 12;
        auto masterKnobCenter = masterKnobArea.withSizeKeepingCentre(masterKnobSize * 3 + masterKnobGap * 2, masterKnobArea.getHeight());

        auto masterGainArea = masterKnobCenter.removeFromLeft(masterKnobSize);
        masterGainLabel.setBounds(masterGainArea.removeFromTop(14));
        masterGainSlider.setBounds(masterGainArea);

        masterKnobCenter.removeFromLeft(masterKnobGap);

        auto masterClipArea = masterKnobCenter.removeFromLeft(masterKnobSize);
        masterClipLabel.setBounds(masterClipArea.removeFromTop(14));
        masterClipSlider.setBounds(masterClipArea);

        masterKnobCenter.removeFromLeft(masterKnobGap);

        auto masterDryWetArea = masterKnobCenter.removeFromLeft(masterKnobSize);
        masterDryWetLabel.setBounds(masterDryWetArea.removeFromTop(14));
        masterDryWetSlider.setBounds(masterDryWetArea);
    }

    // --- Panel 3: Notes ---
    // Split remaining space into notes (top half) and preset (bottom half)
    int remainingH = rightSide.getHeight();
    int notesPanelH = remainingH / 2;
    notesPanel = rightSide.removeFromTop(notesPanelH);
    rightSide.removeFromTop(panelGap);

    {
        auto inner = notesPanel.reduced(panelPad);
        inner.removeFromTop(panelTitleH); // space for title
        notesEditor.setBounds(inner);
    }

    // --- Panel 4: Preset ---
    presetPanel = rightSide;

    {
        auto inner = presetPanel.reduced(panelPad);
        inner.removeFromTop(panelTitleH); // space for title

        // Preset path label (read-only display of loaded file)
        presetPathLabel.setBounds(inner.removeFromTop(16));
        inner.removeFromTop(4);

        // Center Save/Load buttons vertically in remaining space
        auto btnArea = inner.withSizeKeepingCentre(160, 60);
        savePresetButton.setBounds(btnArea.removeFromTop(26));
        btnArea.removeFromTop(8);
        loadPresetButton.setBounds(btnArea.removeFromTop(26));
    }

    // Panel area: left side, from tab bottom to window bottom
    panelArea = bounds.reduced(10, 0);
    panelArea.removeFromBottom(5);

    // Position bank tab buttons (transparent, for hit detection only)
    for (int i = 0; i < 4; ++i)
    {
        int tx = tabStartX + i * (tabWidth + 4);
        bankButtons[i].setBounds(tx, panelArea.getY() - tabHeight, tabWidth, tabHeight);
    }

    // Inside the panel: snap windows in 5x2 grid with arrow gaps + gain/clip at bottom
    auto innerPanel = panelArea.reduced(10);

    // Reserve bottom for gain/clip knobs + final OUT label
    auto bottomControls = innerPanel.removeFromBottom(90);
    innerPanel.removeFromBottom(5); // gap

    // Layout constants for signal flow visualization
    const int inLabelH = 24;      // "IN L" / "IN R" text + arrow
    const int arrowGapH = 20;     // gaps between rows for arrows
    const int outAreaH = 100;     // "OUT L" / "OUT R" between pan and feedback (cross arrows)
    const int fbLoopH = 4;        // minimal gap below feedback

    int fixedH = inLabelH + arrowGapH * 3 + outAreaH + fbLoopH;
    int snapRowHeight = std::min((innerPanel.getHeight() - fixedH) / 5, 110);
    int gapBetweenWindows = 10;
    int sideMargin = 20;  // space for feedback loop arrows on each side

    // Generic layout helper for a pair of snap windows
    auto layoutSnapRow = [&](juce::Rectangle<int> rowBounds, juce::Component& snapL, juce::Component& snapR)
    {
        rowBounds = rowBounds.reduced(sideMargin, 0);
        int halfWidth = (rowBounds.getWidth() - gapBetweenWindows) / 2;

        auto leftHalf = rowBounds.removeFromLeft(halfWidth);
        rowBounds.removeFromLeft(gapBetweenWindows);
        auto rightHalf = rowBounds;

        snapL.setBounds(leftHalf);
        snapR.setBounds(rightHalf);
    };

    // "IN L" / "IN R" label area (painted in paint())
    innerPanel.removeFromTop(inLabelH);

    // Row 1: Dynamics
    layoutSnapRow(innerPanel.removeFromTop(snapRowHeight), dynamicsL, dynamicsR);
    innerPanel.removeFromTop(arrowGapH);

    // Row 2: Shift
    layoutSnapRow(innerPanel.removeFromTop(snapRowHeight), shiftL, shiftR);
    innerPanel.removeFromTop(arrowGapH);

    // Row 3: Delay
    layoutSnapRow(innerPanel.removeFromTop(snapRowHeight), snapDelayL, snapDelayR);
    innerPanel.removeFromTop(arrowGapH);

    // Overlay delay max editors in top-right control strip (can't overlap curve points)
    auto posDelayMax = [](const juce::Component& snap, juce::Label& caption, juce::TextEditor& editor)
    {
        auto sb = snap.getBounds();
        int editorW = 44, captionW = 44, h = 16, rm = 2;
        caption.setBounds(sb.getRight() - rm - editorW - captionW, sb.getY() + 2, captionW, h);
        editor.setBounds(sb.getRight() - rm - editorW, sb.getY() + 2, editorW, h);
    };
    posDelayMax(snapDelayL, delayMaxCaptionL, delayMaxEditorL);
    posDelayMax(snapDelayR, delayMaxCaptionR, delayMaxEditorR);

    // Log/Linear toggle to the left of each "max ms:" caption
    {
        int btnW = 40, btnH = 16;
        auto capL = delayMaxCaptionL.getBounds();
        delayLogScaleButtonL.setBounds(capL.getX() - btnW - 2, capL.getY(), btnW, btnH);
        auto capR = delayMaxCaptionR.getBounds();
        delayLogScaleButtonR.setBounds(capR.getX() - btnW - 2, capR.getY(), btnW, btnH);
    }

    // Row 4: Pan
    layoutSnapRow(innerPanel.removeFromTop(snapRowHeight), snapPanL, snapPanR);

    // "OUT L" / "OUT R" area with cross arrows (painted in paint())
    innerPanel.removeFromTop(outAreaH);

    // Row 5: Feedback
    layoutSnapRow(innerPanel.removeFromTop(snapRowHeight), snapFeedbackL, snapFeedbackR);

    // Remaining: fbLoopH gap then bottomControls

    // Small gap for "OUT L  OUT R" label above gain/clip/pan knobs
    bottomControls.removeFromTop(10);

    int controlKnobSize = 55;
    int knobGap = 20;
    int totalKnobW = controlKnobSize * 3 + knobGap * 2;
    auto controlsCenter = bottomControls.withSizeKeepingCentre(totalKnobW, bottomControls.getHeight());

    auto gainArea = controlsCenter.removeFromLeft(controlKnobSize);
    gainLabel.setBounds(gainArea.removeFromTop(14));
    gainSlider.setBounds(gainArea);

    controlsCenter.removeFromLeft(knobGap);

    auto clipArea = controlsCenter.removeFromLeft(controlKnobSize);
    softClipLabel.setBounds(clipArea.removeFromTop(14));
    softClipSlider.setBounds(clipArea);

    controlsCenter.removeFromLeft(knobGap);

    auto panArea = controlsCenter.removeFromLeft(controlKnobSize);
    panLabel.setBounds(panArea.removeFromTop(14));
    panSlider.setBounds(panArea);
}
