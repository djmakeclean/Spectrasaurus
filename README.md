# Spectrasaurus

A spectral processing VST3 plugin. Per-bin delay, panning, dynamics, frequency shifting, and feedback across 4 morphable banks.

## Download

Go to the [Releases](https://github.com/patdemichele/Spectrasaurus/releases) page. Download the zip for your platform:

- **macOS** (Intel + Apple Silicon): Unzip, copy `Spectrasaurus.vst3` to `~/Library/Audio/Plug-Ins/VST3/`. On first launch you may need to right-click the plugin and select Open, or run `xattr -cr Spectrasaurus.vst3` in Terminal.
- **Windows**: Unzip, copy `Spectrasaurus.vst3` to `C:\Program Files\Common Files\VST3\`.

Rescan plugins in your DAW. Presets are included in the zip under `Presets/Factory/`.

## Building from source

Requires CMake 3.22+ and a C++17 compiler. JUCE is fetched automatically.

```bash
git clone https://github.com/patdemichele/Spectrasaurus.git
cd Spectrasaurus
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target Spectrasaurus_VST3 --config Release
```

The built plugin is at `build/Spectrasaurus_artefacts/Release/VST3/Spectrasaurus.vst3`.

## License

See LICENSE file for details.
