cmake_minimum_required(VERSION 3.22)
project(ParametricEQ VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)

add_subdirectory(/Users/kushdeoghare/JUCE JUCE)

juce_add_gui_app(ParametricEQ
    PRODUCT_NAME "Parametric EQ"
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT "Parametric EQ needs mic access to process audio")

target_sources(ParametricEQ PRIVATE
    Source/Main.cpp
    Source/MainComponent.cpp)

target_compile_definitions(ParametricEQ PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0)

target_include_directories(ParametricEQ PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR}/ParametricEQ_artefacts/JuceLibraryCode)

juce_add_binary_data(Assets SOURCES
    Assets/knob.png
    Assets/display.png
    Assets/red-button.png
    Assets/orange-button.png
    Assets/green-button.png
    Assets/blue-button.png
    Assets/purple-button.png)

target_link_libraries(ParametricEQ PRIVATE
    Assets
    juce::juce_audio_utils
    juce::juce_dsp
    juce::juce_gui_basics)