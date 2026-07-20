#include "hardware/HardwareProbe.h"

#include <windows.h>
#include <dxgi1_6.h>
#include <d3d11.h>
#include <wrl/client.h>
#include <intrin.h>

#include <algorithm>
#include <cstring>
#include <sstream>
#include <thread>

using Microsoft::WRL::ComPtr;

namespace {

std::string Trim(const std::string& s) {
    const size_t start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return std::string();
    const size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

// CPU brand string via extended CPUID leaves 0x80000002..0x80000004.
std::string CpuBrand() {
    int regs[4] = {0};
    __cpuid(regs, 0x80000000);
    const unsigned int maxExt = static_cast<unsigned int>(regs[0]);
    if (maxExt < 0x80000004) return "Unknown CPU";

    char brand[0x40] = {0};
    __cpuid(regs, 0x80000002); std::memcpy(brand + 0,  regs, sizeof(regs));
    __cpuid(regs, 0x80000003); std::memcpy(brand + 16, regs, sizeof(regs));
    __cpuid(regs, 0x80000004); std::memcpy(brand + 32, regs, sizeof(regs));
    return Trim(std::string(brand));
}

std::string WideToUtf8(const wchar_t* w) {
    if (!w) return std::string();
    const int len = WideCharToMultiByte(CP_UTF8, 0, w, -1, nullptr, 0, nullptr, nullptr);
    if (len <= 1) return std::string();
    std::string out(static_cast<size_t>(len - 1), '\0');
    WideCharToMultiByte(CP_UTF8, 0, w, -1, out.data(), len, nullptr, nullptr);
    return out;
}

std::string VendorName(UINT vendorId) {
    switch (vendorId) {
        case 0x10DE: return "NVIDIA";
        case 0x8086: return "Intel";
        case 0x1002: return "AMD";
        case 0x1414: return "Microsoft"; // WARP / software renderer
        default:     return "Other";
    }
}

// Query the D3D11 video device for supported hardware decoder profiles.
vms::DecodeCaps ProbeDecode(IDXGIAdapter1* adapter) {
    vms::DecodeCaps caps;

    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_11_0;

    // D3D_DRIVER_TYPE_UNKNOWN is required when an explicit adapter is supplied.
    const HRESULT hr = D3D11CreateDevice(
        adapter, D3D_DRIVER_TYPE_UNKNOWN, nullptr, 0,
        nullptr, 0, D3D11_SDK_VERSION,
        device.GetAddressOf(), &featureLevel, context.GetAddressOf());
    if (FAILED(hr)) return caps;

    ComPtr<ID3D11VideoDevice> videoDevice;
    if (FAILED(device.As(&videoDevice))) return caps;

    const UINT profileCount = videoDevice->GetVideoDecoderProfileCount();
    for (UINT i = 0; i < profileCount; ++i) {
        GUID guid = {};
        if (FAILED(videoDevice->GetVideoDecoderProfile(i, &guid))) continue;
        if (IsEqualGUID(guid, D3D11_DECODER_PROFILE_H264_VLD_NOFGT))   caps.h264 = true;
        else if (IsEqualGUID(guid, D3D11_DECODER_PROFILE_HEVC_VLD_MAIN))   caps.h265Main = true;
        else if (IsEqualGUID(guid, D3D11_DECODER_PROFILE_HEVC_VLD_MAIN10)) caps.h265Main10 = true;
    }
    return caps;
}

std::string JsonEscape(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 8);
    for (char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:   out += c;      break;
        }
    }
    return out;
}

const char* Bool(bool b) { return b ? "true" : "false"; }

} // namespace

