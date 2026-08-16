cmake_minimum_required(VERSION 3.22)

project(DeltaSpine VERSION 0.1.1 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(DELTASPINE_BUILD_PLUGIN "Build the JUCE plug-in targets" ON)
option(DELTASPINE_BUILD_TESTS "Build tests" ON)
set(DELTASPINE_ARTIFACT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/artifacts" CACHE PATH
    "Directory for final VST3, AU, and Standalone products")
set(EHL_JUCE_SOURCE_DIR "" CACHE PATH "Existing JUCE source checkout; avoids network when set")

add_library(deltaspine_dsp STATIC
    Source/dsp/DeltaCoderCore.cpp)

target_include_directories(deltaspine_dsp PUBLIC Source)
target_compile_features(deltaspine_dsp PUBLIC cxx_std_17)

if(MSVC)
    target_compile_options(deltaspine_dsp PRIVATE /W4)
else()
    target_compile_options(deltaspine_dsp PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(DELTASPINE_BUILD_TESTS)
    enable_testing()
    add_executable(DeltaSpineTests Tests/TestMain.cpp)
    target_link_libraries(DeltaSpineTests PRIVATE deltaspine_dsp)
    add_test(NAME DeltaSpineDSPTests COMMAND DeltaSpineTests)
endif()

if(DELTASPINE_BUILD_PLUGIN)
    include(FetchContent)

    if(EHL_JUCE_SOURCE_DIR)
        if(NOT EXISTS "${EHL_JUCE_SOURCE_DIR}/CMakeLists.txt")
            message(FATAL_ERROR "EHL_JUCE_SOURCE_DIR does not contain JUCE: ${EHL_JUCE_SOURCE_DIR}")
        endif()
        add_subdirectory("${EHL_JUCE_SOURCE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/juce")
    elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/JUCE/CMakeLists.txt")
        add_subdirectory(JUCE)
    else()
        FetchContent_Declare(
            JUCE
            GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
            GIT_TAG 8.0.15
            GIT_SHALLOW TRUE)
        FetchContent_MakeAvailable(JUCE)
    endif()

    add_subdirectory(modules/juce-ehl-design-module)

    set(DELTASPINE_FORMATS VST3 Standalone)
    if(APPLE)
        list(APPEND DELTASPINE_FORMATS AU)
    endif()

    set(EHL_COPY_PLUGIN_AFTER_BUILD_DEFAULT OFF)
    if(APPLE AND NOT DEFINED ENV{CI})
        set(EHL_COPY_PLUGIN_AFTER_BUILD_DEFAULT ON)
    endif()
    option(EHL_COPY_PLUGIN_AFTER_BUILD
        "Copy built plug-in formats to the current user's standard plug-in folders"
        ${EHL_COPY_PLUGIN_AFTER_BUILD_DEFAULT})

    set(EHL_PLUGIN_COPY_ARGS
        COPY_PLUGIN_AFTER_BUILD ${EHL_COPY_PLUGIN_AFTER_BUILD})
    if(APPLE)
        list(APPEND EHL_PLUGIN_COPY_ARGS
            VST3_COPY_DIR "$ENV{HOME}/Library/Audio/Plug-Ins/VST3"
            AU_COPY_DIR "$ENV{HOME}/Library/Audio/Plug-Ins/Components")
    endif()

    juce_add_plugin(DeltaSpine
        VERSION ${PROJECT_VERSION}
        COMPANY_NAME "EsionHsrahLatigid"
        BUNDLE_ID "jp.ehl.deltaspine"
        PRODUCT_NAME "DeltaSpine"
        DESCRIPTION "Predictive delta coder fracture effect"
        PLUGIN_MANUFACTURER_CODE EHL_
        PLUGIN_CODE DlSp
        FORMATS ${DELTASPINE_FORMATS}
        IS_SYNTH FALSE
        NEEDS_MIDI_INPUT FALSE
        NEEDS_MIDI_OUTPUT FALSE
        IS_MIDI_EFFECT FALSE
        AU_SANDBOX_SAFE TRUE
        EDITOR_WANTS_KEYBOARD_FOCUS TRUE
        MICROPHONE_PERMISSION_ENABLED TRUE
        MICROPHONE_PERMISSION_TEXT "DeltaSpine requires microphone access to process live audio input."
        ${EHL_PLUGIN_COPY_ARGS})

    target_sources(DeltaSpine PRIVATE
        Source/ParameterIDs.h
        Source/PluginEditor.cpp
        Source/PluginEditor.h
        Source/PluginProcessor.cpp
        Source/PluginProcessor.h)

    target_compile_definitions(DeltaSpine PUBLIC
        JUCE_REPORT_APP_USAGE=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_WEB_BROWSER=0)

    target_link_libraries(DeltaSpine
        PRIVATE
            deltaspine_dsp
            EHL::JuceDesign
            juce::juce_audio_utils
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_lto_flags
            juce::juce_recommended_warning_flags)

    target_sources(DeltaSpine_Standalone PRIVATE Source/StandaloneApp.cpp)
    target_compile_definitions(DeltaSpine_Standalone PRIVATE JUCE_USE_CUSTOM_PLUGIN_STANDALONE_APP=1)

    set(deltaspine_stage_platform "linux-x64")
    if(APPLE)
        set(deltaspine_stage_platform "macos-arm64")
    elseif(WIN32)
        set(deltaspine_stage_platform "windows-x64")
    endif()
    set(deltaspine_juce_products "${CMAKE_CURRENT_BINARY_DIR}/DeltaSpine_artefacts/$<CONFIG>")
    set(deltaspine_staged_products "${DELTASPINE_ARTIFACT_DIR}/plugin-release/${deltaspine_stage_platform}")
    set(deltaspine_artifact_dependencies DeltaSpine_VST3 DeltaSpine_Standalone)
    set(deltaspine_artifact_commands
        COMMAND ${CMAKE_COMMAND} -E make_directory "${deltaspine_staged_products}/vst3"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${deltaspine_staged_products}/standalone"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${deltaspine_staged_products}/au")

    if(APPLE)
        list(APPEND deltaspine_artifact_commands
            COMMAND ${CMAKE_COMMAND} -E rm -rf "${deltaspine_staged_products}/vst3/deltaspine_vst3_plugin.vst3"
            COMMAND ${CMAKE_COMMAND} -E rm -rf "${deltaspine_staged_products}/standalone/deltaspine_standalone_plugin.app"
            COMMAND ${CMAKE_COMMAND} -E rm -rf "${deltaspine_staged_products}/au/deltaspine_au_plugin.component"
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                    "${deltaspine_juce_products}/VST3/DeltaSpine.vst3" "${deltaspine_staged_products}/vst3/deltaspine_vst3_plugin.vst3"
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                    "${deltaspine_juce_products}/Standalone/DeltaSpine.app" "${deltaspine_staged_products}/standalone/deltaspine_standalone_plugin.app")
    elseif(WIN32)
        list(APPEND deltaspine_artifact_commands
            COMMAND ${CMAKE_COMMAND} -E rm -rf "${deltaspine_staged_products}/vst3/deltaspine_vst3_plugin.vst3"
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                    "${deltaspine_juce_products}/VST3/DeltaSpine.vst3" "${deltaspine_staged_products}/vst3/deltaspine_vst3_plugin.vst3"
            COMMAND ${CMAKE_COMMAND} -E copy
                    "${deltaspine_juce_products}/Standalone/DeltaSpine.exe" "${deltaspine_staged_products}/standalone/deltaspine_standalone_plugin.exe")
    else()
        list(APPEND deltaspine_artifact_commands
            COMMAND ${CMAKE_COMMAND} -E rm -rf "${deltaspine_staged_products}/vst3/deltaspine_vst3_plugin.vst3"
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                    "${deltaspine_juce_products}/VST3/DeltaSpine.vst3" "${deltaspine_staged_products}/vst3/deltaspine_vst3_plugin.vst3"
            COMMAND ${CMAKE_COMMAND} -E copy
                    "${deltaspine_juce_products}/Standalone/DeltaSpine" "${deltaspine_staged_products}/standalone/deltaspine_standalone_plugin")
    endif()

    if(TARGET DeltaSpine_AU)
        list(APPEND deltaspine_artifact_dependencies DeltaSpine_AU)
        list(APPEND deltaspine_artifact_commands
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                    "${deltaspine_juce_products}/AU/DeltaSpine.component" "${deltaspine_staged_products}/au/deltaspine_au_plugin.component")
    endif()

    if(APPLE)
        list(APPEND deltaspine_artifact_commands
            COMMAND codesign --force --deep --sign -
                    "${deltaspine_staged_products}/vst3/deltaspine_vst3_plugin.vst3"
            COMMAND codesign --force --deep --sign -
                    "${deltaspine_staged_products}/standalone/deltaspine_standalone_plugin.app"
            COMMAND codesign --force --deep --sign -
                    "${deltaspine_staged_products}/au/deltaspine_au_plugin.component")
    endif()

    list(APPEND deltaspine_artifact_commands
        COMMAND ${CMAKE_COMMAND}
            -DPRODUCT_NAME=DeltaSpine
            -DVERSION=${PROJECT_VERSION}
            -DSLUG=deltaspine
            -DBUNDLE_ID=jp.ehl.deltaspine
            -DPLUGIN_CODE=DlSp
            -DSTAGE_DIR=${deltaspine_staged_products}
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/WriteArtifactsManifest.cmake")

    add_custom_target(ehl_stage_products
        ${deltaspine_artifact_commands}
        DEPENDS ${deltaspine_artifact_dependencies}
        COMMENT "Staging DeltaSpine products under ${DELTASPINE_ARTIFACT_DIR}/plugin-release")

    add_custom_target(DeltaSpine_Artifacts DEPENDS ehl_stage_products)

    if(DELTASPINE_BUILD_TESTS)
        add_executable(DeltaSpineIntegrationTests Tests/PluginIntegrationTest.cpp)
        target_include_directories(DeltaSpineIntegrationTests PRIVATE Source)
        target_link_libraries(DeltaSpineIntegrationTests PRIVATE DeltaSpine juce::juce_audio_utils)
        add_dependencies(DeltaSpineIntegrationTests DeltaSpine_Artifacts)

        juce_add_console_app(DeltaSpinePluginLoadTest
            PRODUCT_NAME "DeltaSpine Plugin Load Test")
        target_sources(DeltaSpinePluginLoadTest PRIVATE Tests/PluginLoadTest.cpp)
        target_include_directories(DeltaSpinePluginLoadTest PRIVATE Source)
        target_link_libraries(DeltaSpinePluginLoadTest PRIVATE
            juce::juce_audio_processors_headless
            juce::juce_recommended_config_flags
            juce::juce_recommended_warning_flags)
        target_compile_definitions(DeltaSpinePluginLoadTest PRIVATE
            JUCE_PLUGINHOST_VST3=1
            JUCE_USE_CURL=0
            JUCE_WEB_BROWSER=0)
        add_dependencies(DeltaSpinePluginLoadTest DeltaSpine_VST3)

        add_test(NAME DeltaSpinePluginIntegrationTests COMMAND DeltaSpineIntegrationTests)
        add_test(NAME DeltaSpineHostVST3
            COMMAND DeltaSpinePluginLoadTest
                    "${deltaspine_juce_products}/VST3/DeltaSpine.vst3"
                    "DeltaSpine")
        add_test(
            NAME DeltaSpineArtifactChecks
            COMMAND ${CMAKE_COMMAND}
                    -DSTAGE_DIR=${deltaspine_staged_products}
                    -DSLUG=deltaspine
                    -DEXPECT_VERSION=${PROJECT_VERSION}
                    -DEXPECT_AU=$<BOOL:${APPLE}>
                    -P ${CMAKE_CURRENT_SOURCE_DIR}/Tests/VerifyArtifacts.cmake)
    endif()
endif()
