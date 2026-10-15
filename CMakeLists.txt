cmake_minimum_required(VERSION 3.22)
project(PastelDrumSampler VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Offline builds: -DFETCHCONTENT_SOURCE_DIR_JUCE=/path/to/JUCE reuses a local checkout
include(FetchContent)
FetchContent_Declare(
  JUCE
  GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
  GIT_TAG 8.0.6
  GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(PastelDrumSampler
    COMPANY_NAME "Pastel Drum Synth"
    BUNDLE_ID "com.pasteldrumsynth.pasteldrumsampler"
    DESCRIPTION "16-pad drum sampler: 4 layers per pad, macros, per-pad + global FX, multi-out"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    PLUGIN_MANUFACTURER_CODE Past
    PLUGIN_CODE PaS1
    VST3_CATEGORIES Instrument Drum Sampler
    FORMATS VST3 Standalone
    PRODUCT_NAME "Pastel Drum Sampler"
)

juce_generate_juce_header(PastelDrumSampler)

set(SAMPLER_CORE_SOURCES
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/PresetBrowser.cpp
    Source/dsp/SampleEngine.cpp
    Source/dsp/Effects.cpp
    Source/Presets.cpp
    Source/PresetFiles.cpp
    Source/FileLibrary.cpp
)
set(SAMPLER_DEFS
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_USE_MP3AUDIOFORMAT=1
)

target_sources(PastelDrumSampler PRIVATE ${SAMPLER_CORE_SOURCES})
target_include_directories(PastelDrumSampler PRIVATE Source Source/dsp)
target_compile_definitions(PastelDrumSampler PRIVATE ${SAMPLER_DEFS})
target_link_libraries(PastelDrumSampler PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)

# Headless dev tools (never shipped): E2E test suite, offscreen UI snapshots,
# FactoryBuild: tools/kitgen manifest -> Factory/ kit XMLs
set(TOOL_E2ETest TestE2E.cpp)
set(TOOL_UiShot UiShot.cpp)
set(TOOL_FactoryBuild FactoryBuild.cpp)
foreach(tool E2ETest UiShot FactoryBuild)
    add_executable(${tool} ${TOOL_${tool}} ${SAMPLER_CORE_SOURCES})
    target_include_directories(${tool} PRIVATE Source Source/dsp
        ${CMAKE_CURRENT_BINARY_DIR}/PastelDrumSampler_artefacts/JuceLibraryCode)
    target_link_libraries(${tool} PRIVATE juce::juce_audio_utils juce::juce_audio_processors juce::juce_audio_formats
        juce::juce_gui_basics juce::juce_dsp juce::juce_audio_basics juce::juce_graphics
        juce::juce_data_structures juce::juce_events juce::juce_core
        juce::juce_recommended_config_flags)
    target_compile_definitions(${tool} PRIVATE ${SAMPLER_DEFS} JUCE_MODAL_LOOPS_PERMITTED=1 JUCE_STANDALONE_APPLICATION=0)
    add_dependencies(${tool} PastelDrumSampler) # JuceHeader.h is generated at build time by the plugin target
endforeach()

# Ship factory kits (+ their samples) inside the VST3 bundle + next to the standalone
add_custom_target(PastelSamplerFactory ALL
    COMMAND ${CMAKE_COMMAND} -E remove_directory
        "${CMAKE_CURRENT_BINARY_DIR}/PastelDrumSampler_artefacts/$<CONFIG>/VST3/Pastel Drum Sampler.vst3/Contents/Resources/Factory"
    COMMAND ${CMAKE_COMMAND} -E remove_directory
        "${CMAKE_CURRENT_BINARY_DIR}/PastelDrumSampler_artefacts/$<CONFIG>/Standalone/Factory"
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "${CMAKE_CURRENT_BINARY_DIR}/PastelDrumSampler_artefacts/$<CONFIG>/VST3/Pastel Drum Sampler.vst3/Contents/Resources/Factory"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/Factory"
        "${CMAKE_CURRENT_BINARY_DIR}/PastelDrumSampler_artefacts/$<CONFIG>/VST3/Pastel Drum Sampler.vst3/Contents/Resources/Factory"
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "${CMAKE_CURRENT_BINARY_DIR}/PastelDrumSampler_artefacts/$<CONFIG>/Standalone/Factory"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/Factory"
        "${CMAKE_CURRENT_BINARY_DIR}/PastelDrumSampler_artefacts/$<CONFIG>/Standalone/Factory"
)
add_dependencies(PastelSamplerFactory PastelDrumSampler)

# HostCheck: loads the built .vst3 through JUCE's VST3 hosting (dev only)
add_executable(HostCheck HostCheck.cpp)
target_include_directories(HostCheck PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/PastelDrumSampler_artefacts/JuceLibraryCode)
target_link_libraries(HostCheck PRIVATE juce::juce_audio_utils juce::juce_audio_processors juce::juce_recommended_config_flags)
target_compile_definitions(HostCheck PRIVATE JUCE_PLUGINHOST_VST3=1 JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_STANDALONE_APPLICATION=1)
add_dependencies(HostCheck PastelDrumSampler)
