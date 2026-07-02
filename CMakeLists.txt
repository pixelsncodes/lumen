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

# /W4 /WX on project sources only; JUCE module code keeps its own warning setup.
function(lumen_strict_sources)
    if(MSVC)
        set_source_files_properties(${ARGV} PROPERTIES COMPILE_OPTIONS "/W4;/WX")
    endif()
endfunction()

# ---------------------------------------------------------------------------
# lumen_render — headless render/verification harness (console, no GUI)
# ---------------------------------------------------------------------------
juce_add_console_app(lumen_render PRODUCT_NAME "lumen_render")

set(LUMEN_RENDER_SOURCES
    Tools/Render/RenderMain.cpp
)
target_sources(lumen_render PRIVATE ${LUMEN_RENDER_SOURCES})
lumen_strict_sources(${LUMEN_RENDER_SOURCES})

target_compile_definitions(lumen_render PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
)
target_link_libraries(lumen_render PRIVATE
    juce::juce_audio_formats
    juce::juce_recommended_config_flags
)
