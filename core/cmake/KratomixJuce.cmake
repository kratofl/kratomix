include_guard(GLOBAL)

option(KRATOMIX_FETCH_JUCE "Download JUCE with CMake FetchContent when JUCE_DIR is not set" ON)
set(JUCE_DIR "" CACHE PATH "Path to a local JUCE checkout")

function(kratomix_setup_juce)
    if(TARGET juce::juce_audio_utils)
        return()
    endif()

    if(JUCE_DIR)
        add_subdirectory("${JUCE_DIR}" JUCE)
        return()
    endif()

    if(NOT KRATOMIX_FETCH_JUCE)
        message(FATAL_ERROR "Set JUCE_DIR or enable KRATOMIX_FETCH_JUCE")
    endif()

    include(FetchContent)
    FetchContent_Declare(
        JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.8)
    FetchContent_MakeAvailable(JUCE)
endfunction()
