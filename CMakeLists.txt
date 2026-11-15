# =============================================================================
#  HEM DARK MATTER SERIES - root build (C++23, no JUCE)
#  Ship target: Windows x64, MSVC, AVX2 (Intel Core i7-14700HX rig).
#  GCC/Clang are accepted only for developer test runs; releases come from MSVC CI.
# =============================================================================
cmake_minimum_required(VERSION 3.25)
project(HEMDarkMatterSeries VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

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

# ---- Tests ---------------------------------------------------------------------------------------------
enable_testing()
add_executable(dm_tests tests/main.cpp tests/AllocTracker.cpp tests/phantom_vox_tests.cpp tests/syndicate_tests.cpp tests/apex_tests.cpp tests/ironlock_tests.cpp)
target_link_libraries(dm_tests PRIVATE dm_phantom_vox dm_syndicate dm_apex dm_ironlock)
add_test(NAME dm_tests COMMAND dm_tests)
