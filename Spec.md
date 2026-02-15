### Spec for the Spectrasaurus Plugin

## Overview
Spectrasaurus is a JUCE-based VST3/Standalone audio plugin that performs spectral processing via FFT, applying per-bin delay, stereo crossfeed, and feedback independently to each frequency bin and stereo channel.

## Architecture

### DSP Pipeline (per FFT frame, per bin)
1. Extract FFT bin (complex real+imag) for L and R channels
2. Add feedback from previous frame
3. Apply per-bin delay (write to circular buffer, read delayed)
4. Apply L->R / R->L crossfeed (equal-power panning law)
5. Store output × feedback gain into feedback buffer (pre-gain/clip)
6. Apply per-bank gain
7. Apply per-bank soft clip (tanh saturation)
8. Write to output FFT buffer

### FFT Settings
- **FFT Size**: 2048 samples (1024 bins)
- **Overlap Factor**: 4x (75% overlap)
- **Window Function**: Hann (applied after IFFT for COLA)
- **Scale Factor**: 0.25 for unity gain with synthesis window

## Interface

### Bank System
4 banks (A, B, C, D), each containing:
- **6 piecewise function curves**: Delay L, Delay R, L->R, R->L, Feedback L, Feedback R
- **Settings**: delay max time (ms), delay log/linear scale, gain (dB), soft clip threshold (dB)

Banks are selected via chrome-style tabs at the top of the panel.

### Snap Windows (Curve Editors)
Arranged in 3 rows × 2 columns inside the panel:
- **Row 1**: Delay L / Delay R — Y-axis: 0 to max ms (linear or log)
- **Row 2**: L->R / R->L — Y-axis: 0.0 (Same) to 1.0 (Opposite)
- **Row 3**: Feedback L / Feedback R — Y-axis: -60 dB to 0 dB

Each snap window is a piecewise linear function editor:
- X-axis: frequency on log scale (20 Hz to Nyquist)
- Click empty space to add a control point
- Click existing point to remove it (endpoints cannot be removed)
- Drag points to move them (endpoints only move vertically)
- Right-click for copy/paste context menu (shared clipboard across all windows)
- Hover shows frequency + Y-value

### XY Morphing Pad
- Square pad with A (top-left), B (top-right), C (bottom-left), D (bottom-right)
- Bilinear interpolation of all curve values and bank settings
- X/Y knobs exposed as AudioProcessorValueTreeState parameters for DAW automation

### Per-Bank Controls
- **Gain knob**: -40 to +12 dB
- **Soft Clip knob**: -20 to 0 dB threshold (tanh saturation)
- **Delay Max editor**: 1–10000 ms text input

### Morphing Details
- All curve evaluations use normalized values (0–1) which are interpolated across banks
- Bank settings (delay max, log scale, gain, soft clip) are also interpolated
- Log scale decision uses majority vote (>0.5 weight)

### Feedback Safety
- Feedback is muted on any bin where the delay is less than ~1ms
- NaN/Inf values in feedback buffers are sanitized to 0

## Copy/Paste
- Right-click any snap window → Copy Curve / Paste Curve
- Single shared clipboard across all 6 snap windows (shape transfers cross-type)
- SafePointer used in async callback for crash safety

## Build
```bash
cmake --build build
```
Requires CMake 3.22+, C++17, macOS. JUCE 7.0.12 fetched automatically.
