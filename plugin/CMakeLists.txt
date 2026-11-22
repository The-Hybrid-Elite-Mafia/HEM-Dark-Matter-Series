# =====================================================================================================
#  HEM Dark Matter Series - VST3 bundle (DM_BUILD_VST3=ON): all 13 plug-ins in "HEM Dark Matter.vst3".
#  Steinberg VST3 SDK v3.8.1 (MIT licence) is fetched at configure time, never committed. VSTGUI and the
#  SDK examples are not built. Steinberg's validator is built and run on the bundle after it links.
#  Faceplates (Windows): fonts + knob parts ship in the bundle's Contents/Resources.
# =====================================================================================================
include(FetchContent)

set(SMTG_ENABLE_VST3_PLUGIN_EXAMPLES  OFF CACHE BOOL "" FORCE)
set(SMTG_ENABLE_VST3_HOSTING_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SMTG_ENABLE_VSTGUI_SUPPORT        OFF CACHE BOOL "" FORCE)
set(SMTG_CREATE_PLUGIN_LINK           OFF CACHE BOOL "" FORCE)   # no link into Common Files (needs admin)
set(SMTG_RUN_VST_VALIDATOR            ON  CACHE BOOL "" FORCE)
set(SMTG_USE_STATIC_CRT               ON  CACHE BOOL "" FORCE)   # matches CMAKE_MSVC_RUNTIME_LIBRARY

# The SDK is C++17 code: build it as C++17 (this directory only); the plug-in target stays C++23.
set(CMAKE_CXX_STANDARD 17)

FetchContent_Declare(vst3sdk
    GIT_REPOSITORY https://github.com/steinbergmedia/vst3sdk.git
    GIT_TAG        v3.8.1_build_84
    GIT_SHALLOW    TRUE
    GIT_SUBMODULES base cmake pluginterfaces public.sdk
    GIT_PROGRESS   FALSE)
FetchContent_MakeAvailable(vst3sdk)

set(DM_VST3_SOURCES DarkMatterVst3.cpp)
if(WIN32)
    list(APPEND DM_VST3_SOURCES "${vst3sdk_SOURCE_DIR}/public.sdk/source/common/pluginview.cpp" "${PROJECT_SOURCE_DIR}/src/plugin/PanelWindow.cpp")
endif()
smtg_add_vst3plugin(HemDarkMatter PACKAGE_NAME "HEM Dark Matter" SOURCES_LIST ${DM_VST3_SOURCES})
set_target_properties(HemDarkMatter PROPERTIES CXX_STANDARD 23 CXX_STANDARD_REQUIRED ON)
target_include_directories(HemDarkMatter SYSTEM PRIVATE "${vst3sdk_SOURCE_DIR}")
target_compile_definitions(HemDarkMatter PRIVATE DM_VERSION_STR="${PROJECT_VERSION}")
target_link_libraries(HemDarkMatter PRIVATE sdk dm_plugin_core)
if(NOT WIN32 AND NOT APPLE)   # ModuleEntry / ModuleExit (Linux: developer test builds only)
    target_sources(HemDarkMatter PRIVATE "${vst3sdk_SOURCE_DIR}/public.sdk/source/main/linuxmain.cpp")
endif()
if(WIN32)
    target_link_libraries(HemDarkMatter PRIVATE dm_ui)
    # Faceplate art inside the bundle (copied after the build: the SDK's per-file resource rules don't cope
    # with a space in the bundle name under MSBuild).
    get_target_property(DM_PKG HemDarkMatter SMTG_PLUGIN_PACKAGE_PATH)
    add_custom_command(TARGET HemDarkMatter POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${DM_PKG}/Contents/Resources/fonts" "${DM_PKG}/Contents/Resources/faceplates"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${PROJECT_SOURCE_DIR}/assets/fonts/Barlow-Regular.ttf" "${PROJECT_SOURCE_DIR}/assets/fonts/Barlow-Medium.ttf"
                "${PROJECT_SOURCE_DIR}/assets/fonts/Barlow-SemiBold.ttf" "${PROJECT_SOURCE_DIR}/assets/fonts/OFL.txt"
                "${DM_PKG}/Contents/Resources/fonts/"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${PROJECT_SOURCE_DIR}/assets/faceplates/Main-Faceplate.png"
                "${DM_PKG}/Contents/Resources/faceplates/"
        VERBATIM)
endif()
# Test host: loads the built bundle like a music program and checks sound, parameters, state, sidechain.
set(DM_HOSTING "${vst3sdk_SOURCE_DIR}/public.sdk/source")
set(DM_HOSTTEST_SOURCES "${PROJECT_SOURCE_DIR}/tools/vst3_hosttest/main.cpp"
    "${DM_HOSTING}/common/memorystream.cpp" "${DM_HOSTING}/vst/hosting/module.cpp"
    "${DM_HOSTING}/vst/hosting/plugprovider.cpp" "${DM_HOSTING}/vst/hosting/connectionproxy.cpp")
if(WIN32)
    list(APPEND DM_HOSTTEST_SOURCES "${DM_HOSTING}/vst/hosting/module_win32.cpp")
elseif(APPLE)
    list(APPEND DM_HOSTTEST_SOURCES "${DM_HOSTING}/vst/hosting/module_mac.mm")
else()
    list(APPEND DM_HOSTTEST_SOURCES "${DM_HOSTING}/vst/hosting/module_linux.cpp")
endif()
add_executable(dm_vst3_hosttest ${DM_HOSTTEST_SOURCES})
set_target_properties(dm_vst3_hosttest PROPERTIES CXX_STANDARD 23 CXX_STANDARD_REQUIRED ON)
target_include_directories(dm_vst3_hosttest SYSTEM PRIVATE "${vst3sdk_SOURCE_DIR}")
target_link_libraries(dm_vst3_hosttest PRIVATE sdk_hosting dm_plugin_core ${CMAKE_DL_LIBS})
add_dependencies(dm_vst3_hosttest HemDarkMatter)
get_target_property(DM_PKG_PATH HemDarkMatter SMTG_PLUGIN_PACKAGE_PATH)
add_test(NAME dm_vst3_hosttest COMMAND dm_vst3_hosttest "${DM_PKG_PATH}")
message(STATUS "[HEM Dark Matter] VST3 bundle enabled (SDK ${vst3sdk_SOURCE_DIR})")
