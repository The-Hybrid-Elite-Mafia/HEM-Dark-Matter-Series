# =============================================================================
#  HEM DARK MATTER SERIES - root build (C++23, no JUCE)
#  Ship target: Windows x64, MSVC, AVX2 (Intel Core i7-14700HX rig).
#  GCC/Clang are accepted only for developer test runs; releases come from MSVC CI.
# =============================================================================
cmake_minimum_required(VERSION 3.25)
project(HEMDarkMatterSeries VERSION 0.3.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
# Static MSVC runtime everywhere: the VST3 SDK is built with it, and the installed plug-in then needs no
# separate Visual C++ runtime on the user's computer.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
set(CMAKE_POSITION_INDEPENDENT_CODE ON)   # static libraries go into the plug-in (a shared library)
option(DM_BUILD_VST3 "Build the VST3 bundle (fetches the Steinberg VST3 SDK at configure time)" OFF)

add_library(dm_build_flags INTERFACE)
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    target_compile_options(dm_build_flags INTERFACE
        /permissive- /Zc:__cplusplus /Zc:preprocessor /utf-8 /W4 /MP
        /arch:AVX2 /fp:precise /fp:contract
        $<$<CONFIG:Release>:/O2 /Oi /Ot /Gy /Gw>)
    target_compile_definitions(dm_build_flags INTERFACE NOMINMAX WIN32_LEAN_AND_MEAN _USE_MATH_DEFINES)
else()
    target_compile_options(dm_build_flags INTERFACE -Wall -Wextra -mavx2 -mfma $<$<CONFIG:Release>:-O2>)
endif()

# ---- Engines -------------------------------------------------------------------------------------------
add_library(dm_phantom_vox STATIC
    src/phantom_vox/PhantomVox.cpp
    src/phantom_vox/PhantomVoxParams.cpp
    src/phantom_vox/SfxUnit.cpp)
target_include_directories(dm_phantom_vox PUBLIC include)
target_link_libraries(dm_phantom_vox PUBLIC dm_build_flags)

add_library(dm_syndicate STATIC
    src/syndicate/Modules.cpp
    src/syndicate/SyndicateRack.cpp)
target_include_directories(dm_syndicate PUBLIC include)
target_link_libraries(dm_syndicate PUBLIC dm_build_flags)

add_library(dm_apex STATIC src/apex/ApexCore.cpp)
target_include_directories(dm_apex PUBLIC include)
target_link_libraries(dm_apex PUBLIC dm_build_flags)

add_library(dm_ironlock STATIC src/ironlock/IronLock.cpp)
target_include_directories(dm_ironlock PUBLIC include)
target_link_libraries(dm_ironlock PUBLIC dm_build_flags)

add_library(dm_rider STATIC src/rider/OutlawRider.cpp)
target_include_directories(dm_rider PUBLIC include)
target_link_libraries(dm_rider PUBLIC dm_build_flags)

add_library(dm_darkmatter STATIC src/darkmatter/DarkMatter4D.cpp)
target_include_directories(dm_darkmatter PUBLIC include)
target_link_libraries(dm_darkmatter PUBLIC dm_build_flags)

add_library(dm_chainbreaker STATIC src/chainbreaker/Chainbreaker108.cpp)
target_include_directories(dm_chainbreaker PUBLIC include)
target_link_libraries(dm_chainbreaker PUBLIC dm_build_flags)

# ---- Faceplates (Phase 2) ------------------------------------------------------------------------------
# Reused Hybrid Core renderer (third_party/hc_ui, same author, GPLv3).
set(HC_UI_SRC third_party/hc_ui/src/StbImpl.cpp third_party/hc_ui/src/Image.cpp third_party/hc_ui/src/Faceplate.cpp
    third_party/hc_ui/src/Parts.cpp third_party/hc_ui/src/FontAtlas.cpp third_party/hc_ui/src/Gl.cpp
    third_party/hc_ui/src/Renderer.cpp)
if(WIN32)
    list(APPEND HC_UI_SRC third_party/hc_ui/src/GlWindow.cpp)
endif()
add_library(hc_ui STATIC ${HC_UI_SRC})
target_include_directories(hc_ui PUBLIC third_party/hc_ui/include)
target_include_directories(hc_ui SYSTEM PUBLIC third_party/hc_ui/external/stb)
target_compile_features(hc_ui PUBLIC cxx_std_23)
if(WIN32)
    target_compile_definitions(hc_ui PUBLIC NOMINMAX WIN32_LEAN_AND_MEAN PRIVATE UNICODE _UNICODE)   # as in Hybrid Core
    target_compile_options(hc_ui PRIVATE /utf-8 /Zc:__cplusplus)
    target_link_libraries(hc_ui PUBLIC opengl32 user32 gdi32 shell32)
else()
    target_link_libraries(hc_ui PUBLIC /usr/lib/x86_64-linux-gnu/libGL.so.1 /usr/lib/x86_64-linux-gnu/libEGL.so.1)
endif()

# Faceplate model + interaction (no graphics code) and drawing.
file(GLOB DM_UI_LAYOUTS CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/src/ui/layouts/*.cpp)
# Faceplate model, interaction, layouts and engine bindings: no graphics, unit-tested on any machine.
add_library(dm_ui_core STATIC src/ui/PanelModel.cpp src/ui/PanelController.cpp src/ui/Faceplates.cpp ${DM_UI_LAYOUTS})
target_include_directories(dm_ui_core PUBLIC include)
target_link_libraries(dm_ui_core PUBLIC dm_build_flags dm_phantom_vox dm_syndicate dm_apex dm_ironlock dm_rider dm_darkmatter dm_chainbreaker)
# Plug-in catalogue + host binding (no plug-in SDK): shared by the VST3 wrapper and the tests.
add_library(dm_plugin_core STATIC src/plugin/Registry.cpp src/plugin/HostBinding.cpp)
target_include_directories(dm_plugin_core PUBLIC include)
target_link_libraries(dm_plugin_core PUBLIC dm_ui_core)
# Faceplate drawing on the Hybrid Core OpenGL renderer.
add_library(dm_ui STATIC src/ui/PanelView.cpp)
target_include_directories(dm_ui PUBLIC include)
target_link_libraries(dm_ui PUBLIC dm_ui_core hc_ui)

add_executable(dm_faceplate_snapshot tools/faceplate_snapshot/main.cpp)
target_link_libraries(dm_faceplate_snapshot PRIVATE dm_ui)
add_custom_command(TARGET dm_faceplate_snapshot POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/assets $<TARGET_FILE_DIR:dm_faceplate_snapshot>/assets)

# ---- Tests ---------------------------------------------------------------------------------------------
enable_testing()
add_executable(dm_tests tests/main.cpp tests/AllocTracker.cpp tests/phantom_vox_tests.cpp tests/syndicate_tests.cpp tests/apex_tests.cpp tests/ironlock_tests.cpp tests/rider_darkmatter_tests.cpp tests/chainbreaker_tests.cpp tests/faceplate_tests.cpp tests/plugin_tests.cpp tests/qa_report.cpp)
target_link_libraries(dm_tests PRIVATE dm_phantom_vox dm_syndicate dm_apex dm_ironlock dm_rider dm_darkmatter dm_chainbreaker dm_ui_core dm_plugin_core)
add_test(NAME dm_tests COMMAND dm_tests)

# ---- VST3 bundle ---------------------------------------------------------------------------------------
if(DM_BUILD_VST3)
    add_subdirectory(plugin)
endif()
