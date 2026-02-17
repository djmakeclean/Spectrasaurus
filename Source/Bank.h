#pragma once

#include <juce_core/juce_core.h>
#include "PiecewiseFunction.h"

enum class CurveType
{
    DelayL = 0,
    DelayR = 1,
    PanL = 2,
    PanR = 3,
    FeedbackL = 4,
    FeedbackR = 5,
    PreGainL = 6,
    PreGainR = 7,
    MinGateL = 8,
    MinGateR = 9,
    MaxClipL = 10,
    MaxClipR = 11,
    ShiftL = 12,
    ShiftR = 13,
    MultiplyL = 14,
    MultiplyR = 15
};

class Bank
{
public:
    Bank();
    ~Bank() = default;

    // FFT settings
    int fftSize = 2048;
    int overlapFactor = 4;

    // Delay settings (per channel)
    float delayMaxTimeMsL = 1000.0f; // Maximum delay time in ms (left)
    float delayMaxTimeMsR = 1000.0f; // Maximum delay time in ms (right)
    bool delayLogScaleL = false;     // Log vs linear scale for delay Y-axis (left)
    bool delayLogScaleR = false;     // Log vs linear scale for delay Y-axis (right)

    // Piecewise function curves
    PiecewiseFunction delayL;
    PiecewiseFunction delayR;
    PiecewiseFunction panL;
    PiecewiseFunction panR;
    PiecewiseFunction feedbackL;
    PiecewiseFunction feedbackR;

    // Dynamics curves (pre-delay stage)
    // PreGain: per-bin gain/filter, default 0 dB (Y=1.0)
    // MinGate: gate threshold, default -60 dB (Y=0.0, no gating)
    // MaxClip: clip threshold, default 0 dB (Y=1.0, no clipping)
    PiecewiseFunction preGainL;
    PiecewiseFunction preGainR;
    PiecewiseFunction minGateL;
    PiecewiseFunction minGateR;
    PiecewiseFunction maxClipL;
    PiecewiseFunction maxClipR;

    // Spectral shift curves (pre-delay stage)
    // Shift: additive Hz offset, curve at Y=0.5 means 0Hz shift
    // Multiply: multiplicative factor, curve at Y=0.5 means 1.0x
    PiecewiseFunction shiftL;
    PiecewiseFunction shiftR;
    PiecewiseFunction multiplyL;
    PiecewiseFunction multiplyR;

    // Shift/multiply application order (ranges are display-only, stored in UI)
    bool shiftBeforeMultiply = true; // true = shift→multiply, false = multiply→shift

    // Per-bank soft clip threshold in dB (0 = no clipping, -20 = aggressive)
    float softClipThresholdDB = 0.0f;

    // Per-bank stereo pan (-1.0 = full left, 0.0 = center, +1.0 = full right)
    float panValue = 0.0f;

    // Get curve by type
    PiecewiseFunction& getCurve(CurveType type);
    const PiecewiseFunction& getCurve(CurveType type) const;

    // Get number of FFT bins
    int getNumBins() const { return fftSize / 2; }

    // Evaluate delay for a specific bin (returns delay in samples)
    float evaluateDelay(CurveType curveType, int binIndex, float sampleRate) const;

    // Evaluate pan for a specific bin (returns 0.0 to 1.0)
    float evaluatePan(CurveType curveType, int binIndex) const;

    // Evaluate feedback for a specific bin (returns linear gain 0.0 to ~2.0)
    // Maps normalized Y (0-1) through -60 dB floor to +6 dB ceiling: dB = (y * 66) - 60
    float evaluateFeedback(CurveType curveType, int binIndex) const;

    // Evaluate normalized curve value (0-1) for a bin without conversion
    float evaluateCurveNormalized(CurveType curveType, int binIndex, float sampleRate) const;

    // Per-bank gain in dB
    float gainDB = 0.0f;

    // Reset all curves to default
    void reset();

    // Serialization
    juce::var toVar() const;
    void fromVar(const juce::var& v);

    // --- Curve LUT precomputation ---
    static constexpr int kLUTMaxBins = 1024;

    // Per-curve LUT: stores raw normalized values (0-1) for each bin
    float curveLUT[16][kLUTMaxBins] = {};

    // Version tracking: last-seen version per curve
    uint32_t lutCurveVersions[16] = {};

    // Cached parameters for LUT validity
    float lutSampleRate = 0.0f;
    int lutNumBins = 0;

    // Precomputed log-frequency constants (for LUT rebuild)
    float lutLogMin = 0.0f;
    float lutLogRangeInv = 1.0f;

    // Rebuild any stale curves in the LUT. Called from audio thread under bankLock.
    void rebuildLUTIfNeeded(int numBins, float sampleRate);

private:
    // Convert normalized frequency (0-1) to actual frequency in Hz
    float normalizedToFrequency(float normalized, float sampleRate) const;

    // Convert bin index to normalized frequency (0-1) on log scale
    float binToNormalizedFreq(int binIndex, float sampleRate) const;
};
