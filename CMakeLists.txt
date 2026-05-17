cmake_minimum_required(VERSION 3.22)

project(Kratomix
    VERSION 0.1.0
    LANGUAGES C CXX)

set(KRATOMIX_PLUGIN_VERSION "${PROJECT_VERSION}")
if(DEFINED KRATOMIX_RELEASE_VERSION AND NOT KRATOMIX_RELEASE_VERSION STREQUAL "")
    set(KRATOMIX_PLUGIN_VERSION "${KRATOMIX_RELEASE_VERSION}")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")

enable_testing()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/core/cmake")

include(KratomixJuce)
include(KratomixPlugin)

kratomix_setup_juce()

file(GLOB KRATOMIX_PLUGIN_FILES RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}" "plugins/*/plugin.cmake")

if(NOT KRATOMIX_PLUGIN_FILES)
    message(FATAL_ERROR "No plugins found under ${CMAKE_CURRENT_SOURCE_DIR}/plugins")
endif()

list(SORT KRATOMIX_PLUGIN_FILES)

foreach(plugin_file IN LISTS KRATOMIX_PLUGIN_FILES)
    include("${CMAKE_CURRENT_SOURCE_DIR}/${plugin_file}")
endforeach()

kratomix_finalize_plugins()
