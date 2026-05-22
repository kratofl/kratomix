set(KRATOMIX_PLUGIN_SLUG "multiband-compressor")
set(KRATOMIX_CMAKE_TARGET "KratomixMultibandCompressor")
set(KRATOMIX_PRODUCT_NAME "Kratomix Multiband Compressor")
set(KRATOMIX_BUNDLE_ID "com.kratomix.multibandcompressor")
set(KRATOMIX_PLUGIN_MANUFACTURER_CODE "Kmix")
set(KRATOMIX_PLUGIN_CODE "KMBC")
set(KRATOMIX_AU_MAIN_TYPE "audyn")
set(KRATOMIX_PLUGIN_FORMATS
    AU
    VST3
    Standalone)
set(KRATOMIX_PLUGIN_SOURCES
    Source/Parameters.h
    Source/PluginProcessor.h
    Source/PluginProcessor.cpp
    Source/PluginEditor.h
    Source/PluginEditor.cpp
    Source/Dsp/MultibandTypes.h
    Source/Dsp/MultibandProcessor.h
    Source/Dsp/MultibandProcessor.cpp
    Source/Ui/MultibandGraph.h
    Source/Ui/MultibandGraph.cpp)
set(KRATOMIX_PLUGIN_TEST_SOURCES
    Tests/ProcessorBehaviorTests.cpp)
set(KRATOMIX_PLUGIN_INCLUDE_DIRS
    .
    Source)

if(COMMAND kratomix_add_plugin)
    kratomix_add_plugin(
        SLUG "${KRATOMIX_PLUGIN_SLUG}"
        TARGET "${KRATOMIX_CMAKE_TARGET}"
        PRODUCT_NAME "${KRATOMIX_PRODUCT_NAME}"
        BUNDLE_ID "${KRATOMIX_BUNDLE_ID}"
        MANUFACTURER_CODE "${KRATOMIX_PLUGIN_MANUFACTURER_CODE}"
        PLUGIN_CODE "${KRATOMIX_PLUGIN_CODE}"
        AU_MAIN_TYPE "${KRATOMIX_AU_MAIN_TYPE}"
        FORMATS ${KRATOMIX_PLUGIN_FORMATS}
        SOURCES ${KRATOMIX_PLUGIN_SOURCES}
        TEST_SOURCES ${KRATOMIX_PLUGIN_TEST_SOURCES}
        INCLUDE_DIRS ${KRATOMIX_PLUGIN_INCLUDE_DIRS}
        SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}")
endif()
