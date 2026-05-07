cmake_minimum_required(VERSION 3.22)

project(KratomixVelvetChannel
    VERSION 0.1.0
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")

option(KRATOMIX_FETCH_JUCE "Download JUCE with CMake FetchContent when JUCE_DIR is not set" ON)
set(JUCE_DIR "" CACHE PATH "Path to a local JUCE checkout")

if(JUCE_DIR)
    add_subdirectory("${JUCE_DIR}" JUCE)
elseif(KRATOMIX_FETCH_JUCE)
    include(FetchContent)
    FetchContent_Declare(
        JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.8)
    FetchContent_MakeAvailable(JUCE)
else()
    message(FATAL_ERROR "Set JUCE_DIR or enable KRATOMIX_FETCH_JUCE")
endif()

juce_add_plugin(KratomixVelvetChannel
    COMPANY_NAME "Kratomix"
    BUNDLE_ID "com.kratomix.velvetchannel"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE
    PLUGIN_MANUFACTURER_CODE Kmix
    PLUGIN_CODE VChn
    FORMATS AU VST3 Standalone
    PRODUCT_NAME "Kratomix Velvet Channel")

target_sources(KratomixVelvetChannel
    PRIVATE
        Source/Parameters.h
        Source/PluginProcessor.h
        Source/PluginProcessor.cpp
        Source/PluginEditor.h
        Source/PluginEditor.cpp
        Source/Dsp/WarmthProcessor.h
        Source/Dsp/WarmthProcessor.cpp)

target_compile_definitions(KratomixVelvetChannel
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(KratomixVelvetChannel
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
