cmake_minimum_required(VERSION 3.22)
project(NeomorphX1 VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(UNIX AND NOT APPLE)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -I/usr/include/freetype2")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -I/usr/include/freetype2")
endif()

# Unter Linux PkgConfig nutzen, um FreeType Include-Pfade systemweit aufzulösen
if(UNIX AND NOT APPLE)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(FREETYPE REQUIRED freetype2)
    include_directories(${FREETYPE_INCLUDE_DIRS})
endif()

# JUCE per FetchContent aus GitHub einbinden (stabiles Tag 8.0.4)
include(FetchContent)
FetchContent_Declare(
    juce
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.4
)

# Hindert JUCE daran, juceaide für VST3 Manifests zu bauen
set(JUCE_BUILD_HELPER_TOOLS OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(juce)

# Grafik-Assets in Binary-Data kompilieren (falls Ordner Assets/ existiert)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Assets")
    juce_add_binary_data(NeomorphAssets
        SOURCES
            Assets/background_main.png
            Assets/tab_oscillators.png
            Assets/tab_filter.png
            Assets/tab_envelopes.png
            Assets/tab_free_eg.png
            Assets/tab_step_seq.png
            Assets/tab_mod_matrix.png
    )
	set_target_properties(NeomorphAssets PROPERTIES POSITION_INDEPENDENT_CODE ON)
endif()

# Plugin-Target definieren
juce_add_plugin(NeomorphX1
    COMPANY_NAME "YourStudio"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    IS_MIDI_EFFECT FALSE
    SUPPORTS_MIDI_SINGLE_TO_MULTI_CHANNEL_ROUTING FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    PLUGIN_MANUFACTURER_CODE Neom
    PLUGIN_CODE Nmx1
    FORMATS VST3 AU Standalone
    PRODUCT_NAME "Neomorph X1"
)

# Generiert automatisch die benötigte JuceHeader.h für CMake Targets
juce_generate_juce_header(NeomorphX1)

# Sourcedateien
target_sources(NeomorphX1
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginProcessor.h
        Source/PluginEditor.cpp
        Source/PluginEditor.h
)

# Linken gegen BinaryData (falls vorhanden) und JUCE Module
if(TARGET NeomorphAssets)
    target_link_libraries(NeomorphX1 PRIVATE NeomorphAssets)
endif()

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

target_compile_definitions(NeomorphX1
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
)