namespace vms {

MachineProfile ProbeMachine() {
    MachineProfile p;

    p.cpuBrand = CpuBrand();

    p.logicalCores = std::thread::hardware_concurrency();
    if (p.logicalCores == 0) {
        SYSTEM_INFO si;
        GetSystemInfo(&si);
        p.logicalCores = si.dwNumberOfProcessors;
    }

    MEMORYSTATUSEX mem;
    mem.dwLength = sizeof(mem);
    if (GlobalMemoryStatusEx(&mem)) {
        p.totalRamBytes = mem.ullTotalPhys;
    }

    ComPtr<IDXGIFactory1> factory;
    if (SUCCEEDED(CreateDXGIFactory1(IID_PPV_ARGS(&factory)))) {
        ComPtr<IDXGIAdapter1> adapter;
        for (UINT i = 0;
             factory->EnumAdapters1(i, adapter.ReleaseAndGetAddressOf()) != DXGI_ERROR_NOT_FOUND;
             ++i) {
            DXGI_ADAPTER_DESC1 desc = {};
            if (FAILED(adapter->GetDesc1(&desc))) continue;

            GpuInfo g;
            g.description = WideToUtf8(desc.Description);
            g.vendor = VendorName(desc.VendorId);
            g.dedicatedVideoMemoryBytes = static_cast<std::uint64_t>(desc.DedicatedVideoMemory);
            g.isSoftwareAdapter = (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) != 0;
            if (!g.isSoftwareAdapter) {
                g.decode = ProbeDecode(adapter.Get());
            }
            p.gpus.push_back(std::move(g));
        }
    }

    // Tier heuristic (a starting point; refined once the decode spike gives real ceilings).
    bool hwHevc = false;
    bool hasDiscrete = false;
    for (const auto& g : p.gpus) {
        if (g.decode.h265Main || g.decode.h265Main10) hwHevc = true;
        const double vramGb = g.dedicatedVideoMemoryBytes / (1024.0 * 1024.0 * 1024.0);
        if (!g.isSoftwareAdapter && (g.vendor == "NVIDIA" || g.vendor == "AMD") && vramGb >= 2.0) {
            hasDiscrete = true;
        }
    }
    const double ramGb = p.totalRamBytes / (1024.0 * 1024.0 * 1024.0);

    if (hasDiscrete && hwHevc && ramGb >= 15.0 && p.logicalCores >= 8) {
        p.tier = "High";
    } else if (!hasDiscrete && (ramGb < 9.0 || !hwHevc)) {
        p.tier = "Low";
    } else {
        p.tier = "Typical";
    }

    return p;
}

std::string MachineProfile::toJson() const {
    std::ostringstream os;
    os << "{\n";
    os << "  \"cpuBrand\": \"" << JsonEscape(cpuBrand) << "\",\n";
    os << "  \"logicalCores\": " << logicalCores << ",\n";
    os << "  \"totalRamBytes\": " << totalRamBytes << ",\n";
    os << "  \"totalRamGB\": " << (totalRamBytes / (1024.0 * 1024.0 * 1024.0)) << ",\n";
    os << "  \"tier\": \"" << JsonEscape(tier) << "\",\n";
    os << "  \"gpus\": [";
    for (size_t i = 0; i < gpus.size(); ++i) {
        const GpuInfo& g = gpus[i];
        os << (i == 0 ? "\n" : ",\n");
        os << "    {\n";
        os << "      \"description\": \"" << JsonEscape(g.description) << "\",\n";
        os << "      \"vendor\": \"" << JsonEscape(g.vendor) << "\",\n";
        os << "      \"dedicatedVideoMemoryBytes\": " << g.dedicatedVideoMemoryBytes << ",\n";
        os << "      \"isSoftwareAdapter\": " << Bool(g.isSoftwareAdapter) << ",\n";
        os << "      \"decode\": { \"h264\": " << Bool(g.decode.h264)
           << ", \"h265Main\": " << Bool(g.decode.h265Main)
           << ", \"h265Main10\": " << Bool(g.decode.h265Main10) << " }\n";
        os << "    }";
    }
    os << (gpus.empty() ? "]\n" : "\n  ]\n");
    os << "}\n";
    return os.str();
}

} // namespace vms
