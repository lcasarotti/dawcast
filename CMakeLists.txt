cmake_minimum_required(VERSION 3.22)

set(CMAKE_POLICY_VERSION_MINIMUM 3.5)

project(DawCast VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Download dependencies using FetchContent
include(FetchContent)

# 1. JUCE framework
message(STATUS "Fetching JUCE 7...")
FetchContent_Declare(
    juce
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 7.0.12
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(juce)

# 2. LAME MP3 Encoder
message(STATUS "Fetching libmp3lame...")
FetchContent_Declare(
    libmp3lame
    GIT_REPOSITORY https://github.com/Iunusov/libmp3lame-CMAKE.git
    GIT_TAG master
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(libmp3lame)

# 3. Ogg Vorbis Encoder
message(STATUS "Fetching OGG-Vorbis...")
FetchContent_Declare(
    ogg_vorbis
    GIT_REPOSITORY https://github.com/Iunusov/OGG-Vorbis-CMAKE.git
    GIT_TAG master
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(ogg_vorbis)

# Create the Audio Plugin
juce_add_plugin(DawCast
    VERSION 1.0.0
    COMPANY_NAME "LucaCasarotti"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD FALSE
    PLUGIN_MANUFACTURER_CODE LcCs
    PLUGIN_CODE DwCs
    FORMATS VST3 AU AUv3
    PRODUCT_NAME "DawCast"
)

# Compile options
target_compile_definitions(DawCast PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

# Source files
target_sources(DawCast PRIVATE
    Source/PluginProcessor.h
    Source/PluginProcessor.cpp
    Source/PluginEditor.h
    Source/PluginEditor.cpp
    Source/IcecastClient.h
    Source/IcecastClient.cpp
    Source/AudioEncoder.h
    Source/Mp3Encoder.h
    Source/Mp3Encoder.cpp
    Source/OggEncoder.h
    Source/OggEncoder.cpp
)

target_include_directories(DawCast PRIVATE
    Source
)

# Link libraries
# note: 'vorbis' contains libogg and libvorbis (including encoder) from the repo we fetch.
# 'mp3lame' is the target from libmp3lame-CMAKE.
target_link_libraries(DawCast PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_gui_basics
    mp3lame
    vorbis
)
# link libraries definitions are inherited automatically from JUCE targets
