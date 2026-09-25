cmake_minimum_required(VERSION 3.22)

project(stemhub VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
option(COPY_PLUGIN "Copy plugin bundles to local plugin folders after build" ON)

set(JUCE_DIR "" CACHE PATH "Path to JUCE repository root")
set(STEMHUB_API_BASE_URL "http://localhost:8000" CACHE STRING
    "StemHub API the plugin uses unless overridden at runtime (see Source/include/network/ApiConfig.hpp)")

if(JUCE_DIR STREQUAL "")
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/JUCE/CMakeLists.txt")
        set(JUCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/JUCE")
    else()
        message(FATAL_ERROR
            "JUCE_DIR is not set. Pass -DJUCE_DIR=/path/to/JUCE or place JUCE at Plugin/stemhub/JUCE.")
    endif()
endif()

add_subdirectory(${JUCE_DIR} JUCE)

# Everything except the editor and its views: built into the plugin and into the headless tests.
set(STEMHUB_CORE_SOURCES
    Source/src/application/ProcAudio.cpp
    Source/src/application/ProcAuth.cpp
    Source/src/application/ProcProject.cpp
    Source/src/application/PluginProcessor.cpp
    Source/src/application/ProjectFileService.cpp
    Source/src/application/SessionCache.cpp
    Source/src/application/SnapshotBundler.cpp
    Source/src/application/UseCases.cpp
    Source/src/network/ApiClient.cpp
    Source/src/network/ApiConfig.cpp
    Source/src/network/VersionControlService.cpp)

set(STEMHUB_UI_SOURCES
    Source/src/ui/PluginEditor.cpp
    Source/src/ui/PluginTheme.cpp
    Source/src/ui/DashboardView.cpp
    Source/src/ui/LoginView.cpp)

# Settings every StemHub target shares. JUCE compiles its modules into each target that links
# them, so the core is shared as a source list: a static library would build JUCE twice.
function(stemhub_configure_target target)
    target_include_directories(${target} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/Source/include")

    target_compile_definitions(${target}
        PRIVATE
            JUCE_WEB_BROWSER=0
            JUCE_STRICT_REFCOUNTEDPOINTER=1
            JUCE_VST3_CAN_REPLACE_VST2=0
            STEMHUB_DEFAULT_API_BASE_URL="${STEMHUB_API_BASE_URL}")

    target_link_libraries(${target}
        PRIVATE
            juce::juce_audio_utils
            juce::juce_cryptography
            juce::juce_recommended_config_flags
            juce::juce_recommended_lto_flags
            juce::juce_recommended_warning_flags)
endfunction()

set(STEMHUB_PLUGIN_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND STEMHUB_PLUGIN_FORMATS AU)
endif()

# NEEDS_CURL only matters on Linux, where JUCE links libcurl for HTTP; macOS and Windows use
# the system networking stack.
juce_add_plugin(stemhub
    COMPANY_NAME "Stemhub"
    BUNDLE_ID "com.stemhub.session"
    IS_SYNTH FALSE
    NEEDS_CURL TRUE
    NEEDS_WEB_BROWSER FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD ${COPY_PLUGIN}
    PLUGIN_MANUFACTURER_CODE Manu
    PLUGIN_CODE Wbcr
    VST3_CATEGORIES Fx Tools
    FORMATS ${STEMHUB_PLUGIN_FORMATS}
    PRODUCT_NAME "Stemhub Session")

juce_generate_juce_header(stemhub)

# Brand typefaces (SIL OFL 1.1, see Resources/fonts/OFL-*.txt), embedded so the UI renders identically in every DAW.
juce_add_binary_data(stemhub_assets
    HEADER_NAME StemhubAssets.h
    NAMESPACE StemhubAssets
    SOURCES
        Resources/fonts/Syne-ExtraBold.ttf
        Resources/fonts/PlusJakartaSans-Regular.ttf
        Resources/fonts/PlusJakartaSans-Medium.ttf
        Resources/fonts/PlusJakartaSans-SemiBold.ttf
        Resources/fonts/PlusJakartaSans-Bold.ttf)

target_sources(stemhub PRIVATE ${STEMHUB_CORE_SOURCES} ${STEMHUB_UI_SOURCES})
stemhub_configure_target(stemhub)
target_link_libraries(stemhub PRIVATE stemhub_assets)

juce_add_console_app(stemhub_plugin_tests NEEDS_CURL TRUE)

juce_generate_juce_header(stemhub_plugin_tests)

target_sources(stemhub_plugin_tests PRIVATE ${STEMHUB_CORE_SOURCES} tests/ProcessorStabilityTests.cpp)
stemhub_configure_target(stemhub_plugin_tests)

# The processor sources still include the plugin wrapper's settings; the tests supply them.
target_compile_definitions(stemhub_plugin_tests
    PRIVATE
        STEMHUB_PLUGIN_HEADLESS_TESTS=1
        JucePlugin_Name="Stemhub Session"
        JucePlugin_WantsMidiInput=0
        JucePlugin_ProducesMidiOutput=0
        JucePlugin_IsMidiEffect=0
        JucePlugin_IsSynth=0)
