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

# The session and everything under it. No GUI or audio code, so the tests build it without either.
set(STEMHUB_CORE_SOURCES
    Source/src/application/CredentialStore.cpp
    Source/src/application/Log.cpp
    Source/src/application/PluginState.cpp
    Source/src/application/ProjectFileService.cpp
    Source/src/application/RestoreHandoff.cpp
    Source/src/application/SessionStorage.cpp
    Source/src/application/SnapshotFiles.cpp
    Source/src/application/SnapshotSync.cpp
    Source/src/application/StemhubSession.cpp
    Source/src/application/UseCases.cpp
    Source/src/domain/Manifest.cpp
    Source/src/network/ApiClient.cpp
    Source/src/network/ApiConfig.cpp
    Source/src/network/ApiJson.cpp)

# The host-facing processor, the editor and its views.
set(STEMHUB_PLUGIN_SOURCES
    Source/src/application/PluginProcessor.cpp
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
            JUCE_STRICT_REFCOUNTEDPOINTER=1
            STEMHUB_DEFAULT_API_BASE_URL="${STEMHUB_API_BASE_URL}")

    target_link_libraries(${target}
        PRIVATE
            juce::juce_cryptography
            juce::juce_events
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

target_sources(stemhub PRIVATE ${STEMHUB_CORE_SOURCES} ${STEMHUB_PLUGIN_SOURCES})
stemhub_configure_target(stemhub)
target_compile_definitions(stemhub PRIVATE JUCE_WEB_BROWSER=0 JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(stemhub PRIVATE stemhub_assets juce::juce_audio_utils)

set(STEMHUB_TEST_SOURCES
    tests/Main.cpp
    tests/support/TestSupport.cpp
    tests/ApiTests.cpp
    tests/PersistenceTests.cpp
    tests/SaveRestoreTests.cpp
    tests/SessionTests.cpp
    tests/SnapshotTests.cpp)

juce_add_console_app(stemhub_plugin_tests NEEDS_CURL TRUE)

juce_generate_juce_header(stemhub_plugin_tests)

target_sources(stemhub_plugin_tests PRIVATE ${STEMHUB_CORE_SOURCES} ${STEMHUB_TEST_SOURCES})
stemhub_configure_target(stemhub_plugin_tests)
target_include_directories(stemhub_plugin_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/tests")
# Failed assertions are checked and logged in Release too, through the test app's console logger.
target_compile_definitions(stemhub_plugin_tests PRIVATE JUCE_LOG_ASSERTIONS=1)

enable_testing()
add_test(NAME stemhub_plugin_tests COMMAND stemhub_plugin_tests)
# JUCE logs a failed assertion or a leaked object and carries on; either fails the run.
set_tests_properties(stemhub_plugin_tests PROPERTIES
    FAIL_REGULAR_EXPRESSION "JUCE Assertion failure;Leaked objects detected")
