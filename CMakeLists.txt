cmake_minimum_required(VERSION 3.22)

project(BrownHelp VERSION 0.3.0 LANGUAGES C CXX)

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
    FORMATS AU VST3
    PRODUCT_NAME "Brown Help"
    DESCRIPTION "Automatic podcast loudness and voice-band engineer"
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
        Source/VoiceEngineer.cpp
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

if(APPLE)
    # JUCE's VST3 manifest helper updates moduleinfo.json after the initial ad-hoc
    # signature. Sign once more at the very end so the produced bundle verifies.
    add_custom_command(TARGET BrownHelp_VST3 POST_BUILD
        COMMAND codesign --force --deep --sign - "$<TARGET_BUNDLE_DIR:BrownHelp_VST3>"
        COMMENT "Re-signing Brown Help after VST3 manifest generation"
        VERBATIM
    )

    add_custom_command(TARGET BrownHelp_AU POST_BUILD
        COMMAND codesign --force --deep --sign - "$<TARGET_BUNDLE_DIR:BrownHelp_AU>"
        COMMENT "Re-signing Brown Help Audio Unit"
        VERBATIM
    )
endif()

option(BROWN_HELP_BUILD_TESTS "Build Brown Help tests" ON)
option(BROWN_HELP_BUILD_SCREENSHOT_TOOL "Build the Brown Help UI screenshot helper" OFF)

if(BROWN_HELP_BUILD_TESTS)
    enable_testing()

    add_executable(BrownHelpTests
        Tests/VoiceEngineerTests.cpp
        Source/VoiceEngineer.cpp
    )

    target_link_libraries(BrownHelpTests
        PRIVATE
            juce::juce_dsp
            juce::juce_audio_basics
            juce::juce_core
    )

    target_include_directories(BrownHelpTests PRIVATE Source)

    target_compile_definitions(BrownHelpTests
        PUBLIC
            JUCE_WEB_BROWSER=0
            JUCE_USE_CURL=0
            JUCE_VST3_CAN_REPLACE_VST2=0
    )

    add_test(NAME BrownHelpTests COMMAND BrownHelpTests)

    add_executable(BrownHelpProcessorTests
        Tests/BrownHelpProcessorTests.cpp
    )

    target_include_directories(BrownHelpProcessorTests
        PRIVATE
            Source
            ${juce_SOURCE_DIR}/modules
    )
    target_link_libraries(BrownHelpProcessorTests PRIVATE BrownHelp)

    add_test(NAME BrownHelpProcessorTests COMMAND BrownHelpProcessorTests)
endif()

if(BROWN_HELP_BUILD_SCREENSHOT_TOOL)
    add_executable(BrownHelpUiScreenshot
        Tools/RenderPluginUi.cpp
        Source/VoiceEngineer.cpp
        Source/BrownHelpProcessor.cpp
        Source/BrownHelpEditor.cpp
        Source/BrownHelpUiComponents.cpp
    )

    target_include_directories(BrownHelpUiScreenshot PRIVATE Source)

    target_compile_definitions(BrownHelpUiScreenshot
        PUBLIC
            JUCE_WEB_BROWSER=0
            JUCE_USE_CURL=0
            JUCE_VST3_CAN_REPLACE_VST2=0
            JucePlugin_Name="Brown Help"
    )

    target_link_libraries(BrownHelpUiScreenshot
        PRIVATE
            juce::juce_audio_utils
            juce::juce_dsp
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_lto_flags
            juce::juce_recommended_warning_flags
    )
endif()
