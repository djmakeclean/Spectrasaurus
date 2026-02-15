#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"
#include "SnapWindow.h"
#include "DynamicsSnapWindow.h"
#include "XYPad.h"
#include "ShiftSnapWindow.h"
#include "ClipboardMeta.h"

class SpectrasaurusAudioProcessorEditor : public juce::AudioProcessorEditor,
                                          private juce::Timer
{
public:
    SpectrasaurusAudioProcessorEditor (SpectrasaurusAudioProcessor&);
    ~SpectrasaurusAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;

private:
    SpectrasaurusAudioProcessor& audioProcessor;

    // Bank selector tabs
    juce::TextButton bankButtons[4];
    int selectedBank = 0;

    // Dynamics snap windows (row 1) — controls are internal to DynamicsSnapWindow
    DynamicsSnapWindow dynamicsL;
    DynamicsSnapWindow dynamicsR;

    // Snap windows for current bank (3 pairs: delay, crossfeed, feedback)
    SnapWindow snapDelayL;
    SnapWindow snapDelayR;
    SnapWindow snapPanL;   // L -> R
    SnapWindow snapPanR;   // R -> L
    SnapWindow snapFeedbackL;
    SnapWindow snapFeedbackR;

    // Shift snap windows (row 2)
    ShiftSnapWindow shiftL;
    ShiftSnapWindow shiftR;

    // XY Morphing pad
    XYPad xyPad;

    // Sliders for X and Y (for automation)
    juce::Slider morphXSlider;
    juce::Slider morphYSlider;
    juce::Label morphXLabel;
    juce::Label morphYLabel;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> morphXAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> morphYAttachment;

    // Single shared clipboard for right-click copy/paste
    PiecewiseFunction curveClipboard;
    bool clipboardFilled = false;
    ClipboardMeta clipboardMeta;

    // Per-bank gain/clip/pan controls (shows current bank only)
    juce::Slider gainSlider;
    juce::Label gainLabel;
    juce::Slider softClipSlider;
    juce::Label softClipLabel;
    juce::Slider panSlider;
    juce::Label panLabel;

    // Master gain/clip/dry-wet controls (global, not per-bank)
    juce::Slider masterGainSlider;
    juce::Label masterGainLabel;
    juce::Slider masterClipSlider;
    juce::Label masterClipLabel;
    juce::Slider masterDryWetSlider;
    juce::Label masterDryWetLabel;

    // Delay max time editors (per channel)
    juce::Label delayMaxCaptionL;
    juce::TextEditor delayMaxEditorL;
    juce::Label delayMaxCaptionR;
    juce::TextEditor delayMaxEditorR;

    // Delay log scale toggles (per-channel)
    juce::TextButton delayLogScaleButtonL;
    juce::TextButton delayLogScaleButtonR;

    // Notes text editor
    juce::TextEditor notesEditor;

    // Preset save/load
    juce::TextButton savePresetButton;
    juce::TextButton loadPresetButton;
    std::unique_ptr<juce::FileChooser> fileChooser;
    void savePreset();
    void loadPreset();

    // Chrome-tab panel bounds (used in paint)
    juce::Rectangle<int> panelArea;

    // Right-side panel bounds (used in paint)
    juce::Rectangle<int> bankMorphPanel;
    juce::Rectangle<int> masterPanel;
    juce::Rectangle<int> notesPanel;
    juce::Rectangle<int> presetPanel;

    void selectBank(int bankIndex);
    void updateSnapWindows();

    // Bank right-click context menu
    void mouseDown(const juce::MouseEvent& event) override;
    void showBankContextMenu(int bankIndex);
    Bank bankClipboard;
    bool bankClipboardFilled = false;

    // Level metering
    float meterLevelL = 0.0f;
    float meterLevelR = 0.0f;
    juce::Rectangle<int> meterAreaL;
    juce::Rectangle<int> meterAreaR;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SpectrasaurusAudioProcessorEditor)
};
