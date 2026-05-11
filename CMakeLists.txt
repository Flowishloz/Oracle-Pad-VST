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
    Source/PluginEditor.cpp)

target_link_libraries(OraclePad
    PRIVATE
        juce::juce_audio_utils
        juce::juce_audio_devices
        juce::juce_audio_processors
        juce::juce_gui_basics
        juce::juce_graphics
        juce::juce_events
        juce::juce_core
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# --- THE MANUAL FORCE-COPY STEP ---
# This executes after the VST3 target is built.
add_custom_command(TARGET OraclePad_VST3 POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "$<TARGET_FILE_DIR:OraclePad_VST3>/../.." 
    "E:/Music/Plugins/VST3/OraclePad.vst3"
    COMMENT "Force-copying VST3 to E: drive..."
)

target_compile_definitions(OraclePad PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0  
)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)