cmake_minimum_required(VERSION 3.22)

project(Fuzzion5000 VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Never bake the local dependency sysroot into distributed binaries.
set(CMAKE_SKIP_RPATH TRUE)

option(FUZZION5000_BUILD_TESTS "Build the offline DSP test executable" ON)
option(FUZZION5000_BUILD_SCREENSHOT "Build the headless editor renderer" ON)
option(FUZZION5000_BUILD_HOST_TEST "Build the VST3 host smoke test" ON)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/external/JUCE/CMakeLists.txt")
    add_subdirectory(external/JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(
        JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.8
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

juce_add_binary_data(Fuzzion5000Assets
    SOURCES
        Resources/LatoRegular.ttf
        Resources/LatoSemibold.ttf
        Resources/LatoBlack.ttf
        Resources/Lato-OFL.txt)

set(FUZZION5000_SOURCES
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h
    Source/Parameters.h
    Source/Presets.h
    Source/dsp/FuzzCore.h
    Source/ui/Theme.cpp
    Source/ui/Theme.h
    Source/ui/StudioLookAndFeel.cpp
    Source/ui/StudioLookAndFeel.h
    Source/ui/Controls.cpp
    Source/ui/Controls.h
    Source/ui/WaveformDisplay.cpp
    Source/ui/WaveformDisplay.h
    Source/ui/TransferDisplay.cpp
    Source/ui/TransferDisplay.h)

set(FUZZION5000_DEFINITIONS
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_REPORT_APP_USAGE=0
    JUCE_STRICT_REFCOUNTEDPOINTER=1)

juce_add_plugin(Fuzzion5000
    COMPANY_NAME "BLS Audio"
    BUNDLE_ID "com.blsaudio.fuzzion5000"
    PLUGIN_MANUFACTURER_CODE Blsa
    PLUGIN_CODE Fz50
    FORMATS VST3 Standalone
    PRODUCT_NAME "Fuzzion-5000"
    DESCRIPTION "Transistor fuzz distortion with starve bias, octave and gate"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES "Fx")

target_sources(Fuzzion5000 PRIVATE ${FUZZION5000_SOURCES})
target_include_directories(Fuzzion5000 PRIVATE Source)
target_compile_definitions(Fuzzion5000 PUBLIC ${FUZZION5000_DEFINITIONS})
target_link_libraries(Fuzzion5000
    PRIVATE
        Fuzzion5000Assets
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)

if(FUZZION5000_BUILD_TESTS)
    enable_testing()

    juce_add_console_app(Fuzzion5000Tests PRODUCT_NAME "Fuzzion5000Tests")
    target_sources(Fuzzion5000Tests PRIVATE Tests/DspTests.cpp ${FUZZION5000_SOURCES})
    target_include_directories(Fuzzion5000Tests PRIVATE Source)
    target_compile_definitions(Fuzzion5000Tests PRIVATE ${FUZZION5000_DEFINITIONS})
    target_link_libraries(Fuzzion5000Tests
        PRIVATE
            Fuzzion5000Assets
            juce::juce_audio_utils
            juce::juce_dsp
            juce::juce_gui_extra
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_warning_flags)

    add_test(NAME fuzzion5000_dsp COMMAND $<TARGET_FILE:Fuzzion5000Tests>)
endif()

if(FUZZION5000_BUILD_HOST_TEST)
    juce_add_console_app(Fuzzion5000HostSmokeTest PRODUCT_NAME "Fuzzion5000HostSmokeTest")

    target_sources(Fuzzion5000HostSmokeTest PRIVATE Tools/HostSmokeTest.cpp)
    target_compile_definitions(Fuzzion5000HostSmokeTest PRIVATE
        ${FUZZION5000_DEFINITIONS}
        JUCE_PLUGINHOST_VST3=1)
    target_link_libraries(Fuzzion5000HostSmokeTest
        PRIVATE
            juce::juce_audio_utils
            juce::juce_gui_extra
            juce::juce_recommended_config_flags
            juce::juce_recommended_warning_flags)

    add_dependencies(Fuzzion5000HostSmokeTest Fuzzion5000_VST3)
endif()

if(FUZZION5000_BUILD_SCREENSHOT)
    juce_add_gui_app(Fuzzion5000Screenshot
        PRODUCT_NAME "Fuzzion5000Screenshot"
        COMPANY_NAME "BLS Audio"
        BUNDLE_ID "com.blsaudio.fuzzion5000.screenshot")

    target_sources(Fuzzion5000Screenshot PRIVATE ${FUZZION5000_SOURCES} Tools/RenderEditor.cpp)
    target_include_directories(Fuzzion5000Screenshot PRIVATE Source)
    target_compile_definitions(Fuzzion5000Screenshot PRIVATE ${FUZZION5000_DEFINITIONS} FUZZION5000_SCREENSHOT=1)
    target_link_libraries(Fuzzion5000Screenshot
        PRIVATE
            Fuzzion5000Assets
            juce::juce_audio_utils
            juce::juce_dsp
            juce::juce_gui_extra
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_warning_flags)
endif()
