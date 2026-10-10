cmake_minimum_required(VERSION 3.24)
project(GameHD2DEnhancer VERSION 0.1.1 LANGUAGES CXX RC)
if(NOT WIN32 OR NOT MSVC)
  message(FATAL_ERROR "Build with MSVC on Windows (Visual Studio 2022+ and Windows SDK)")
endif()
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
set(NIS_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/third_party/NVIDIAGameWorks__NVIDIAImageScaling/35e13ba316c98eeecf16f37eae70ce88019911f6")
if(NOT EXISTS "${NIS_ROOT}/NIS/NIS_Config.h")
  message(FATAL_ERROR "Pinned NIS source is missing. Restore the archived source snapshot first.")
endif()
add_library(hd2d_gpu STATIC app/gpu.cpp)
target_include_directories(hd2d_gpu PUBLIC app PRIVATE "${NIS_ROOT}/NIS")
target_compile_definitions(hd2d_gpu PUBLIC UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN _WIN32_WINNT=0x0A00)
target_compile_options(hd2d_gpu PUBLIC /utf-8 /W4 /permissive- /EHsc /Zc:__cplusplus)
target_link_libraries(hd2d_gpu PUBLIC d3d11 dxgi d3dcompiler windowsapp ole32 runtimeobject)
add_executable(Game-HD2D-Enhancer WIN32 app/main.cpp app/self_test.cpp app/app.rc)
target_link_libraries(Game-HD2D-Enhancer PRIVATE hd2d_gpu user32 gdi32 comctl32 dwmapi shell32)
target_link_options(Game-HD2D-Enhancer PRIVATE /MANIFEST:NO)
add_executable(HD2D-Tests app/self_test.cpp)
target_compile_definitions(HD2D-Tests PRIVATE HD2D_TEST_EXECUTABLE)
target_link_libraries(HD2D-Tests PRIVATE hd2d_gpu user32)
foreach(target Game-HD2D-Enhancer HD2D-Tests)
  add_custom_command(TARGET ${target} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${target}>/shaders/NIS"
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${target}>/licenses"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${NIS_ROOT}/NIS/NIS_Main.hlsl" "$<TARGET_FILE_DIR:${target}>/shaders/NIS/NIS_Main.hlsl"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${NIS_ROOT}/NIS/NIS_Scaler.h" "$<TARGET_FILE_DIR:${target}>/shaders/NIS/NIS_Scaler.h"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${NIS_ROOT}/README.md" "$<TARGET_FILE_DIR:${target}>/licenses/NVIDIAImageScaling.txt"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/docs/RUN_V0_1_1.txt" "$<TARGET_FILE_DIR:${target}>/使用说明.txt"
    VERBATIM)
endforeach()
enable_testing()
add_test(NAME Geometry_and_D3D11_WARP_NIS COMMAND HD2D-Tests)
set_tests_properties(Geometry_and_D3D11_WARP_NIS PROPERTIES TIMEOUT 180)
