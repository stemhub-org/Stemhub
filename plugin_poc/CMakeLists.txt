cmake_minimum_required(VERSION 3.22)

project(StemHubPluginPOC VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(STEMHUB_POC_FETCH_JUCE "Download JUCE when a local checkout is not available" ON)
set(STEMHUB_POC_JUCE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/JUCE" CACHE PATH "Path to a JUCE checkout")

if (EXISTS "${STEMHUB_POC_JUCE_PATH}/CMakeLists.txt")
    add_subdirectory("${STEMHUB_POC_JUCE_PATH}" JUCE)
elseif (STEMHUB_POC_FETCH_JUCE)
    include(FetchContent)
    FetchContent_Declare(
        juce
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG master
    )
    FetchContent_MakeAvailable(juce)
else ()
    message(FATAL_ERROR "JUCE not found. Set STEMHUB_POC_JUCE_PATH to a valid JUCE checkout.")
endif ()

add_library(stemhub_juce_defaults INTERFACE)
target_link_libraries(stemhub_juce_defaults
    INTERFACE
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

set(STEMHUB_POC_USER_CMAKE "${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.user.cmake")
if (EXISTS "${STEMHUB_POC_USER_CMAKE}")
    include("${STEMHUB_POC_USER_CMAKE}")
    message(STATUS "Loaded user plugin config: ${STEMHUB_POC_USER_CMAKE}")
else ()
    add_custom_target(stemhub_poc_ready
        COMMAND ${CMAKE_COMMAND} -E echo "JUCE is ready. Create CMakeLists.user.cmake for your juce_add_plugin(...) target."
        VERBATIM
    )
    message(STATUS "JUCE bootstrap configured. Add your plugin target in CMakeLists.user.cmake.")
endif ()
