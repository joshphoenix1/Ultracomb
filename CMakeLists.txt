cmake_minimum_required(VERSION 3.22)

project(UltraCombSAL VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Fetch JUCE
include(FetchContent)
FetchContent_Declare(
  JUCE
  GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
  GIT_TAG        8.0.4
  GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(UltraCombSAL
  COMPANY_NAME              "Homebrew"
  BUNDLE_ID                 "com.homebrew.ultracombsal"
  PLUGIN_MANUFACTURER_CODE  Hmbr
  PLUGIN_CODE               Ucsl
  FORMATS                   AU VST3
  PRODUCT_NAME              "UltraComb SAL"
  IS_SYNTH                  FALSE
  NEEDS_MIDI_INPUT          FALSE
  NEEDS_MIDI_OUTPUT         FALSE
  IS_MIDI_EFFECT            FALSE
  EDITOR_WANTS_KEYBOARD_FOCUS FALSE
  COPY_PLUGIN_AFTER_BUILD   FALSE
  AU_MAIN_TYPE              kAudioUnitType_Effect
  VST3_CATEGORIES           Fx
)

target_sources(UltraCombSAL PRIVATE
  src/PluginProcessor.cpp
  src/PluginProcessor.h
  src/PluginEditor.cpp
  src/PluginEditor.h
  src/XYPad.h
  src/ImperialLookAndFeel.h
)

target_compile_definitions(UltraCombSAL PUBLIC
  JUCE_WEB_BROWSER=0
  JUCE_USE_CURL=0
  JUCE_VST3_CAN_REPLACE_VST2=0
  JUCE_DISPLAY_SPLASH_SCREEN=0
)

target_link_libraries(UltraCombSAL
  PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_dsp
    juce::juce_gui_basics
  PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)
