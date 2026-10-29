cmake_minimum_required(VERSION 3.22)

# macOS: universal binaries (Apple Silicon + Intel) and a conservative minimum system.
# These variables must be set before project().
if(CMAKE_HOST_APPLE)
    if(NOT CMAKE_OSX_ARCHITECTURES)
        set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Build architectures for macOS" FORCE)
    endif()
    if(NOT CMAKE_OSX_DEPLOYMENT_TARGET)
        set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15" CACHE STRING "Minimum macOS version" FORCE)
    endif()
endif()

project(StudioFrosVinyl VERSION 0.7.1)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Windows (MSVC): static runtime, so users do not need the Visual C++ redistributable.
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

# Copy the VST3 to the user's plugin folder after building.
#   Linux: ~/.vst3    macOS: ~/Library/Audio/Plug-Ins/VST3
#   Windows: off by default (the system folder needs administrator rights);
#            use -DVST3_COPY_DIR=... together with -DSTUDIOFROS_COPY_AFTER_BUILD=ON to choose a folder.
if(WIN32)
    set(_studiofros_copy_default OFF)
else()
    set(_studiofros_copy_default ON)
endif()
option(STUDIOFROS_COPY_AFTER_BUILD "Copy the VST3 to the plugin folder after building" ${_studiofros_copy_default})

# JUCE is downloaded the first time you configure (internet needed)
include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.4
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(StudioFrosVinyl
    COMPANY_NAME "GuitarFros"
    PRODUCT_NAME "StudioFros - Vinyl"
    PLUGIN_MANUFACTURER_CODE GFro
    PLUGIN_CODE SFvn
    FORMATS VST3 Standalone
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    VST3_CATEGORIES Fx
    COPY_PLUGIN_AFTER_BUILD ${STUDIOFROS_COPY_AFTER_BUILD})

target_sources(StudioFrosVinyl PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

# The panel and switch pictures are embedded in the plugin
juce_add_binary_data(VinylAssets SOURCES
    Assets/background.jpg
    Assets/switch_up.png
    Assets/switch_center.png
    Assets/switch_down.png)

target_compile_definitions(StudioFrosVinyl PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(StudioFrosVinyl
    PRIVATE
        VinylAssets
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
