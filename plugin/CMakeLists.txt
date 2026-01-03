# FT2 Plugin CMake Configuration
# This builds the FT2 replayer as a VST3/AU/LV2 plugin using JUCE

cmake_minimum_required(VERSION 3.22)
project(FT2Plugin VERSION 1.0.23)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add JUCE
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../JUCE JUCE_Build)

# Core C replayer sources (the essential ones for audio)
# Uses ft2_tables_plugin.c instead of ft2_tables.c to avoid SDL2 dependency
set(FT2_CORE_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/ft2_instance.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/ft2_tables_plugin.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/plugin/ft2_plugin_replayer.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/plugin/ft2_plugin_loader.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/plugin/ft2_plugin_load_mod.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/plugin/ft2_plugin_load_s3m.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/plugin/ft2_plugin_interpolation.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/plugin/ft2_plugin_bmp.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/plugin/ft2_plugin_video.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/plugin/ft2_plugin_pushbuttons.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/plugin/ft2_plugin_scrollbars.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/plugin/ft2_plugin_checkboxes.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/plugin/ft2_plugin_radiobuttons.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/plugin/ft2_plugin_layout.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/plugin/ft2_plugin_instrsw.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/plugin/ft2_plugin_callbacks.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/plugin/ft2_plugin_widgets.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/plugin/ft2_plugin_scopes.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/plugin/ft2_plugin_pattern_ed.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/plugin/ft2_plugin_sample_ed.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/plugin/ft2_plugin_dialog.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/plugin/ft2_plugin_modal_panels.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/plugin/ft2_plugin_volume_panel.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/plugin/ft2_plugin_resample_panel.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/plugin/ft2_plugin_echo_panel.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/plugin/ft2_plugin_mix_panel.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/plugin/ft2_plugin_wave_panel.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/plugin/ft2_plugin_filter_panel.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/plugin/ft2_plugin_smpfx.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/plugin/ft2_plugin_instr_ed.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/plugin/ft2_plugin_diskop.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/plugin/ft2_plugin_input.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/plugin/ft2_plugin_ui.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/plugin/ft2_plugin_about.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/plugin/ft2_plugin_textbox.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/plugin/ft2_plugin_config.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/plugin/ft2_plugin_help.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/plugin/ft2_plugin_gui.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/plugin/ft2_plugin_palette.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/plugin/ft2_plugin_trim.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/plugin/ft2_plugin_nibbles.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/plugin/ft2_plugin_timemap.c
)

# Plugin sources
set(PLUGIN_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/PluginProcessor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/PluginProcessor.h
    ${CMAKE_CURRENT_SOURCE_DIR}/PluginEditor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/PluginEditor.h
    ${CMAKE_CURRENT_SOURCE_DIR}/UpdateChecker.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/UpdateChecker.h
)

# Create the plugin
juce_add_plugin(FT2Plugin
    COMPANY_NAME "Blamstrain"
    PLUGIN_MANUFACTURER_CODE Blam
    PLUGIN_CODE Ft2P
    FORMATS VST3 AU LV2
    PRODUCT_NAME "Fasttracker II"
    BUNDLE_ID "com.blamstrain.ft2plugin"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE
    VST3_CATEGORIES "Instrument" "Synth"
    AU_MAIN_TYPE "kAudioUnitType_MusicDevice"
    LV2URI "urn:blamstrain:ft2plugin"
)

# Add FT2 core as a static library to interface C code with C++
add_library(ft2_core STATIC ${FT2_CORE_SOURCES})
target_include_directories(ft2_core PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/plugin
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/mixer
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/gfxdata
)
set_target_properties(ft2_core PROPERTIES
    C_STANDARD 11
    C_STANDARD_REQUIRED ON
    POSITION_INDEPENDENT_CODE ON
)
target_compile_definitions(ft2_core PRIVATE
    FT2_PLUGIN_VERSION="${PROJECT_VERSION}"
)

# Link the plugin
target_sources(FT2Plugin PRIVATE ${PLUGIN_SOURCES})

target_include_directories(FT2Plugin PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/plugin
)

target_compile_definitions(FT2Plugin
    PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    PRIVATE
    FT2_PLUGIN_VERSION="${PROJECT_VERSION}"
)

# MSVC: Generate map file and debug symbols for crash debugging
if(MSVC)
    target_compile_options(ft2_core PRIVATE /Zi)
    target_compile_options(FT2Plugin PRIVATE /Zi)
    target_link_options(FT2Plugin PRIVATE /MAP /DEBUG)
endif()

target_link_libraries(FT2Plugin
    PRIVATE
    ft2_core
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_dsp
    juce::juce_opengl
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)

