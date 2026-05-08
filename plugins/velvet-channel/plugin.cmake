set(KRATOMIX_PLUGIN_SLUG "velvet-channel")
set(KRATOMIX_CMAKE_TARGET "KratomixVelvetChannel")
set(KRATOMIX_PRODUCT_NAME "Kratomix Velvet Channel")
set(KRATOMIX_BUNDLE_ID "com.kratomix.velvetchannel")
set(KRATOMIX_PLUGIN_MANUFACTURER_CODE "Kmix")
set(KRATOMIX_PLUGIN_CODE "VChn")
set(KRATOMIX_AU_MAIN_TYPE "aufx")
set(KRATOMIX_PLUGIN_FORMATS
    AU
    VST3
    Standalone)
set(KRATOMIX_PLUGIN_SOURCES
    Source/ControlValues.h
    Source/Parameters.h
    Source/PluginProcessor.h
    Source/PluginProcessor.cpp
    Source/PluginEditor.h
    Source/PluginEditor.cpp
    Source/Dsp/WarmthProcessor.h
    Source/Dsp/WarmthProcessor.cpp
    Source/Ui/RackLookAndFeel.h
    Source/Ui/RackLookAndFeel.cpp
    Source/Ui/SteppedSlider.h
    Source/Ui/VuMeter.h
    Source/Ui/VuMeter.cpp)
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
