cmake_minimum_required(VERSION 3.22)

project(BrownHelp VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

include(FetchContent)

FetchContent_Declare(
    juce
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.10
)

FetchContent_MakeAvailable(juce)

juce_add_plugin(BrownHelp
    COMPANY_NAME "D35P Audio"
    COMPANY_WEBSITE "https://d35p.audio"
    COMPANY_EMAIL "hello@d35p.audio"
    BUNDLE_ID "audio.d35p.brownhelp"
    PLUGIN_MANUFACTURER_CODE D35P
    PLUGIN_CODE BrHp
    FORMATS VST3
    PRODUCT_NAME "Brown Help"
    DESCRIPTION "Podcast spectral balancer toward a tilted brown-noise curve"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Fx EQ
)

target_sources(BrownHelp
    PRIVATE
        Source/BrownCurveBalancer.cpp
        Source/BrownHelpProcessor.cpp
        Source/BrownHelpEditor.cpp
        Source/BrownHelpUiComponents.cpp
)

target_compile_definitions(BrownHelp
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(BrownHelp
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

option(BROWN_HELP_BUILD_TESTS "Build Brown Help tests" ON)

if(BROWN_HELP_BUILD_TESTS)
    enable_testing()

    add_executable(BrownHelpTests
        Tests/BrownCurveBalancerTests.cpp
        Source/BrownCurveBalancer.cpp
    )

    target_link_libraries(BrownHelpTests
        PRIVATE
            juce::juce_dsp
            juce::juce_audio_basics
            juce::juce_core
    )

    target_include_directories(BrownHelpTests PRIVATE Source)

    add_test(NAME BrownHelpTests COMMAND BrownHelpTests)
endif()
