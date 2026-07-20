# VMS Native — build configuration
#
# Increment 1: console hardware probe only (no Qt / no GStreamer yet).
# Requires: CMake >= 3.21, MSVC (Visual Studio Build Tools 2026), Windows SDK.
# Qt and GStreamer are introduced in later increments.

cmake_minimum_required(VERSION 3.21)
project(vms_native LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(vms_hwprobe
    src/main.cpp
    src/hardware/HardwareProbe.cpp
)

target_include_directories(vms_hwprobe PRIVATE src)

if(WIN32)
    target_compile_definitions(vms_hwprobe PRIVATE
        UNICODE _UNICODE
        WIN32_LEAN_AND_MEAN NOMINMAX
    )
    # dxgi: adapter enumeration; d3d11: video-decoder capability probe;
    # dxguid: definitions of the D3D11_DECODER_PROFILE_* GUIDs.
    target_link_libraries(vms_hwprobe PRIVATE dxgi d3d11 dxguid)
endif()
