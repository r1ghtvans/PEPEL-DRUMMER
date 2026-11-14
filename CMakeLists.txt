cmake_minimum_required(VERSION 3.22)

# Статическая C++ библиотека на Windows: плагину не нужен Visual C++ Redistributable
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

project(PepelDrummer VERSION 0.3.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(PEPEL_BUILD_TOOLS "Build DSP test and UI snapshot tools" OFF)

include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.4
    GIT_SHALLOW    ON)
FetchContent_MakeAvailable(JUCE)

# Шрифты: три слота в Assets/ (FontTitle, FontText, FontKanji), их можно заменить своими
juce_add_binary_data(PepelAssets SOURCES
    Assets/FontTitle.ttf
    Assets/FontText.ttf
    Assets/FontKanji.ttf)

juce_add_plugin(PepelDrummer
    COMPANY_NAME "PEPEL"
    PLUGIN_MANUFACTURER_CODE Pepl
    PLUGIN_CODE Pdrm
    FORMATS VST3
    PRODUCT_NAME "PEPEL DRUMMER"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Instrument Sampler)

target_sources(PepelDrummer PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(PepelDrummer PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

juce_generate_juce_header(PepelDrummer)

# для быстрых проверок LTO отключаем
if(PEPEL_BUILD_TOOLS)
    set(PEPEL_LTO "")
else()
    set(PEPEL_LTO juce::juce_recommended_lto_flags)
endif()

target_link_libraries(PepelDrummer
    PRIVATE
        PepelAssets
        juce::juce_audio_utils
        juce::juce_audio_formats
    PUBLIC
        juce::juce_recommended_config_flags
        ${PEPEL_LTO}
        juce::juce_recommended_warning_flags)

# Необязательные инструменты: проверка DSP без DAW и снимок интерфейса в PNG
if(PEPEL_BUILD_TOOLS)
    foreach(tool DspTest Snapshot)
        juce_add_console_app(PepelDrummer_${tool} PRODUCT_NAME "PepelDrummer_${tool}")
        target_sources(PepelDrummer_${tool} PRIVATE Tools/${tool}.cpp)
        target_compile_definitions(PepelDrummer_${tool} PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
        juce_generate_juce_header(PepelDrummer_${tool})
        target_link_libraries(PepelDrummer_${tool} PRIVATE PepelDrummer PepelAssets
            juce::juce_audio_utils juce::juce_audio_formats)
    endforeach()
endif()
