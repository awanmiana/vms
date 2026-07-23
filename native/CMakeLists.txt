# VMS Native — build configuration
#
# Requires: CMake >= 3.21, MSVC (Visual Studio Build Tools 2026), Windows SDK.
# For the Qt shell (increment 2+): Qt 6 (MSVC 2022 64-bit) and GStreamer 1.x (MSVC x86_64, devel).
#
# Configure example:
#   cmake -S . -B build -G Ninja ^
#     -DCMAKE_PREFIX_PATH="C:/Qt/6.11.1/msvc2022_64" ^
#     -DGSTREAMER_ROOT="C:/Program Files/gstreamer/1.0/msvc_x86_64"

cmake_minimum_required(VERSION 3.21)
project(vms_native LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ---------------------------------------------------------------------------
# Increment 1 — console hardware probe (no Qt / no GStreamer).
# ---------------------------------------------------------------------------
add_executable(vms_hwprobe
    src/main.cpp
    src/hardware/HardwareProbe.cpp
)
target_include_directories(vms_hwprobe PRIVATE src)

if(WIN32)
    target_compile_definitions(vms_hwprobe PRIVATE
        UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
    # dxgi: adapter enumeration; d3d11: decoder-capability probe;
    # dxguid: definitions of the D3D11_DECODER_PROFILE_* GUIDs.
    target_link_libraries(vms_hwprobe PRIVATE dxgi d3d11 dxguid)
endif()

# ---------------------------------------------------------------------------
# Increment 2 — Qt + GStreamer shell (linkage smoke test).
# Builds only when both Qt6 and a GStreamer dev tree are available.
# ---------------------------------------------------------------------------
set(GSTREAMER_ROOT "C:/Program Files/gstreamer/1.0/msvc_x86_64"
    CACHE PATH "GStreamer MSVC x86_64 root (contains include/, lib/, bin/)")

find_package(Qt6 COMPONENTS Core Gui Widgets QUIET)

if(Qt6_FOUND AND EXISTS "${GSTREAMER_ROOT}/include/gstreamer-1.0/gst/gst.h")
    message(STATUS "Building vms_shell with Qt6 ${Qt6_VERSION} and GStreamer at ${GSTREAMER_ROOT}")
    set(CMAKE_AUTOMOC ON)

    add_executable(vms_shell
        src/shell/main_shell.cpp
        src/hardware/HardwareProbe.cpp
    )
    target_include_directories(vms_shell PRIVATE
        src
        "${GSTREAMER_ROOT}/include/gstreamer-1.0"
        "${GSTREAMER_ROOT}/include/glib-2.0"
        "${GSTREAMER_ROOT}/lib/glib-2.0/include"
    )
    target_link_directories(vms_shell PRIVATE "${GSTREAMER_ROOT}/lib")
    target_link_libraries(vms_shell PRIVATE
        Qt6::Core Qt6::Gui Qt6::Widgets
        gstreamer-1.0 gobject-2.0 glib-2.0
    )
    if(WIN32)
        target_link_libraries(vms_shell PRIVATE dxgi d3d11 dxguid)
        target_compile_definitions(vms_shell PRIVATE
            UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
        # Keep the console subsystem so version output is visible during the smoke test.
        set_target_properties(vms_shell PROPERTIES WIN32_EXECUTABLE OFF)
    endif()
else()
    message(STATUS "Qt6 or GStreamer dev tree not found; skipping vms_shell (increment 2).")
endif()

# ---------------------------------------------------------------------------
# Increment 3 — RTSP decode + render spike (GStreamer only; no Qt).
# d3d11videosink creates its own window, so this needs no UI toolkit.
# ---------------------------------------------------------------------------
if(EXISTS "${GSTREAMER_ROOT}/include/gstreamer-1.0/gst/gst.h")
    add_executable(vms_spike src/spike/main_spike.cpp)
    target_include_directories(vms_spike PRIVATE
        "${GSTREAMER_ROOT}/include/gstreamer-1.0"
        "${GSTREAMER_ROOT}/include/glib-2.0"
        "${GSTREAMER_ROOT}/lib/glib-2.0/include"
    )
    target_link_directories(vms_spike PRIVATE "${GSTREAMER_ROOT}/lib")
    target_link_libraries(vms_spike PRIVATE gstreamer-1.0 gobject-2.0 glib-2.0)
    if(WIN32)
        target_compile_definitions(vms_spike PRIVATE
            UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
    endif()
endif()

# ---------------------------------------------------------------------------
# Increment 4 (spike half) — multi-tile grid decode/render measurement.
# N streams -> per-tile hardware decode -> d3d11compositor -> one d3d11 window.
# Measurement only (no governor policy); GStreamer d3d11 plugin required.
# ---------------------------------------------------------------------------
if(EXISTS "${GSTREAMER_ROOT}/include/gstreamer-1.0/gst/gst.h")
    add_executable(vms_grid src/grid/main_grid.cpp)
    target_include_directories(vms_grid PRIVATE
        "${GSTREAMER_ROOT}/include/gstreamer-1.0"
        "${GSTREAMER_ROOT}/include/glib-2.0"
        "${GSTREAMER_ROOT}/lib/glib-2.0/include"
    )
    target_link_directories(vms_grid PRIVATE "${GSTREAMER_ROOT}/lib")
    target_link_libraries(vms_grid PRIVATE gstreamer-1.0 gobject-2.0 glib-2.0)
    if(WIN32)
        target_compile_definitions(vms_grid PRIVATE
            UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
    endif()
endif()
