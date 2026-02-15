cmake_minimum_required(VERSION 3.22)

project(Spectrasaurus VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Fetch JUCE
include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 7.0.12
)
FetchContent_MakeAvailable(JUCE)

# Add plugin target
juce_add_plugin(Spectrasaurus
    COMPANY_NAME "Spectrasaurus"
    PLUGIN_MANUFACTURER_CODE Spec
    PLUGIN_CODE Saur
    FORMATS VST3 Standalone
    PRODUCT_NAME "Spectrasaurus"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE
    VST3_CATEGORIES Fx Stereo
)

# Add source files
target_sources(Spectrasaurus
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
        Source/PiecewiseFunction.cpp
        Source/Bank.cpp
        Source/SnapWindow.cpp
        Source/XYPad.cpp
        Source/DynamicsSnapWindow.cpp
        Source/ShiftSnapWindow.cpp
)

# Link JUCE modules
target_link_libraries(Spectrasaurus
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

# Preprocessor definitions
target_compile_definitions(Spectrasaurus
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
)
