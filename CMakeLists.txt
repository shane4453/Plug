cmake_minimum_required(VERSION 3.22)
project(MyGain VERSION 0.1.0)

# 첫 빌드 시 JUCE를 자동으로 다운로드합니다 (몇 분 걸림, 한 번만)
include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.4
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(MyGain
    COMPANY_NAME "Shane"
    PLUGIN_MANUFACTURER_CODE Shne
    PLUGIN_CODE Gn01
    FORMATS AU VST3 Standalone      # Ableton은 macOS에서 AU/VST3 둘 다 지원
    PRODUCT_NAME "Shane Dist"
    COPY_PLUGIN_AFTER_BUILD TRUE)   # 빌드하면 자동으로 플러그인 폴더에 설치

target_sources(MyGain PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

# UI 배경 이미지를 플러그인 바이너리에 내장
juce_add_binary_data(DistAssets SOURCES
    Assets/background.png)

target_compile_features(MyGain PRIVATE cxx_std_17)

target_compile_definitions(MyGain PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(MyGain
    PRIVATE
        DistAssets
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
