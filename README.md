# Spectrasaurus VST Plugin

A VST3 plugin for creative spectral processing with per-bin delay and stereo panning.

## Features

- **FFT-based processing** with configurable FFT size and overlap factor
- **4 Banks (A, B, C, D)** each containing:
  - Delay L/R curves (per-bin delay control)
  - Pan L/R curves (per-bin stereo panning)
- **XY Morphing Pad** for real-time interpolation between all 4 banks
- **SnapWindow UI** for editing piecewise functions with intuitive click-and-drag interface
- **Copy/Paste** functionality for curves across banks

## Building

### Prerequisites
- CMake 3.22 or higher
- C++17 compatible compiler
- macOS (tested on macOS 14.4)

### Build Instructions

```bash
mkdir build
cd build
cmake ..
cmake --build . --config Release
```

The built plugin will be available in:
- **Standalone**: `build/Spectrasaurus_artefacts/Standalone/Spectrasaurus.app`
- **VST3**: `build/Spectrasaurus_artefacts/VST3/Spectrasaurus.vst3`

To install the VST3 plugin:
```bash
cp -r build/Spectrasaurus_artefacts/VST3/Spectrasaurus.vst3 ~/Library/Audio/Plug-Ins/VST3/
```

## Usage

### Banks
- Click on A, B, C, or D tabs at the top to switch between banks
- Each bank has independent delay and pan curves
- Use the XY pad on the right to morph between all 4 banks simultaneously

### SnapWindow Editing
- **Add point**: Click anywhere on the curve
- **Move point**: Click and drag a control point
- **Remove point**: Click near an existing point (endpoints cannot be removed)
- **Hover**: Move mouse over the window to see frequency values

### Copy/Paste
- **Copy Delay**: Copies both delay curves (L/R) from current bank
- **Paste Delay**: Pastes delay curves to current bank
- **Copy Pan**: Copies both pan curves (L/R) from current bank
- **Paste Pan**: Pastes pan curves to current bank

### Parameters

#### Delay
- **Y-axis**: Delay time (0 to max delay time, default 1000ms)
- **Log/Linear**: Toggle between logarithmic and linear scaling
- **Max Delay**: Configurable maximum delay time per bank

#### Pan
- **0.0**: Sound stays in original channel
- **0.5**: Equal-power center (indicated by faint line)
- **1.0**: Sound goes to opposite channel

#### X-axis (all curves)
- Frequency on logarithmic scale (20 Hz to Nyquist)

## Current Status

### ✅ Implemented
- JUCE project structure with CMake
- Bank data structure with 4 piecewise functions each
- FFT processing with overlap-add and Hann window
- Bank interpolation and XY morphing logic
- SnapWindow UI with click/drag/hover
- XY pad UI for morphing
- Bank tab selector
- Copy/paste functionality
- Basic UI layout

### 🚧 In Progress / TODO
1. **Per-bin delay buffers**: Currently delay is evaluated but not applied
2. **Proper stereo panning**: Pan evaluation is implemented but needs proper stereo bin processing
3. **Point removal**: Click near point to remove (partially working, needs refinement)
4. **UI controls**: Add FFT size, overlap factor, delay max/log controls to UI
5. **State saving**: Implement getStateInformation/setStateInformation for preset recall
6. **Performance optimization**: Skip processing identity banks/bins
7. **Testing**: Comprehensive audio testing and debugging

## Technical Details

### FFT Processing
- Default FFT size: 2048 samples (1024 bins)
- Default overlap: 4x
- Window function: Hann
- Process each bin independently with delay and pan

### Panning Law
- Equal-power panning for constant perceived loudness
- Formula: pan angle = panValue × π/2
  - gainSame = cos(angle)
  - gainOpposite = sin(angle)

### Morphing
- Bilinear interpolation between 4 banks
- XY coordinates map to corner banks:
  - (0, 0) = Bank A (top-left)
  - (1, 0) = Bank B (top-right)
  - (0, 1) = Bank C (bottom-left)
  - (1, 1) = Bank D (bottom-right)

## File Structure

```
Source/
├── PluginProcessor.h/cpp    # Main audio processor with FFT engine
├── PluginEditor.h/cpp        # Main UI editor
├── Bank.h/cpp                # Bank data structure
├── PiecewiseFunction.h/cpp   # Piecewise function class
├── SnapWindow.h/cpp          # Curve editor UI component
└── XYPad.h/cpp               # Morphing pad UI component
```

## License

See LICENSE file for details.
