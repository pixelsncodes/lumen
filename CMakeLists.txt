cmake_minimum_required(VERSION 3.25)

project(Lumen VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# JUCE, pinned. Record tag changes in CLAUDE.md.
include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.14
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(JUCE)

# /W4 /WX (+ correct UTF-8 source decoding) on project sources only;
# JUCE module code keeps its own warning setup.
function(lumen_strict_sources)
    if(MSVC)
        set_source_files_properties(${ARGV} PROPERTIES COMPILE_OPTIONS "/W4;/WX;/utf-8")
    endif()
endfunction()

set(LUMEN_COMMON_DEFS
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
)

# ---------------------------------------------------------------------------
# Embedded assets: Inter (SIL OFL; license ships alongside, SPEC section 14)
# ---------------------------------------------------------------------------
juce_add_binary_data(LumenAssets SOURCES
    Assets/Fonts/InterRegular.ttf
    Assets/Fonts/InterMedium.ttf
    Assets/Fonts/InterSemiBold.ttf
    Assets/Fonts/InterLicenseOFL.txt
)

# ---------------------------------------------------------------------------
# Lumen — the plugin (VST3 + Standalone)
# ---------------------------------------------------------------------------
juce_add_plugin(Lumen
    COMPANY_NAME                "Lumen Audio"
    PLUGIN_MANUFACTURER_CODE    Lumo
    PLUGIN_CODE                 Lmn1
    FORMATS                     VST3 Standalone
    PRODUCT_NAME                "Lumen"
    IS_SYNTH                    TRUE
    NEEDS_MIDI_INPUT            TRUE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    VST3_CATEGORIES             "Instrument Synth"
    COPY_PLUGIN_AFTER_BUILD     FALSE   # done by cmake/CopyVST3.cmake, best effort
)

# Engine + state + Lens sources shared by the plugin, render harness, and
# tests. Source/Lens needs juce_graphics (image decode) + juce_cryptography
# (SHA-256 checksums) — both already/newly linked by every target.
set(LUMEN_ENGINE_SOURCES
    Source/Engine/Wavetable.cpp
    Source/Engine/FactoryTables.cpp
    Source/Engine/Voice.cpp
    Source/Engine/FxChain.cpp
    Source/Engine/SynthEngine.cpp
    Source/Lens/LensEngine.cpp
    Source/Lens/TestImages.cpp
    Source/State/Parameters.cpp
    Source/State/EngineBindings.cpp
    Source/State/ModState.cpp
    Source/State/LensState.cpp
)
set(LUMEN_ENGINE_HEADERS
    Source/Engine/EngineParams.h
    Source/Engine/Envelope.h
    Source/Engine/FactoryTables.h
    Source/Engine/FxChain.h
    Source/Engine/Lfo.h
    Source/Engine/ModDestinations.h
    Source/Engine/ModMatrix.h
    Source/Engine/SVF.h
    Source/Engine/SynthEngine.h
    Source/Engine/Voice.h
    Source/Engine/Wavetable.h
    Source/Lens/LensEngine.h
    Source/Lens/TestImages.h
    Source/State/EngineBindings.h
    Source/State/LensState.h
    Source/State/ModState.h
    Source/State/Parameters.h
)

set(LUMEN_PLUGIN_SOURCES
    Source/PluginProcessor.cpp
    Source/Lens/LensController.cpp
    Source/UI/LumenLookAndFeel.cpp
    Source/UI/Controls.cpp
    Source/UI/Visualizers.cpp
    Source/UI/LensPanel.cpp
    Source/UI/Cards.cpp
    Source/UI/PluginEditor.cpp
    ${LUMEN_ENGINE_SOURCES}
)
target_sources(Lumen PRIVATE
    ${LUMEN_PLUGIN_SOURCES}
    Source/PluginProcessor.h
    Source/Lens/LensController.h
    Source/UI/Theme.h
    Source/UI/LumenLookAndFeel.h
    Source/UI/Controls.h
    Source/UI/WaterfallModel.h
    Source/UI/Visualizers.h
    Source/UI/LensPanel.h
    Source/UI/Cards.h
    Source/UI/PluginEditor.h
    Source/Engine/UiTap.h
    ${LUMEN_ENGINE_HEADERS}
)
lumen_strict_sources(${LUMEN_PLUGIN_SOURCES})

target_include_directories(Lumen PUBLIC Source)
target_compile_definitions(Lumen PUBLIC
    ${LUMEN_COMMON_DEFS}
    JUCE_VST3_CAN_REPLACE_VST2=0
)
target_link_libraries(Lumen
    PRIVATE juce::juce_audio_utils juce::juce_dsp juce::juce_opengl
            juce::juce_cryptography LumenAssets
    PUBLIC  juce::juce_recommended_config_flags
)

# Custom standalone app: adds --screenshot / --version (SPEC section 18).
target_sources(Lumen_Standalone PRIVATE Source/Standalone/StandaloneApp.cpp)
target_compile_definitions(Lumen_Standalone PRIVATE JUCE_USE_CUSTOM_PLUGIN_STANDALONE_APP=1)
lumen_strict_sources(Source/Standalone/StandaloneApp.cpp)

# Best-effort copy into the system VST3 folder; warns (never fails) on denial.
set(LUMEN_VST3_DEST "C:/Program Files/Common Files/VST3")
add_custom_command(TARGET Lumen_VST3 POST_BUILD
    COMMAND ${CMAKE_COMMAND}
        -DSRC=$<TARGET_FILE_DIR:Lumen_VST3>/../..
        -DDEST=${LUMEN_VST3_DEST}
        -P ${CMAKE_SOURCE_DIR}/cmake/CopyVST3.cmake
    COMMENT "Copying Lumen.vst3 to ${LUMEN_VST3_DEST} (best effort)"
    VERBATIM
)

# ---------------------------------------------------------------------------
# lumen_render — headless render/verification harness (console, no GUI)
# ---------------------------------------------------------------------------
juce_add_console_app(lumen_render PRODUCT_NAME "lumen_render")

set(LUMEN_RENDER_SOURCES
    Tools/Render/RenderMain.cpp
    ${LUMEN_ENGINE_SOURCES}
)
target_sources(lumen_render PRIVATE ${LUMEN_RENDER_SOURCES})
lumen_strict_sources(Tools/Render/RenderMain.cpp)

target_include_directories(lumen_render PRIVATE Source)
target_compile_definitions(lumen_render PRIVATE ${LUMEN_COMMON_DEFS})
target_link_libraries(lumen_render PRIVATE
    juce::juce_audio_formats
    juce::juce_audio_processors
    juce::juce_cryptography
    juce::juce_dsp
    juce::juce_graphics
    juce::juce_recommended_config_flags
)

# ---------------------------------------------------------------------------
# lumen_tests — unit test runner (console)
# ---------------------------------------------------------------------------
juce_add_console_app(lumen_tests PRODUCT_NAME "lumen_tests")

set(LUMEN_TESTS_SOURCES
    Tools/Tests/TestsMain.cpp
    ${LUMEN_ENGINE_SOURCES}
)
target_sources(lumen_tests PRIVATE ${LUMEN_TESTS_SOURCES})
lumen_strict_sources(Tools/Tests/TestsMain.cpp)

target_include_directories(lumen_tests PRIVATE Source)
target_compile_definitions(lumen_tests PRIVATE ${LUMEN_COMMON_DEFS})
target_link_libraries(lumen_tests PRIVATE
    juce::juce_audio_processors
    juce::juce_cryptography
    juce::juce_dsp
    juce::juce_graphics
    juce::juce_recommended_config_flags
)
