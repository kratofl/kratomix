include_guard(GLOBAL)

get_filename_component(KRATOMIX_CORE_DIR "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
set(KRATOMIX_CORE_UI_SOURCES
    "${KRATOMIX_CORE_DIR}/ui/RackLookAndFeel.cpp"
    "${KRATOMIX_CORE_DIR}/ui/VuMeter.cpp")

function(kratomix_append_global property_name value)
    set_property(GLOBAL APPEND PROPERTY "${property_name}" "${value}")
endfunction()

function(kratomix_add_plugin)
    set(options)
    set(one_value_args
        AU_MAIN_TYPE
        BUNDLE_ID
        COMPANY_NAME
        MANUFACTURER_CODE
        PLUGIN_CODE
        PRODUCT_NAME
        SLUG
        SOURCE_DIR
        TARGET)
    set(multi_value_args
        FORMATS
        INCLUDE_DIRS
        SOURCES
        TEST_SOURCES)

    cmake_parse_arguments(KRATOMIX "${options}" "${one_value_args}" "${multi_value_args}" ${ARGN})

    foreach(required_arg SLUG TARGET PRODUCT_NAME BUNDLE_ID MANUFACTURER_CODE PLUGIN_CODE AU_MAIN_TYPE SOURCE_DIR)
        if(NOT KRATOMIX_${required_arg})
            message(FATAL_ERROR "kratomix_add_plugin missing required argument ${required_arg}")
        endif()
    endforeach()

    if(NOT KRATOMIX_COMPANY_NAME)
        set(KRATOMIX_COMPANY_NAME "Kratomix")
    endif()

    set(source_files)
    foreach(source_file IN LISTS KRATOMIX_SOURCES)
        list(APPEND source_files "${KRATOMIX_SOURCE_DIR}/${source_file}")
    endforeach()

    set(include_dirs "${KRATOMIX_SOURCE_DIR}" "${KRATOMIX_CORE_DIR}")
    foreach(include_dir IN LISTS KRATOMIX_INCLUDE_DIRS)
        if(IS_ABSOLUTE "${include_dir}")
            list(APPEND include_dirs "${include_dir}")
        else()
            list(APPEND include_dirs "${KRATOMIX_SOURCE_DIR}/${include_dir}")
        endif()
    endforeach()
    list(REMOVE_DUPLICATES include_dirs)

    juce_add_plugin(${KRATOMIX_TARGET}
        COMPANY_NAME "${KRATOMIX_COMPANY_NAME}"
        VERSION "${KRATOMIX_PLUGIN_VERSION}"
        BUNDLE_ID "${KRATOMIX_BUNDLE_ID}"
        IS_SYNTH FALSE
        NEEDS_MIDI_INPUT FALSE
        NEEDS_MIDI_OUTPUT FALSE
        IS_MIDI_EFFECT FALSE
        COPY_PLUGIN_AFTER_BUILD TRUE
        PLUGIN_MANUFACTURER_CODE ${KRATOMIX_MANUFACTURER_CODE}
        PLUGIN_CODE ${KRATOMIX_PLUGIN_CODE}
        FORMATS ${KRATOMIX_FORMATS}
        PRODUCT_NAME "${KRATOMIX_PRODUCT_NAME}")

    juce_generate_juce_header(${KRATOMIX_TARGET})

    target_sources(${KRATOMIX_TARGET}
        PRIVATE
            ${source_files}
            ${KRATOMIX_CORE_UI_SOURCES})

    target_include_directories(${KRATOMIX_TARGET}
        PRIVATE
            ${include_dirs})

    target_compile_definitions(${KRATOMIX_TARGET}
        PUBLIC
            KRATOMIX_PLUGIN_VERSION_STRING="${KRATOMIX_PLUGIN_VERSION}"
            JUCE_VST3_CAN_REPLACE_VST2=0
            JUCE_WEB_BROWSER=0
            JUCE_USE_CURL=0)

    target_link_libraries(${KRATOMIX_TARGET}
        PRIVATE
            juce::juce_audio_utils
            juce::juce_dsp
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_lto_flags
            juce::juce_recommended_warning_flags)

    set(plugin_output_targets)

    foreach(plugin_format IN LISTS KRATOMIX_FORMATS)
        if(plugin_format STREQUAL "AU")
            add_custom_target("plugin-${KRATOMIX_SLUG}-au" DEPENDS "${KRATOMIX_TARGET}_AU")
            list(APPEND plugin_output_targets "${KRATOMIX_TARGET}_AU")
        elseif(plugin_format STREQUAL "Standalone")
            add_custom_target("plugin-${KRATOMIX_SLUG}-standalone" DEPENDS "${KRATOMIX_TARGET}_Standalone")
            list(APPEND plugin_output_targets "${KRATOMIX_TARGET}_Standalone")
        elseif(plugin_format STREQUAL "VST3")
            add_custom_target("plugin-${KRATOMIX_SLUG}-vst3" DEPENDS "${KRATOMIX_TARGET}_VST3")
            list(APPEND plugin_output_targets "${KRATOMIX_TARGET}_VST3")
        else()
            message(FATAL_ERROR "Unsupported plugin format '${plugin_format}' for ${KRATOMIX_SLUG}")
        endif()
    endforeach()

    add_custom_target("plugin-${KRATOMIX_SLUG}" DEPENDS ${plugin_output_targets})
    kratomix_append_global(KRATOMIX_PLUGIN_TARGETS "plugin-${KRATOMIX_SLUG}")

    if(KRATOMIX_TEST_SOURCES)
        set(test_source_files)
        foreach(test_source_file IN LISTS KRATOMIX_TEST_SOURCES)
            list(APPEND test_source_files "${KRATOMIX_SOURCE_DIR}/${test_source_file}")
        endforeach()

        set(test_target "${KRATOMIX_TARGET}Tests")

        juce_add_console_app(${test_target}
            PRODUCT_NAME "${KRATOMIX_PRODUCT_NAME} Tests")

        juce_generate_juce_header(${test_target})

        target_sources(${test_target}
            PRIVATE
                ${test_source_files}
                ${source_files}
                ${KRATOMIX_CORE_UI_SOURCES})

        target_include_directories(${test_target}
            PRIVATE
                ${include_dirs})

        target_compile_definitions(${test_target}
            PUBLIC
                KRATOMIX_PLUGIN_VERSION_STRING="${KRATOMIX_PLUGIN_VERSION}"
                JUCE_VST3_CAN_REPLACE_VST2=0
                JUCE_WEB_BROWSER=0
                JUCE_USE_CURL=0)

        target_link_libraries(${test_target}
            PRIVATE
                juce::juce_audio_utils
                juce::juce_dsp
            PUBLIC
                juce::juce_recommended_config_flags
                juce::juce_recommended_lto_flags
                juce::juce_recommended_warning_flags)

        add_test(NAME "test-${KRATOMIX_SLUG}" COMMAND ${test_target})
        add_custom_target("test-${KRATOMIX_SLUG}" DEPENDS ${test_target})
        kratomix_append_global(KRATOMIX_TEST_TARGETS "test-${KRATOMIX_SLUG}")
    endif()
endfunction()

function(kratomix_finalize_plugins)
    get_property(plugin_targets GLOBAL PROPERTY KRATOMIX_PLUGIN_TARGETS)
    if(plugin_targets)
        list(REMOVE_DUPLICATES plugin_targets)
        add_custom_target(plugins-all DEPENDS ${plugin_targets})
    else()
        add_custom_target(plugins-all)
    endif()

    get_property(test_targets GLOBAL PROPERTY KRATOMIX_TEST_TARGETS)
    if(test_targets)
        list(REMOVE_DUPLICATES test_targets)
        add_custom_target(tests-all DEPENDS ${test_targets})
    else()
        add_custom_target(tests-all)
    endif()
endfunction()
