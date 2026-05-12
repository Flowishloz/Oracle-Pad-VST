cmake_minimum_required(VERSION 3.22)
project(OraclePad VERSION 0.0.1)

add_subdirectory("E:/DEVELOPMENT/AI/JUCE" JUCE_build)

juce_add_plugin(OraclePad
    COMPANY_NAME "OracleAgents"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT TRUE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    # Disable JUCE's stubborn internal copy step
    COPY_PLUGIN_AFTER_BUILD FALSE 
    FORMATS VST3 Standalone
    PRODUCT_NAME "OraclePad")

target_sources(OraclePad PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/RadarComponent.cpp)

target_link_libraries(OraclePad
    PRIVATE
        juce::juce_audio_utils
        juce::juce_audio_devices
        juce::juce_audio_processors
        juce::juce_dsp
        juce::juce_gui_basics
        juce::juce_graphics
        juce::juce_events
        juce::juce_core
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# --- DEPLOYMENT: copy binary directly into the Ableton VST3 scan path ---
# Step 1: guarantee the destination folder exists (no error if it already does).
# Step 2: copy only the compiled DLL — faster than copy_directory and avoids
#         stale bundle artefacts confusing the host scanner on incremental builds.
add_custom_command(TARGET OraclePad_VST3 POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
            "E:/Music/Plugins/VST3/OraclePad.vst3/Contents/x86_64-win"
    COMMAND ${CMAKE_COMMAND} -E copy
            "$<TARGET_FILE:OraclePad_VST3>"
            "E:/Music/Plugins/VST3/OraclePad.vst3/Contents/x86_64-win/OraclePad.vst3"
    COMMENT "Deploying OraclePad.vst3 -> E:/Music/Plugins/VST3"
)

target_compile_definitions(OraclePad PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0  
)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)