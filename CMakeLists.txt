cmake_minimum_required(VERSION 3.22)
project(NeomorphX1 VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# JUCE per FetchContent automatisch aus GitHub einbinden
include(FetchContent)
FetchContent_Declare(
    juce
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.0 # Oder neuere Version
)
FetchContent_MakeAvailable(juce)

# Plugin-Target definieren
juce_add_plugin(NeomorphX1
    COMPANY_NAME "YourStudio"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    IS_MIDI_EFFECT FALSE
    SUPPORTS_MIDI_SINGLE_TO_MULTI_CHANNEL_ROUTING FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE
    PLUGIN_MANUFACTURER_CODE Neom
    PLUGIN_CODE Nmx1
    FORMATS VST3 AU Standalone
    PRODUCT_NAME "Neomorph X1"
)

# Sourcedateien
target_sources(NeomorphX1
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginProcessor.h
        Source/PluginEditor.cpp
        Source/PluginEditor.h
)

# Linken gegen JUCE Module
target_link_libraries(NeomorphX1
    PRIVATE
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)