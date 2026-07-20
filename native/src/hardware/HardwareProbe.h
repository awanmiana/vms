#pragma once

#include <string>
#include <vector>
#include <cstdint>

namespace vms {

// Per-codec hardware-decode support for one GPU adapter, as reported by the
// Direct3D 11 video device. This is the honest answer to "can THIS machine
// hardware-decode this codec?" — the core input to per-machine tuning.
struct DecodeCaps {
    bool h264 = false;
    bool h265Main = false;   // HEVC 8-bit
    bool h265Main10 = false; // HEVC 10-bit
};

struct GpuInfo {
    std::string description;
    std::string vendor; // NVIDIA / Intel / AMD / Microsoft / Other
    std::uint64_t dedicatedVideoMemoryBytes = 0;
    bool isSoftwareAdapter = false;
    DecodeCaps decode;
};

struct MachineProfile {
    std::string cpuBrand;
    unsigned int logicalCores = 0;
    std::uint64_t totalRamBytes = 0;
    std::vector<GpuInfo> gpus;
    std::string tier; // Low / Typical / High

    // Dependency-free JSON serialization (no external library).
    std::string toJson() const;
};

// Probes the current machine. Safe to call once at startup.
MachineProfile ProbeMachine();

} // namespace vms
