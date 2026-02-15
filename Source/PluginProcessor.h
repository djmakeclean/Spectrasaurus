#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "Bank.h"
#include <array>

class SpectrasaurusAudioProcessor : public juce::AudioProcessor
{
public:
    SpectrasaurusAudioProcessor();
    ~SpectrasaurusAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // Banks
    std::array<Bank, 4> banks; // A, B, C, D

    // Active bank for editing (0=A, 1=B, 2=C, 3=D)
    std::atomic<int> activeBankIndex { 0 };

    // Parameter management
    juce::AudioProcessorValueTreeState parameters;

    // Get morph parameters
    float getMorphX() const { return *parameters.getRawParameterValue("morphX"); }
    float getMorphY() const { return *parameters.getRawParameterValue("morphY"); }

    // Level metering (atomic for thread-safe access from UI)
    std::atomic<float> outputLevelL { 0.0f };
    std::atomic<float> outputLevelR { 0.0f };

    // Master gain + soft clip (global, not per-bank — safety for morph spikes)
    std::atomic<float> masterGainDB { 0.0f };
    std::atomic<float> masterClipDB { 0.0f }; // 0 = no clipping, negative = clip threshold

    // Master dry/wet mix (0.0 = fully dry, 1.0 = fully wet)
    std::atomic<float> masterDryWet { 1.0f };

    // Notes text (persisted with presets and state)
    juce::String notesText;

    // UI state (persisted so editor restores to last-used view)
    int dynamicsLCurveIndex = 0; // 0=PreGain, 1=Gate, 2=Clip
    int dynamicsRCurveIndex = 0;
    int shiftLCurveIndex = 0;    // 0=Shift, 1=Multiply
    int shiftRCurveIndex = 0;

    // Spectrograph data (post-dynamics bin magnitudes in dB, -60 to 0)
    static constexpr int kMaxSpectrographBins = 1024;
    juce::SpinLock spectrographLock;
    float spectrographDataL[kMaxSpectrographBins] = {};
    float spectrographDataR[kMaxSpectrographBins] = {};
    int spectrographNumBins = 0;
    std::atomic<bool> spectrographEnabled { false };

private:
    // Simplified FFT processing
    std::unique_ptr<juce::dsp::FFT> fft;
    std::unique_ptr<juce::dsp::WindowingFunction<float>> window;

    juce::AudioBuffer<float> inputBuffer;
    juce::AudioBuffer<float> outputBuffer;
    juce::AudioBuffer<float> fftBuffer;

    int inputBufferWritePos = 0;
    int outputBufferReadPos = 0;
    int outputBufferWritePos = 0;

    std::vector<float> leftFFTData;
    std::vector<float> rightFFTData;

    // Per-bin delay buffers
    std::vector<juce::AudioBuffer<float>> leftBinDelayBuffers;
    std::vector<juce::AudioBuffer<float>> rightBinDelayBuffers;
    std::vector<int> leftBinDelayWritePos;
    std::vector<int> rightBinDelayWritePos;

    // Per-bin feedback buffers (real + imag per bin, per channel)
    std::vector<float> feedbackLeftReal;
    std::vector<float> feedbackLeftImag;
    std::vector<float> feedbackRightReal;
    std::vector<float> feedbackRightImag;

    void processFFTFrame();

    double currentSampleRate = 48000.0;
    int currentFFTSize = 2048;
    int currentOverlapFactor = 4;
    int maxDelaySamples = 48000; // 1 second at 48kHz

    void updateFFTSettings();

    // Morphing and evaluation
    struct BinParameters
    {
        float delayL;
        float delayR;
        float panL;
        float panR;
        float feedbackL;         // Linear gain 0-~2
        float feedbackR;         // Linear gain 0-~2
        // Dynamics (linear values)
        float preGainL;          // Linear gain (0 to 1)
        float preGainR;
        float minGateL;          // Linear threshold
        float minGateR;
        float maxClipL;          // Linear threshold
        float maxClipR;
        // Spectral shift (normalized 0-1 curve values)
        float shiftL;            // normalized Y from shift curve
        float shiftR;
        float multiplyL;         // normalized Y from multiply curve
        float multiplyR;
    };

    BinParameters evaluateBinParameters(int binIndex);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SpectrasaurusAudioProcessor)
};
