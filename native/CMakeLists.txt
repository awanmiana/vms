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
# Increment 4b — decode governor (pure decision core + self-check).
# No Qt / no GStreamer: portable C++ policy, always builds, self-verifying.
# ---------------------------------------------------------------------------
add_library(vms_governor STATIC src/governor/Governor.cpp)
target_include_directories(vms_governor PUBLIC src)
if(MSVC)
    target_compile_options(vms_governor PRIVATE /W4 /permissive-)
endif()

add_executable(vms_govtest src/governor/main_govtest.cpp)
target_link_libraries(vms_govtest PRIVATE vms_governor)
if(WIN32)
    target_compile_definitions(vms_govtest PRIVATE
        UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
endif()
enable_testing()
add_test(NAME governor_selfcheck COMMAND vms_govtest)

# ---------------------------------------------------------------------------
# Native increment 8 (P12-03/P0-01F) — optimization service. A live system-
# health probe feeds the governor's capacity so the wall degrades before the
# machine is exhausted; adds a bandwidth admission budget and an honest report.
# Pure decision core (Optimizer) + OS probe (SystemHealth, Windows impl) behind
# a swappable interface, self-verifying like the governor.
# ---------------------------------------------------------------------------
add_library(vms_optimize STATIC src/optimize/Optimizer.cpp)
target_include_directories(vms_optimize PUBLIC src)
target_link_libraries(vms_optimize PUBLIC vms_governor)
if(WIN32)
    target_sources(vms_optimize PRIVATE src/optimize/SystemHealth.cpp)
    target_compile_definitions(vms_optimize PRIVATE
        UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
endif()
if(MSVC)
    target_compile_options(vms_optimize PRIVATE /W4 /permissive-)
endif()

add_executable(vms_opttest src/optimize/main_opttest.cpp)
target_link_libraries(vms_opttest PRIVATE vms_optimize)
if(WIN32)
    target_compile_definitions(vms_opttest PRIVATE
        UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
endif()
add_test(NAME optimize_selfcheck COMMAND vms_opttest)

# ---------------------------------------------------------------------------
# Native increment 13 (P2-13) — device health & lifecycle. Pure decision core:
# honest per-dimension health -> derived state + exception lifecycle. No deps.
# ---------------------------------------------------------------------------
add_library(vms_health STATIC src/health/HealthMonitor.cpp)
target_include_directories(vms_health PUBLIC src)
if(MSVC)
    target_compile_options(vms_health PRIVATE /W4 /permissive-)
endif()

add_executable(vms_healthtest src/health/main_healthtest.cpp)
target_link_libraries(vms_healthtest PRIVATE vms_health)
if(WIN32)
    target_compile_definitions(vms_healthtest PRIVATE
        UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
endif()
add_test(NAME health_selfcheck COMMAND vms_healthtest)

# ---------------------------------------------------------------------------
# Native increment 11 (P2-03/P2-14) — ONVIF WS-Discovery. Standards-exact device
# discovery source for DeviceRepo: a pure Probe build + ProbeMatch parse (Qt6
# Core: XML/URL) and a UDP multicast transport (Qt6 Network). Self-verifying
# against real device fixtures; live LAN discovery needs devices on the network.
# ---------------------------------------------------------------------------
find_package(Qt6 COMPONENTS Core Network QUIET)
if(Qt6_FOUND AND TARGET Qt6::Core)
    message(STATUS "Building vms_onvif with Qt6 ${Qt6_VERSION}")
    add_library(vms_onvif STATIC
        src/onvif/WsDiscovery.cpp
        src/onvif/OnvifClient.cpp
        # inc 16: the injectable discovery seam (pure host-parse + a live source
        # over the transports, gated by VMS_ONVIF_TRANSPORT below).
        src/onvif/DiscoverySource.cpp
    )
    set_target_properties(vms_onvif PROPERTIES AUTOMOC OFF)
    target_include_directories(vms_onvif PUBLIC src)
    target_link_libraries(vms_onvif PUBLIC Qt6::Core)
    if(TARGET Qt6::Network)
        target_sources(vms_onvif PRIVATE
            src/onvif/WsDiscoveryTransport.cpp
            src/onvif/OnvifTransport.cpp)
        target_link_libraries(vms_onvif PUBLIC Qt6::Network)
        # MakeLiveDiscoverySource() returns a real source only when the network
        # transports are present; otherwise it honestly returns nullptr.
        target_compile_definitions(vms_onvif PUBLIC VMS_ONVIF_TRANSPORT)
    endif()
    if(WIN32)
        target_compile_definitions(vms_onvif PRIVATE
            UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
    endif()

    add_executable(vms_onviftest src/onvif/main_onviftest.cpp)
    set_target_properties(vms_onviftest PROPERTIES AUTOMOC OFF)
    target_link_libraries(vms_onviftest PRIVATE vms_onvif Qt6::Core)
    if(WIN32)
        target_compile_definitions(vms_onviftest PRIVATE
            UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
    endif()
    add_test(NAME onvif_selfcheck COMMAND vms_onviftest)
else()
    message(STATUS "Qt6 Core not found; skipping vms_onvif (increment 11).")
endif()

# ---------------------------------------------------------------------------
# Native increment 5 (P0-04) — persistence: an embedded local relational store
# (SQLite via the Windows-bundled winsqlite3) behind the C0-03 adapter contract.
# Qt-free and self-verifying, like the governor. On non-Windows a system SQLite
# backend would slot in behind the same Store interface (deferred).
# ---------------------------------------------------------------------------
if(WIN32)
    add_library(vms_persist STATIC
        src/persist/Store.cpp
        src/persist/Schema.cpp
        src/persist/WorkspaceRepo.cpp
        src/persist/SecretStore.cpp
        src/persist/CredentialRepo.cpp
        src/persist/SegmentIndex.cpp
        src/persist/DeviceRepo.cpp
    )
    target_include_directories(vms_persist PUBLIC src)
    # winsqlite3: the SQLite bundled with Windows (winsqlite/winsqlite3.h +
    # winsqlite3.lib from the Windows SDK). No download, no vendoring.
    # crypt32: DPAPI (CryptProtectData/CryptUnprotectData) for the secret store
    # (native increment 6a). The SecretStore itself is SQLite-free and swappable.
    target_link_libraries(vms_persist PUBLIC winsqlite3 crypt32)
    target_compile_definitions(vms_persist PRIVATE
        UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
    if(MSVC)
        target_compile_options(vms_persist PRIVATE /W4 /permissive-)
    endif()

    add_executable(vms_dbtest src/persist/main_dbtest.cpp)
    target_link_libraries(vms_dbtest PRIVATE vms_persist)
    add_test(NAME persist_selfcheck COMMAND vms_dbtest)

    # Native increment 6a — credential store (DPAPI) self-check.
    add_executable(vms_credtest src/persist/main_credtest.cpp)
    target_link_libraries(vms_credtest PRIVATE vms_persist)
    target_compile_definitions(vms_credtest PRIVATE
        UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
    add_test(NAME credential_selfcheck COMMAND vms_credtest)

    # Native increment 7a — recording segment-index self-check.
    add_executable(vms_rectest src/persist/main_rectest.cpp)
    target_link_libraries(vms_rectest PRIVATE vms_persist)
    target_compile_definitions(vms_rectest PRIVATE
        UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
    add_test(NAME recording_selfcheck COMMAND vms_rectest)

    # Native increment 6c — the standalone ConnectionBroker (single device-
    # connection identity). Pure C++: no Qt / no GStreamer, links only vms_persist
    # (Store + CredentialRepo + SecretStore), so it is unit-testable in isolation.
    add_library(vms_broker STATIC src/broker/ConnectionBroker.cpp)
    target_include_directories(vms_broker PUBLIC src)
    target_link_libraries(vms_broker PUBLIC vms_persist)
    target_compile_definitions(vms_broker PRIVATE
        UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
    if(MSVC)
        target_compile_options(vms_broker PRIVATE /W4 /permissive-)
    endif()

    add_executable(vms_brokertest src/broker/main_brokertest.cpp)
    target_link_libraries(vms_brokertest PRIVATE vms_broker)
    target_compile_definitions(vms_brokertest PRIVATE
        UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
    add_test(NAME broker_selfcheck COMMAND vms_brokertest)

    # Native increment 10 (P2-01/02/05) — device onboarding self-check. Links
    # vms_broker so the test can prove an onboarded device is broker-connectable.
    add_executable(vms_devtest src/persist/main_devtest.cpp)
    target_link_libraries(vms_devtest PRIVATE vms_broker)
    target_compile_definitions(vms_devtest PRIVATE
        UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
    add_test(NAME device_selfcheck COMMAND vms_devtest)
else()
    message(STATUS "Non-Windows: skipping vms_persist (winsqlite3 backend); a "
                   "system-SQLite backend behind Store is deferred.")
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
# P3-14 slice 1 — Qt Quick workspace: the governor's honest per-tile state,
# rendered. Depends only on Qt Quick + the pure vms_governor library (no
# GStreamer, no D3D), so it builds wherever Qt6 Quick is present and is
# verifiable headless via --selftest. Live video under the state chrome is a
# later slice.
# ---------------------------------------------------------------------------
find_package(Qt6 COMPONENTS Core Gui Qml Quick QUIET)

if(Qt6_FOUND AND TARGET Qt6::Quick)
    message(STATUS "Building vms_workspace with Qt6 Quick ${Qt6_VERSION}")

    add_executable(vms_workspace
        src/workspace/main_workspace.cpp
        src/workspace/WorkspaceController.h
        src/workspace/VideoItem.cpp
        src/workspace/VideoItem.h
        src/workspace/GridPipeline.cpp
        src/workspace/GridPipeline.h
        src/workspace/PlaybackPipeline.cpp
        src/workspace/PlaybackPipeline.h
        src/workspace/workspace.qrc
    )
    # --profile auto seeds the governor from the live hardware probe (Windows
    # D3D11/DXGI); the probe source is compiled in on Windows.
    if(WIN32)
        target_sources(vms_workspace PRIVATE src/hardware/HardwareProbe.cpp)
    endif()
    set_target_properties(vms_workspace PROPERTIES
        AUTOMOC ON
        AUTORCC ON)
    target_include_directories(vms_workspace PRIVATE src)
    target_link_libraries(vms_workspace PRIVATE
        vms_governor
        Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick
    )
    # Slice 2: live video under the chrome, via a GStreamer appsink. Compiled in
    # only when the GStreamer dev tree is present; without it the pure-Qt slice-1
    # workspace still builds and --video reports it is unavailable.
    if(EXISTS "${GSTREAMER_ROOT}/include/gstreamer-1.0/gst/gst.h")
        message(STATUS "vms_workspace: GStreamer found, enabling --video (slice 2)")
        target_include_directories(vms_workspace PRIVATE
            "${GSTREAMER_ROOT}/include/gstreamer-1.0"
            "${GSTREAMER_ROOT}/include/glib-2.0"
            "${GSTREAMER_ROOT}/lib/glib-2.0/include"
        )
        target_link_directories(vms_workspace PRIVATE "${GSTREAMER_ROOT}/lib")
        target_link_libraries(vms_workspace PRIVATE
            gstreamer-1.0 gstapp-1.0 gobject-2.0 glib-2.0)
        target_compile_definitions(vms_workspace PRIVATE VMS_WITH_GSTREAMER)
    endif()
    if(WIN32)
        # dxgi/d3d11/dxguid: the HardwareProbe used by --profile auto.
        target_link_libraries(vms_workspace PRIVATE dxgi d3d11 dxguid)
        # P0-04 inc 5c: persist and restore the workspace layout (embedded SQLite).
        target_link_libraries(vms_workspace PRIVATE vms_persist)
        # inc 7c: the Playback availability model over the recording SegmentIndex
        # (needs vms_persist, so Windows-only alongside it).
        target_sources(vms_workspace PRIVATE src/workspace/PlaybackController.cpp)
        # inc 14 (onboarding UI): the Devices tab's DeviceController bridges the
        # DeviceRepo (vms_persist) + HealthMonitor (vms_health) cores to QML.
        target_sources(vms_workspace PRIVATE
            src/workspace/DeviceController.cpp
            src/workspace/DeviceController.h)
        target_link_libraries(vms_workspace PRIVATE vms_health)
        target_compile_definitions(vms_workspace PRIVATE VMS_WITH_PERSIST)
        # inc 16 (discovery -> onboard UI): the Devices tab drives the ONVIF
        # WS-Discovery + media SOAP cores through DeviceController's injectable
        # DiscoverySource. Only wired when vms_onvif was built (Qt6 Core).
        if(TARGET vms_onvif)
            target_link_libraries(vms_workspace PRIVATE vms_onvif)
            target_compile_definitions(vms_workspace PRIVATE VMS_WITH_ONVIF)
        endif()
        # inc 8: the live optimization sampler feeding the governor.
        target_link_libraries(vms_workspace PRIVATE vms_optimize)
        target_compile_definitions(vms_workspace PRIVATE VMS_WITH_OPTIMIZE)
        target_compile_definitions(vms_workspace PRIVATE
            UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
        # Keep the console subsystem so --selftest output is visible.
        set_target_properties(vms_workspace PROPERTIES WIN32_EXECUTABLE OFF)
    endif()
else()
    message(STATUS "Qt6 Quick not found; skipping vms_workspace (P3-14 slice 1).")
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
# Increment 4 — multi-tile grid decode/render measurement + governor harness.
# N streams -> per-tile hardware decode -> d3d11compositor -> one d3d11 window.
# --govern adds the approved 4b tier plan; GStreamer d3d11 plugin required.
# ---------------------------------------------------------------------------
if(EXISTS "${GSTREAMER_ROOT}/include/gstreamer-1.0/gst/gst.h")
    add_executable(vms_grid
        src/grid/main_grid.cpp
    )
    if(WIN32)
        target_sources(vms_grid PRIVATE src/hardware/HardwareProbe.cpp)
    endif()
    target_include_directories(vms_grid PRIVATE
        src
        "${GSTREAMER_ROOT}/include/gstreamer-1.0"
        "${GSTREAMER_ROOT}/include/glib-2.0"
        "${GSTREAMER_ROOT}/lib/glib-2.0/include"
    )
    target_link_directories(vms_grid PRIVATE "${GSTREAMER_ROOT}/lib")
    target_link_libraries(vms_grid PRIVATE
        vms_governor
        gstreamer-1.0 gobject-2.0 glib-2.0
    )
    if(WIN32)
        target_link_libraries(vms_grid PRIVATE dxgi d3d11 dxguid)
        # Native increment 6c-2: connect through the standalone ConnectionBroker by
        # camera_id (no URL-with-password on the run path). vms_broker is Windows-only
        # (it links vms_persist -> winsqlite3/crypt32).
        target_link_libraries(vms_grid PRIVATE vms_broker)
        target_compile_definitions(vms_grid PRIVATE VMS_WITH_BROKER)
        target_compile_definitions(vms_grid PRIVATE
            UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
    endif()
endif()

# ---------------------------------------------------------------------------
# Native increment 7a-media — recording pipeline. splitmuxsink writes segment
# files and every completed segment is written into the SQLite SegmentIndex.
# Needs GStreamer (pipeline) + vms_persist (index), so Windows-only for now.
# ---------------------------------------------------------------------------
if(WIN32 AND EXISTS "${GSTREAMER_ROOT}/include/gstreamer-1.0/gst/gst.h")
    add_executable(vms_record src/record/main_record.cpp)
    target_include_directories(vms_record PRIVATE
        src
        "${GSTREAMER_ROOT}/include/gstreamer-1.0"
        "${GSTREAMER_ROOT}/include/glib-2.0"
        "${GSTREAMER_ROOT}/lib/glib-2.0/include"
    )
    target_link_directories(vms_record PRIVATE "${GSTREAMER_ROOT}/lib")
    target_link_libraries(vms_record PRIVATE
        vms_persist gstreamer-1.0 gobject-2.0 glib-2.0)
    target_compile_definitions(vms_record PRIVATE
        UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
endif()
