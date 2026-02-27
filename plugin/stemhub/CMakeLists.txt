cmake_minimum_required(VERSION 3.22)

project(stemhub VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
option(COPY_PLUGIN "Copy plugin bundles to local plugin folders after build" ON)

set(JUCE_WEB_BROWSER 0 CACHE STRING "Disable JUCE web browser support" FORCE)

set(JUCE_DIR "" CACHE PATH "Path to JUCE repository root")

if(JUCE_DIR STREQUAL "")
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/JUCE/CMakeLists.txt")
        set(JUCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/JUCE")
    else()
        message(FATAL_ERROR
            "JUCE_DIR is not set. Pass -DJUCE_DIR=/path/to/JUCE or place JUCE at Plugin/stemhub/JUCE.")
    endif()
endif()

add_subdirectory(${JUCE_DIR} JUCE)

juce_add_plugin(stemhub
    COMPANY_NAME "Stemhub"
    BUNDLE_ID "com.stemhub.session"
    IS_SYNTH FALSE
    NEEDS_CURL FALSE
    NEEDS_WEB_BROWSER FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD ${COPY_PLUGIN}
    PLUGIN_MANUFACTURER_CODE Manu
    PLUGIN_CODE Wbcr
    VST3_CATEGORIES Fx Tools
    FORMATS VST3 Standalone
    PRODUCT_NAME "Stemhub Session")

juce_generate_juce_header(stemhub)

target_sources(stemhub
    PRIVATE
        Source/PluginEditor.cpp
        Source/PluginProcessor.cpp)

target_compile_definitions(stemhub
    PRIVATE
        JUCE_USE_CURL=0
        JUCE_WEB_BROWSER=0
        JUCE_STRICT_REFCOUNTEDPOINTER=1
        JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(stemhub
    PRIVATE
        juce::juce_audio_utils
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
